#pragma once

#include "user_info.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <unqlite_cpp/unqlite_cpp.hpp>

#include <ctime>
#include <iostream>
#include <memory>
#include <unordered_map>

class Db {
  public:
	Db(std::string_view name): _db(std::string(name), up::db_mode::OPEN_CREATE) {}

	void storeUser(const UserInfo& info) {
		auto vm = *up::vm_store_record::make(_db);
		vm.store("user_info_" + std::to_string(info.id), info.to());
	}

	std::optional<UserInfo> loadUser(std::int64_t id) {
		auto vm = *up::vm_fetch_record::make(_db);
		UserInfo ui;
		ui.id = id;
		auto value = vm.fetch(ui.key());
		if (value && !value->is_null()) {
			spdlog::debug("UserInfo.id({}) found.", id);

			ui.from(*value);

			return ui;
		} else {
			spdlog::debug("UserInfo.id({}) not found.", id);
			return {};
		}
	}

	std::optional<up::vm_value> fetch(std::int64_t id, std::string_view collection) {
		auto vm = *up::vm_fetch_all_records::make(_db);
		auto v = vm.fetch(fmt::format("user_info_{}_{}", id, collection));
		return v;
	}

	auto& db() { return _db; }

  private:
	up::db _db;
};
