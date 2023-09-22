#include "bot.hpp"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

Bot::Bot(): _bot(findToken()), _db("store.db") {
	signal(SIGINT, [](int s) {
		printf("SIGINT got\n");
		exit(0);
	});

	auto max_size = 10 * 1024 * 1024;
	auto max_files = 10;
	auto logger = spdlog::rotating_logger_mt("general_logger", "logs/log_rot.txt", max_size, max_files);
	spdlog::set_default_logger(logger);
}

void Bot::addOnCommand(const std::string& name, Command& command) {
	_bot.getEvents().onCommand(name, [name, &command, this](TgBot::Message::Ptr msg) {
		auto& uc = getOrCreateUserContext(msg->from->id, msg->from->username, msg->chat->id);

		try {
			command.onCommand(uc, name);
		} catch (...) {
			uc.stack.clear();
			throw;
		}
	});
}

UserContext& Bot::getOrCreateUserContext(int64_t id, const std::string& name, int64_t chatId) {
	if (auto found = _userContexts.find(id); found != _userContexts.end()) {
		spdlog::debug("UserInfo({}, {}, {}) found in cache.", id, chatId, name);

		return found->second;
	} else {
		spdlog::debug("UserInfo({}, {}, {}) not found in cache.", id, chatId, name);

		auto user = _db.loadUser(id);
		if (!user) {
			spdlog::debug("UserInfo({}, {}, {}) not found in db.", id, chatId, name);
			spdlog::debug("Create UserInfo({}, {}, {}) in db.", id, chatId, name);
			user = UserInfo{id, chatId, name};
			_db.storeUser(*user);
			_db.db().commit();
		}
		_userContexts[user->id].userInfo = std::make_shared<UserInfo>(*user);

		return _userContexts[user->id];
	}
}

void Bot::run() {
	using namespace TgBot;

	_bot.getEvents().onNonCommandMessage([this](Message::Ptr msg) {
		auto& uc = getOrCreateUserContext(msg->from->id, msg->from->username, msg->chat->id);

		if (uc.stack.empty()) {
			return;
		}
		auto cmd = uc.stack.back().command;

		try {
			cmd->onNonCommand(uc, msg->text);
		} catch (...) {
			uc.stack.clear();
			throw;
		}
	});

	_bot.getEvents().onCallbackQuery([this](CallbackQuery::Ptr query) {
		if (!query->message && !query->data.empty()) {
			return;
		}

		auto& uc = getOrCreateUserContext(query->from->id, query->from->username, query->message->chat->id);

		if (query->data.front() == '/') {
			auto cmd = query->data.substr(1);
			if (cmd == "/back" && !uc.stack.empty()) {
				uc.stack.back().command->onCommand(uc, cmd);
			} else if (auto found = _commands.find(cmd); found != _commands.end()) {
				found->second->onCommand(uc, cmd);
			}
			return;
		}

		if (uc.stack.empty()) {
			return;
		}
		auto cmd = uc.stack.back().command;
		try {
			cmd->onQuery(uc, query->data);
		} catch (...) {
			uc.stack.clear();
			throw;
		}
	});

	printf("Bot username: %s\n", _bot.getApi().getMe()->username.c_str());
	_bot.getApi().deleteWebhook();

	TgLongPoll longPoll(_bot);
	while (true) {
		try {
			printf("Long poll started\n");
			longPoll.start();
		} catch (std::exception& e) { printf("error: %s\n", e.what()); }
	}
}
