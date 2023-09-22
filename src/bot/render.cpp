#include "render.hpp"
#include "time_utils.hpp"

#include <date/date.h>
#include <date/tz.h>

TgBot::InlineKeyboardButton::Ptr makeButon(const std::string& label, const std::string& key) {
	TgBot::InlineKeyboardButton::Ptr bt(new TgBot::InlineKeyboardButton);
	bt->text = label;
	bt->callbackData = key;

	return bt;
}

void setButton(TgBot::InlineKeyboardMarkup::Ptr keyboard, size_t x, size_t y, TgBot::InlineKeyboardButton::Ptr btn) {
	if (keyboard->inlineKeyboard.size() <= y) {
		keyboard->inlineKeyboard.resize(y + 1);
	}
	if (keyboard->inlineKeyboard[y].size() <= x) {
		keyboard->inlineKeyboard[y].resize(x + 1);
	}
	keyboard->inlineKeyboard[y][x] = btn;
}
