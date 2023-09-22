#pragma once

#include <unqlite_cpp/unqlite_cpp.hpp>

#include <array>
#include <cinttypes>
#include <iostream>
#include <string>

struct UserInfo {
	std::int64_t id;
	std::int64_t chatId;
	std::string name;

	std::string key() const { return "user_info_" + std::to_string(id); }

	template<class Value>
	void from(const Value& value) {
		id = value.at("id").get_int_or_throw();
		chatId = value.at("chat_id").get_int_or_throw();
		name = value.at("name").get_string_or_throw();
	}

	up::value to() const {
		up::value value;

		value["id"] = id;
		value["chat_id"] = chatId;
		value["name"] = name;

		return value;
	}
};
