#include <boost/algorithm/string/split.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <tgbot/Bot.h>
#include <tgbot/net/CurlHttpClient.h>
#include <tgbot/net/TgLongPoll.h>
#include <unqlite_cpp/unqlite_cpp.hpp>

#include <curl/curl.h>

#include <algorithm>
#include <csignal>
#include <iostream>
#include <set>

constexpr size_t MAX_MESSAGE_SIZE = 4096;

inline std::string findToken() {
	std::string token;
	std::ifstream tokenFile("token");
	if (!tokenFile.is_open()) {
		throw std::runtime_error("Can't find token");
	}
	tokenFile >> token;

	return token;
}

bool eraseIggid(up::db& db, const std::string& id) {
	auto vm = db.compile_or_throw(R"(
if(db_exists('iggid')){
  $records = db_fetch_all('iggid', function($rec){
    if( $rec.id == $id ){
        return TRUE;
    }
    return FALSE;
  });

  $result = count($records) > 0;

  foreach($records as $rec) {
    db_drop_record('iggid', $rec.__id);
  }
}
)");
	vm.bind_or_throw("id", id);
	vm.exec_or_throw();
	db.commit_or_throw();

	return vm.extract_or_throw("result").get_bool_or_throw();
}

bool eraseSubAdmin(up::db& db, int64_t id) {
	auto vm = db.compile_or_throw(R"(
if(db_exists('sub_admins')){
  $records = db_fetch_all('sub_admins', function($rec){
    if( $rec.id == $id ){
        return TRUE;
    }
    return FALSE;
  });

  $result = count($records) > 0;

  foreach($records as $rec) {
    db_drop_record('sub_admins', $rec.__id);
  }
}
)");
	vm.bind_or_throw("id", id);
	vm.exec_or_throw();
	db.commit_or_throw();

	return vm.extract_or_throw("result").get_bool_or_throw();
}

const std::set<int64_t> ADMINS = {363372858 /*@Agate_GRim*/, 374655909 /*@biscottti*/};

std::set<int64_t> SUB_ADMINS = {};
void addSubAdmin(up::db& db, int64_t id) {
	try {
		up::vm_store_record(db).store("sub_admins", up::value::object{{"id", id}});
	} catch (...) {}
	SUB_ADMINS.insert(id);
}
void delSubAdmin(up::db& db, int64_t id) {
	eraseSubAdmin(db, id);
	SUB_ADMINS.erase(id);
}
void loadSubAdmins(up::db& db) {
	try {
		up::vm_fetch_all_records(db)
		    .fetch_or_throw("sub_admins")
		    .make_value()
		    .foreach_if_array([&](auto i, const up::value& v) {
			    SUB_ADMINS.insert(v.at("id").get_int_or_throw());
			    return true;
		    });
	} catch (...) {}
}

size_t readStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
};

