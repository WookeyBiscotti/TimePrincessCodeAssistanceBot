#pragma once

#include "user_context.hpp"

#include <tgbot/Api.h>


TgBot::InlineKeyboardButton::Ptr makeButon(const std::string& label, const std::string& key);

void setButton(TgBot::InlineKeyboardMarkup::Ptr keyboard, size_t x, size_t y, TgBot::InlineKeyboardButton::Ptr btn);
