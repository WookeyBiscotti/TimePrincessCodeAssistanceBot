#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <tgbot/Bot.h>
#include <tgbot/net/TgLongPoll.h>
#include <unqlite_cpp/unqlite_cpp.hpp>

#include <curl/curl.h>

#include <algorithm>
#include <csignal>
#include <iostream>
#include <set>

inline std::string findToken() {
	std::string token;
	std::ifstream tokenFile("token");
	if (!tokenFile.is_open()) {
		throw std::runtime_error("Can't find token");
	}
	tokenFile >> token;

	return token;
}

const std::set<int64_t> ADMINS = {363372858 /*@Agate_GRim*/, 374655909 /*@biscottti*/};

size_t readStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
};

int main(int, char**) {
	signal(SIGINT, [](int s) {
		printf("SIGINT got\n");
		exit(0);
	});

	using namespace TgBot;

	Bot bot(findToken());
	bot.getApi().deleteWebhook();

	up::db db("db.bin");

	bot.getEvents().onCommand("reg", [&](TgBot::Message::Ptr msg) {
		if (!msg->chat) {
			std::cerr << "Невозможно переслать сообщение" << std::endl;
			return;
		}

		if (!msg->from) {
			bot.getApi().sendMessage(msg->chat->id, "Несуществующий пользователь");
			std::cerr << "Несуществующий пользователь" << std::endl;
			return;
		}

		auto userId = msg->from->id;
		if (ADMINS.count(userId) == 0) {
			auto m = fmt::format("У вас({}({} {}): {}) нет доступа к этой команде", msg->from->username,
			    msg->from->firstName, msg->from->lastName, msg->from->id);

			bot.getApi().sendMessage(msg->chat->id, m);
			std::cout << m << std::endl;

			return;
		}

		auto iggid = msg->text.substr(4);
		iggid.erase(std::remove_if(iggid.begin(), iggid.end(), [](unsigned char x) { return std::isspace(x); }),
		    iggid.end());

		up::vm_store_record(db).store_or_throw("iggid", up::value::object{{"id", iggid}});

		bot.getApi().sendMessage(msg->chat->id, "Пользователь добавлен");
		std::cout << "Пользователь добавлен" << std::endl;
	});

	bot.getEvents().onCommand("code", [&](TgBot::Message::Ptr msg) {
		if (!msg->chat) {
			std::cerr << "Невозможно переслать сообщение" << std::endl;
			return;
		}

		if (!msg->from) {
			bot.getApi().sendMessage(msg->chat->id, "Несуществующий пользователь");
			std::cerr << "Несуществующий пользователь" << std::endl;
			return;
		}

		auto code = msg->text.substr(4);
		code.erase(std::remove_if(code.begin(), code.end(), [](unsigned char x) { return std::isspace(x); }),
		    code.end());

		struct Stats {
			size_t totalUsers = 0;
			size_t totalActivates = 0;
			std::set<std::string> errorMsgs;
		} stats;

		up::value value = up::vm_fetch_all_records(db).fetch_value_or_throw("iggid");

		bot.getApi().sendMessage(msg->chat->id, fmt::format("Старт активации", stats.totalActivates, stats.totalUsers));

		value.foreach_array([&](int64_t i, const up::value& v) {
			auto id = v.at("id").get_string_or_throw();
			using StrPair = std::pair<std::string, std::string>;
			StrPair userData{id, code};

			CURL* handle = curl_easy_init();
			if (!handle) {
				return true;
			}

			auto data = fmt::format("iggid={}&cdkey={}&username=&sign=0", id, code);

			curl_easy_setopt(handle, CURLOPT_POST, 1);
			curl_easy_setopt(handle, CURLOPT_URL, "https://dut.igg.com/event/code?ang=eng");
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
    for(const auto& e: stats.errorMsgs) {
      uniqueErrors += e;
      uniqueErrors += "\n";
    }

		bot.getApi().sendMessage(msg->chat->id, fmt::format("Статистика активации: {}/{} \nОшибки:\n{}",
		                                            stats.totalActivates, stats.totalUsers, uniqueErrors));
	});

	TgLongPoll longPoll(bot);
	while (true) {
		try {
			printf("Long poll started\n");
			longPoll.start();
		} catch (std::exception& e) { printf("error: %s\n", e.what()); }
	}
}