int main(int, char**) {
	signal(SIGINT, [](int s) {
		printf("SIGINT got\n");
		exit(0);
	});

	auto logCmd = spdlog::rotating_logger_st("log_cmd", "log_cmd.log", 50 * 1024 * 1024, 20);
	// logCmd->set_level(spdlog::level::info);
	logCmd->flush_on(spdlog::level::info);
	auto logMsg = [&](const TgBot::Message::Ptr& msg) {
		logCmd->info("'{}'({} {}, id: {}): {}", msg->from->username, msg->from->firstName, msg->from->lastName,
		    msg->from->id, msg->text);
	};
	auto logMsgText = [&](const TgBot::Message::Ptr& msg, std::string_view text) {
		logCmd->info("'{}'({} {}, id: {}): {}: {}", msg->from->username, msg->from->firstName, msg->from->lastName,
		    msg->from->id, msg->text, text);
	};

	using namespace TgBot;

	CurlHttpClient curlHttpClient;
	Bot bot(findToken(), curlHttpClient);
	bot.getApi().deleteWebhook();

	up::db db("db.bin");

	loadSubAdmins(db);

	bot.getEvents().onCommand("admin_del", [&](TgBot::Message::Ptr msg) {
		try {
			if (!msg->chat) {
				std::cerr << "Невозможно переслать сообщение" << std::endl;
				return;
			}

			if (!msg->from) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Несуществующий пользователь!");
				std::cerr << "Несуществующий пользователь" << std::endl;
				return;
			}

			logMsg(msg);

			auto userId = msg->from->id;
			if (ADMINS.count(userId) == 0) {
				auto m = fmt::format("⚠️ У вас({}({} {}): {}) нет доступа к этой команде!", msg->from->username,
				    msg->from->firstName, msg->from->lastName, msg->from->id);

				bot.getApi().sendMessage(msg->chat->id, m);
				logMsgText(msg, m);

				return;
			}

			std::vector<std::string> args;
			boost::split(args, msg->text, [](char c) { return c == ' ' || c == '\n' || c == '\t'; });

			if (args.size() < 2) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Неверное число аргументов!");
				return;
			}
			args.erase(args.begin());

			addSubAdmin(db, std::stoll(args.front()));

			bot.getApi().sendMessage(msg->chat->id, fmt::format("✅ Добавлен администратор, id: {}", args.front()));
		} catch (const std::exception& e) {
			logMsgText(msg, e.what());
			bot.getApi().sendMessage(msg->chat->id, fmt::format("⚠️ {}", e.what()));
		}
	});
	bot.getEvents().onCommand("admin_add", [&](TgBot::Message::Ptr msg) {
		try {
			if (!msg->chat) {
				std::cerr << "Невозможно переслать сообщение" << std::endl;
				return;
			}

			if (!msg->from) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Несуществующий пользователь!");
				std::cerr << "Несуществующий пользователь" << std::endl;
				return;
			}
			logMsg(msg);

			auto userId = msg->from->id;
			if (ADMINS.count(userId) == 0) {
				auto m = fmt::format("⚠️ У вас({}({} {}): {}) нет доступа к этой команде!", msg->from->username,
				    msg->from->firstName, msg->from->lastName, msg->from->id);

				bot.getApi().sendMessage(msg->chat->id, m);
				logMsgText(msg, m);

				return;
			}

			std::vector<std::string> args;
			boost::split(args, msg->text, [](char c) { return c == ' ' || c == '\n' || c == '\t'; });

			if (args.size() < 2) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Неверное число аргументов!");
				return;
			}
			args.erase(args.begin());

			delSubAdmin(db, std::stoll(args.front()));
			bot.getApi().sendMessage(msg->chat->id, fmt::format("❌ Администратор удален, id: {}", args.front()));
		} catch (const std::exception& e) {
			logMsgText(msg, e.what());
			bot.getApi().sendMessage(msg->chat->id, fmt::format("⚠️ {}", e.what()));
		}
	});
	bot.getEvents().onCommand("admin_list", [&](TgBot::Message::Ptr msg) {
		try {
			if (!msg->chat) {
				std::cerr << "Невозможно переслать сообщение" << std::endl;
				return;
			}

			if (!msg->from) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Несуществующий пользователь!");
				std::cerr << "Несуществующий пользователь" << std::endl;
				return;
			}
			logMsg(msg);

			auto userId = msg->from->id;
			if (ADMINS.count(userId) == 0 && SUB_ADMINS.count(userId) == 0) {
				auto m = fmt::format("⚠️ У вас({}({} {}): {}) нет доступа к этой команде!", msg->from->username,
				    msg->from->firstName, msg->from->lastName, msg->from->id);

				bot.getApi().sendMessage(msg->chat->id, m);
				logMsgText(msg, m);

				return;
			}

			std::string users = "Список админов:\n";
			for (auto a : SUB_ADMINS) {
				users += fmt::format("{}\n", a);
			}

			if (users.size() >= MAX_MESSAGE_SIZE) {
				auto file = std::make_shared<InputFile>();
				file->data = users;
				file->fileName = "users.txt";
				file->mimeType = "text/plain";

				bot.getApi().sendDocument(msg->chat->id, file);
			} else {
				bot.getApi().sendMessage(msg->chat->id, users);
			}
		} catch (const std::exception& e) {
			logMsgText(msg, e.what());
			bot.getApi().sendMessage(msg->chat->id, fmt::format("⚠️ {}", e.what()));
		}
	});

	bot.getEvents().onCommand("reg", [&](TgBot::Message::Ptr msg) {
		try {
			if (!msg->chat) {
				std::cerr << "Невозможно переслать сообщение" << std::endl;
				return;
			}

			if (!msg->from) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Несуществующий пользователь!");
				std::cerr << "Несуществующий пользователь" << std::endl;
				return;
			}
			logMsg(msg);

			auto userId = msg->from->id;
			if (ADMINS.count(userId) == 0 && SUB_ADMINS.count(userId) == 0) {
				auto m = fmt::format("⚠️ У вас({}({} {}): {}) нет доступа к этой команде!", msg->from->username,
				    msg->from->firstName, msg->from->lastName, msg->from->id);

				bot.getApi().sendMessage(msg->chat->id, m);
				logMsgText(msg, m);

				return;
			}

			std::vector<std::string> args;
			boost::split(args, msg->text, [](char c) { return c == ' ' || c == '\n' || c == '\t'; });

			if (args.size() < 2) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Неверное число аргументов!");
				return;
			}
			args.erase(args.begin());

			std::set<std::string> users;
			auto usersV = up::vm_fetch_all_records(db).fetch_or_throw("iggid").make_value();
			usersV.foreach_if_array([&users](std::size_t, const up::value& v) {
				users.insert(v.at("id").get_string_or_throw());
				return true;
			});

			std::string errorUsers = "⚠️ Пользователи уже существуют:\n";
			size_t usersAdded = 0;
			size_t usersTotal = args.size();
			for (const auto& iggid : args) {
				try {
					if (users.count(iggid) != 0) {
						throw std::runtime_error("User already created!");
					}
					up::vm_store_record(db).store_or_throw("iggid", up::value::object{{"id", iggid}});
					usersAdded++;
				} catch (const std::exception&) { errorUsers += iggid + "\n"; }
			}

			if (usersAdded != 0) {
				if (usersAdded == usersTotal) {
					bot.getApi().sendMessage(msg->chat->id,
					    fmt::format("✅ Пользователи добавленны: {}/{}", usersAdded, usersTotal));
				} else {
					bot.getApi().sendMessage(msg->chat->id,
					    fmt::format("✅ Пользователи добавленны: {}/{}\n{}", usersAdded, usersTotal, errorUsers));
				}
			} else {
				bot.getApi().sendMessage(msg->chat->id, fmt::format("⚠️ Пользователи существуют."));
			}
		} catch (const std::exception& e) { logMsgText(msg, e.what()); }
	});
	bot.getEvents().onCommand("del", [&](TgBot::Message::Ptr msg) {
		try {
			if (!msg->chat) {
				std::cerr << "Невозможно переслать сообщение" << std::endl;
				return;
			}

			if (!msg->from) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Несуществующий пользователь!");
				std::cerr << "Несуществующий пользователь" << std::endl;
				return;
			}
			logMsg(msg);

			auto userId = msg->from->id;
			if (ADMINS.count(userId) == 0) {
				auto m = fmt::format("⚠️ У вас({}({} {}): {}) нет доступа к этой команде!", msg->from->username,
				    msg->from->firstName, msg->from->lastName, msg->from->id);

				bot.getApi().sendMessage(msg->chat->id, m);
				logMsgText(msg, m);

				return;
			}

			std::vector<std::string> args;
			boost::split(args, msg->text, [](char c) { return c == ' ' || c == '\n' || c == '\t'; });

			if (args.size() != 2) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Неверное число аргументов!");
				return;
			}
			const auto& iggid = args[1];

			if (eraseIggid(db, iggid)) {
				bot.getApi().sendMessage(msg->chat->id, "✅ Пользователь удален.");
			} else {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Пользователя не существует.");
			}
		} catch (const std::exception& e) { logMsgText(msg, e.what()); }
	});
	bot.getEvents().onCommand("list", [&](TgBot::Message::Ptr msg) {
		try {
			if (!msg->chat) {
				std::cerr << "Невозможно переслать сообщение" << std::endl;
				return;
			}

			if (!msg->from) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Несуществующий пользователь!");
				std::cerr << "Несуществующий пользователь" << std::endl;
				return;
			}
			logMsg(msg);

			auto userId = msg->from->id;
			if (ADMINS.count(userId) == 0 && SUB_ADMINS.count(userId) == 0) {
				auto m = fmt::format("⚠️ У вас({}({} {}): {}) нет доступа к этой команде!", msg->from->username,
				    msg->from->firstName, msg->from->lastName, msg->from->id);

				bot.getApi().sendMessage(msg->chat->id, m);
				logMsgText(msg, m);

				return;
			}

			auto values = up::vm_fetch_all_records(db).fetch_or_throw("iggid").make_value();

			std::string users;
			values.foreach_if_array([&users](std::size_t, const up::value& v) {
				users += v.at("id").get_string_or_throw() + "\n";
				return true;
			});

			if (users.size() >= MAX_MESSAGE_SIZE) {
				auto file = std::make_shared<InputFile>();
				file->data = users;
				file->fileName = "users.txt";
				file->mimeType = "text/plain";

				bot.getApi().sendDocument(msg->chat->id, file);
			} else {
				bot.getApi().sendMessage(msg->chat->id, users);
			}
		} catch (const std::exception& e) { logMsgText(msg, e.what()); }
	});

	bot.getEvents().onCommand("code", [&](TgBot::Message::Ptr msg) {
		try {
			if (!msg->chat) {
				std::cerr << "Невозможно переслать сообщение" << std::endl;
				return;
			}

			if (!msg->from) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Несуществующий пользователь!");
				std::cerr << "Несуществующий пользователь" << std::endl;
				return;
			}
			logMsg(msg);

			std::vector<std::string> args;
			boost::split(args, msg->text, [](char c) { return c == ' ' || c == '\n' || c == '\t'; });

			if (args.size() != 2) {
				bot.getApi().sendMessage(msg->chat->id, "⚠️ Неверное число аргументов!");
				return;
			}

			const auto& code = args[1];

			struct Stats {
				size_t totalUsers = 0;
				size_t totalActivates = 0;
				std::set<std::string> errorMsgs;
			} stats;

			up::value value = up::vm_fetch_all_records(db).fetch_value_or_throw("iggid");

			bot.getApi().sendMessage(msg->chat->id,
			    fmt::format("⏳ Старт активации", stats.totalActivates, stats.totalUsers));

			value.foreach_if_array([&](int64_t i, const up::value& v) {
				auto id = v.at("id").get_string_or_throw();
				using StrPair = std::pair<std::string, std::string>;
				StrPair userData{id, code};

				CURL* handle = curl_easy_init();
				if (!handle) {
					return true;
				}

				auto data = fmt::format("iggid={}&cdkey={}&username=&sign=0", id, code);

				curl_easy_setopt(handle, CURLOPT_POST, 1);
				curl_easy_setopt(handle, CURLOPT_URL, "https://dut.igg.com/event/code?lang=rus");
				curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data.c_str());

				std::string readBuffer;
				curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &readStringCallback);
				curl_easy_setopt(handle, CURLOPT_WRITEDATA, &readBuffer);

				CURLcode res = curl_easy_perform(handle);
				curl_easy_cleanup(handle);

				if (res != CURLE_OK) {
					return true;
				}
				auto msg = nlohmann::json::parse(readBuffer);
				int rc = msg.at("code");
				if (rc != -1) {
					stats.totalActivates++;
				} else {
					stats.errorMsgs.insert(msg.at("msg"));
				}

				return true;
			});
			stats.totalUsers = value.size();
			std::string uniqueErrors;
			for (const auto& e : stats.errorMsgs) {
				uniqueErrors += e;
				uniqueErrors += "\n";
			}

			bot.getApi().sendMessage(msg->chat->id,
			    fmt::format("{} Статистика активации: {}/{}. \n Ошибки:\n{}", stats.errorMsgs.empty() ? "✅" : "⚠️",
			        stats.totalActivates, stats.totalUsers, uniqueErrors));
		} catch (const std::exception& e) { logMsgText(msg, e.what()); }
	});

	bot.getEvents().onAnyMessage([&](TgBot::Message::Ptr msg) {
		if (!msg->chat) {
			std::cerr << "Невозможно переслать сообщение" << std::endl;
			return;
		}

		if (!msg->from) {
			std::cerr << "Несуществующий пользователь" << std::endl;
			return;
		}
		logMsg(msg);
	});

	std::vector<BotCommand::Ptr> commands;
	BotCommand::Ptr cmdArray;

	cmdArray = BotCommand::Ptr(new BotCommand);
	cmdArray->command = "code";
	cmdArray->description = "Активация кода. Все.(/code ABCDEFG)";
	commands.push_back(cmdArray);

	cmdArray = BotCommand::Ptr(new BotCommand);
	cmdArray->command = "reg";
	cmdArray->description = "Регистрация нового iggid(игрока). Админы.(/reg 12345678 23434566)";
	commands.push_back(cmdArray);

	cmdArray = BotCommand::Ptr(new BotCommand);
	cmdArray->command = "del";
	cmdArray->description = "Удаление пользователя. Верховный админ.(/del 12345678)";
	commands.push_back(cmdArray);

	cmdArray = BotCommand::Ptr(new BotCommand);
	cmdArray->command = "list";
	cmdArray->description = "Вывести все iggid. Админы.(/list)";
	commands.push_back(cmdArray);

	cmdArray = BotCommand::Ptr(new BotCommand);
	cmdArray->command = "admin_add";
	cmdArray->description = "Добавить админа. Верховный админ.(/admin_add 123234)";
	commands.push_back(cmdArray);

	cmdArray = BotCommand::Ptr(new BotCommand);
	cmdArray->command = "admin_del";
	cmdArray->description = "Удалить админа. Верховный админ.(/admin_del 123234)";
	commands.push_back(cmdArray);

	cmdArray = BotCommand::Ptr(new BotCommand);
	cmdArray->command = "admin_list";
	cmdArray->description = "Список админов. Админы.(/admin_list)";
	commands.push_back(cmdArray);

	bot.getApi().setMyCommands(commands);

	TgLongPoll longPoll(bot);
	printf("Start bot.\n");
	while (true) {
		try {
			longPoll.start();
		} catch (const std::exception& e) { printf("error: %s\n", e.what()); }
	}
}
