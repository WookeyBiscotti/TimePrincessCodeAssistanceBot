#pragma once

#include <string>

#include "user_context.hpp"

class Command {
  public:
	virtual void onCommand(UserContext& context, const std::string& command) {}
	virtual void onNonCommand(UserContext& context, const std::string& nonCommand) {}
	virtual void onQuery(UserContext& context, const std::string& query) {}
};
