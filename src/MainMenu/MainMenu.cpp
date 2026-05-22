#include "MainMenu.h"

#include <string.h>

extern uint8_t SmallFont[];

namespace {
const uint8_t kRows = 4;
const uint8_t kCols = 4;
const uint8_t kHeaderH = 40;
const uint8_t kBtnW = SCREEN_W / kCols;
const uint8_t kBtnH = (SCREEN_H - kHeaderH) / kRows;

enum ActionType {
	ActionNone,
	ActionInsert,
	ActionDelete,
	ActionToggleLayout,
	ActionPlot,
	ActionSdSaveToggle,
	ActionSdMenu
};

struct Button {
	const char *label;
	ActionType action;
	const char *token;
};

const Button kLayout0[kRows * kCols] = {
	{"LAY", ActionToggleLayout, 0},
	{"DEL", ActionDelete, 0},
	{"(", ActionInsert, "("},
	{")", ActionInsert, ")"},

	{"7", ActionInsert, "7"},
	{"8", ActionInsert, "8"},
	{"9", ActionInsert, "9"},
	{"x", ActionInsert, "x"},

	{"4", ActionInsert, "4"},
	{"5", ActionInsert, "5"},
	{"6", ActionInsert, "6"},
	{"y", ActionInsert, "y"},

	{"1", ActionInsert, "1"},
	{"2", ActionInsert, "2"},
	{"3", ActionInsert, "3"},
	{"0", ActionInsert, "0"}
};

const Button kLayout1[kRows * kCols] = {
	{"LAY", ActionToggleLayout, 0},
	{"DEL", ActionDelete, 0},
	{"PLOT", ActionPlot, 0},
	{"ceil", ActionInsert, "ceil("},

	{"+", ActionInsert, "+"},
	{"-", ActionInsert, "-"},
	{"*", ActionInsert, "*"},
	{"/", ActionInsert, "/"},

	{"sin", ActionInsert, "sin("},
	{"cos", ActionInsert, "cos("},
	{"log", ActionInsert, "log("},
    {"pow", ActionInsert, "pow("},

    {",", ActionInsert, ","},
	{"floor", ActionInsert, "floor("},
	{"SAVE", ActionSdSaveToggle, 0},
	{"LOAD", ActionSdMenu, 0}
};

uint8_t to_index(uint8_t row, uint8_t col) {
	return (uint8_t)(row * kCols + col);
}
}

MainMenu::MainMenu()
	: layout_(0),
	  func_len_(0),
	  function_(),
	  last_mask_(0),
	  ready_(false),
	  dirty_(true),
	  plot_requested_(false),
	  sd_save_toggled_(false),
	  sd_menu_requested_(false) {
	function_[0] = '\0';
}

void MainMenu::draw(UTFT &screen) {
	if (!dirty_) {
		return;
	}

	screen.setFont(SmallFont);
	screen.clrScr();

	screen.setColor(30, 30, 30);
	screen.fillRect(0, 0, SCREEN_W - 1, kHeaderH - 1);
	screen.setBackColor(30, 30, 30);
	screen.setColor(255, 255, 255);
	screen.print(layout_ == 0 ? "LAY0" : "LAY1", 5, 5);
	screen.print(function_, 5, 20);
	screen.print("SD: SAVE toggle | MENU", 120, 5);

	const Button *layout = (layout_ == 0) ? kLayout0 : kLayout1;

	for (uint8_t r = 0; r < kRows; ++r) {
		for (uint8_t c = 0; c < kCols; ++c) {
			uint16_t x1 = (uint16_t)(c * kBtnW);
			uint16_t y1 = (uint16_t)(kHeaderH + r * kBtnH);
			uint16_t x2 = (uint16_t)(x1 + kBtnW - 2);
			uint16_t y2 = (uint16_t)(y1 + kBtnH - 2);

			screen.setColor(70, 70, 70);
			screen.fillRect(x1, y1, x2, y2);
			screen.setBackColor(70, 70, 70);
			screen.setColor(255, 255, 255);
			screen.print(layout[to_index(r, c)].label, x1 + 8, y1 + 12);
		}
	}

	dirty_ = false;
	ready_ = true;
}

void MainMenu::poll_keyboard(KeyboardMatrix &keyboard) {
	uint16_t mask = keyboard.getStateMask();
	uint16_t pressed = (uint16_t)(mask & (uint16_t)~last_mask_);
	last_mask_ = mask;

	if (pressed == 0) {
		return;
	}

	for (uint8_t r = 0; r < kRows; ++r) {
		for (uint8_t c = 0; c < kCols; ++c) {
			uint8_t index = to_index(r, c);
			uint16_t bit = (uint16_t)(1u << index);
			if (pressed & bit) {
				handle_key_press(r, c);
			}
		}
	}
}

const char *MainMenu::get_current_function() const {
	return function_;
}

void MainMenu::set_current_function(const char *value) {
	if (!value) {
		function_[0] = '\0';
		func_len_ = 0;
		dirty_ = true;
		return;
	}

	strncpy(function_, value, sizeof(function_) - 1);
	function_[sizeof(function_) - 1] = '\0';
	func_len_ = (uint8_t)strlen(function_);
	dirty_ = true;
}

void MainMenu::request_redraw() {
	dirty_ = true;
}

bool MainMenu::is_ready() const {
	return ready_;
}

bool MainMenu::consume_ready() {
	bool value = ready_;
	ready_ = false;
	return value;
}

bool MainMenu::consume_plot_requested() {
	bool value = plot_requested_;
	plot_requested_ = false;
	return value;
}

bool MainMenu::consume_sd_save_toggled() {
	bool value = sd_save_toggled_;
	sd_save_toggled_ = false;
	return value;
}

bool MainMenu::consume_sd_menu_requested() {
	bool value = sd_menu_requested_;
	sd_menu_requested_ = false;
	return value;
}

void MainMenu::append_char(char c) {
	if (func_len_ + 1 >= sizeof(function_)) {
		return;
	}

	function_[func_len_] = c;
	func_len_++;
	function_[func_len_] = '\0';
	dirty_ = true;
}

void MainMenu::append_token(const char *token) {
	if (token == 0) {
		return;
	}

	size_t len = strlen(token);
	if ((size_t)func_len_ + len >= sizeof(function_)) {
		return;
	}

	memcpy(function_ + func_len_, token, len);
	func_len_ = (uint8_t)(func_len_ + len);
	function_[func_len_] = '\0';
	dirty_ = true;
}

void MainMenu::delete_last_char() {
	if (func_len_ == 0) {
		return;
	}

	func_len_--;
	function_[func_len_] = '\0';
	dirty_ = true;
}

void MainMenu::handle_key_press(uint8_t row, uint8_t col) {
	const Button *layout = (layout_ == 0) ? kLayout0 : kLayout1;
	const Button &btn = layout[to_index(row, col)];

	switch (btn.action) {
		case ActionInsert:
			append_token(btn.token);
			break;
		case ActionDelete:
			delete_last_char();
			break;
		case ActionToggleLayout:
			layout_ = (uint8_t)((layout_ + 1) % 2);
			dirty_ = true;
			break;
		case ActionPlot:
			plot_requested_ = true;
			break;
		case ActionSdSaveToggle:
			sd_save_toggled_ = true;
			break;
		case ActionSdMenu:
			sd_menu_requested_ = true;
			break;
		case ActionNone:
		default:
			break;
	}
}