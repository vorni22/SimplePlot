#include "SdCardMenu.h"

#include <string.h>

extern uint8_t SmallFont[];

namespace {
const uint8_t kRows = 4;
const uint8_t kCols = 4;
const uint8_t kHeaderH = 36;
const uint16_t kScreenW = 320;
const uint16_t kScreenH = 240;

const uint8_t kBtnW = (uint8_t)(kScreenW / kCols);
const uint8_t kBtnH = (uint8_t)((kScreenH - kHeaderH) / kRows);

enum ActionType {
	ActionNone,
	ActionUp,
	ActionDown,
	ActionCancel,
	ActionDone,
	ActionClear
};

struct Button {
	const char *label;
	ActionType action;
};

const Button kButtons[5] = {
	{"^", ActionUp},
	{"v", ActionDown},
	{"CANCEL", ActionCancel},
	{"DONE", ActionDone},
	{"CLEAR", ActionClear}
};

ActionType action_for_key(uint8_t row, uint8_t col) {
	if (row == 0) {
		switch (col) {
			case 0:
				return ActionUp;
			case 1:
				return ActionDown;
			case 2:
				return ActionCancel;
			case 3:
				return ActionDone;
			default:
				return ActionNone;
		}
	}
	if (row == 1 && col == 0) {
		return ActionClear;
	}
	return ActionNone;
}
}

SdCardMenu::SdCardMenu(const char *func0, const char *func1, const char *func2)
	: selected_(0),
	  last_mask_(0),
	  dirty_(true),
	  done_(false),
	  cancel_(false),
	  clear_(false) {
	copy_function(0, func0);
	copy_function(1, func1);
	copy_function(2, func2);
}

void SdCardMenu::draw(UTFT &screen) {
	if (!dirty_) {
		return;
	}

	screen.setFont(SmallFont);
	screen.clrScr();

	screen.setColor(30, 30, 30);
	screen.fillRect(0, 0, kScreenW - 1, kHeaderH - 1);
	screen.setBackColor(30, 30, 30);
	screen.setColor(255, 255, 255);
	screen.print("SD CARD", 5, 5);

	for (uint8_t i = 0; i < 3; ++i) {
		uint16_t y = (uint16_t)(kHeaderH + i * (kBtnH + 2));
		uint8_t shade = (i == selected_) ? 100 : 60;
		screen.setColor(shade, shade, shade);
		screen.fillRect(5, y, kScreenW - 6, (uint16_t)(y + kBtnH - 4));
		screen.setBackColor(shade, shade, shade);
		screen.setColor(255, 255, 255);
		screen.print(functions_[i], 10, (uint16_t)(y + 6));
	}

	uint16_t buttons_y = (uint16_t)(kHeaderH + 3 * (kBtnH + 2) + 8);
	for (uint8_t c = 0; c < 4; ++c) {
		const Button &btn = kButtons[c];
		uint16_t x1 = (uint16_t)(c * kBtnW);
		uint16_t y1 = buttons_y;
		uint16_t x2 = (uint16_t)(x1 + kBtnW - 2);
		uint16_t y2 = (uint16_t)(y1 + kBtnH - 2);

		screen.setColor(70, 70, 70);
		screen.fillRect(x1, y1, x2, y2);
		screen.setBackColor(70, 70, 70);
		screen.setColor(255, 255, 255);
		screen.print(btn.label, x1 + 4, y1 + 10);
	}

	{
		const Button &btn = kButtons[4];
		uint16_t x1 = 0;
		uint16_t y1 = (uint16_t)(buttons_y + kBtnH);
		uint16_t x2 = (uint16_t)(x1 + kBtnW - 2);
		uint16_t y2 = (uint16_t)(y1 + kBtnH - 2);

		screen.setColor(70, 70, 70);
		screen.fillRect(x1, y1, x2, y2);
		screen.setBackColor(70, 70, 70);
		screen.setColor(255, 255, 255);
		screen.print(btn.label, x1 + 4, y1 + 10);
	}

	dirty_ = false;
}

void SdCardMenu::poll_keyboard(KeyboardMatrix &keyboard) {
	uint16_t mask = keyboard.getStateMask();
	uint16_t pressed = (uint16_t)(mask & (uint16_t)~last_mask_);
	last_mask_ = mask;

	if (pressed == 0) {
		return;
	}

	for (uint8_t r = 0; r < kRows; ++r) {
		for (uint8_t c = 0; c < kCols; ++c) {
			uint8_t index = (uint8_t)(r * kCols + c);
			uint16_t bit = (uint16_t)(1u << index);
			if (pressed & bit) {
				handle_key_press(r, c);
			}
		}
	}
}

void SdCardMenu::request_redraw() {
	dirty_ = true;
}

const char *SdCardMenu::get_function(uint8_t index) const {
	if (index >= 3) {
		return 0;
	}
	return functions_[index];
}

bool SdCardMenu::set_function(uint8_t index, const char *value) {
	if (index >= 3) {
		return false;
	}
	copy_function(index, value);
	dirty_ = true;
	return true;
}

uint8_t SdCardMenu::get_selected_index() const {
	return selected_;
}

const char *SdCardMenu::get_selected_function() const {
	return functions_[selected_];
}

bool SdCardMenu::consume_done() {
	bool value = done_;
	done_ = false;
	return value;
}

bool SdCardMenu::consume_cancel() {
	bool value = cancel_;
	cancel_ = false;
	return value;
}

bool SdCardMenu::consume_clear() {
	bool value = clear_;
	clear_ = false;
	return value;
}

void SdCardMenu::clear_all() {
	for (uint8_t i = 0; i < 3; ++i) {
		functions_[i][0] = '\0';
	}
	selected_ = 0;
	dirty_ = true;
}

void SdCardMenu::copy_function(uint8_t index, const char *value) {
	if (!value) {
		functions_[index][0] = '\0';
		return;
	}
	strncpy(functions_[index], value, sizeof(functions_[index]) - 1);
	functions_[index][sizeof(functions_[index]) - 1] = '\0';
}

void SdCardMenu::handle_key_press(uint8_t row, uint8_t col) {
	ActionType action = action_for_key(row, col);

	switch (action) {
		case ActionUp:
			if (selected_ > 0) {
				--selected_;
				dirty_ = true;
			}
			break;
		case ActionDown:
			if (selected_ < 2) {
				++selected_;
				dirty_ = true;
			}
			break;
		case ActionCancel:
			cancel_ = true;
			break;
		case ActionDone:
			done_ = true;
			break;
		case ActionClear:
			clear_ = true;
			break;
		case ActionNone:
		default:
			break;
	}
}

