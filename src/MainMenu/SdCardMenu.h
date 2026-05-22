#ifndef SD_CARD_MENU_H
#define SD_CARD_MENU_H

#include <Arduino.h>
#include <UTFT.h>

#include "Keyboard/Keyboard.h"

class SdCardMenu {
public:
	SdCardMenu(const char *func0, const char *func1, const char *func2);

	void draw(UTFT &screen);
	void poll_keyboard(KeyboardMatrix &keyboard);
	void request_redraw();

	const char *get_function(uint8_t index) const;
	bool set_function(uint8_t index, const char *value);

	uint8_t get_selected_index() const;
	const char *get_selected_function() const;

	bool consume_done();
	bool consume_cancel();
	bool consume_clear();

	void clear_all();

private:
	void copy_function(uint8_t index, const char *value);
	void handle_key_press(uint8_t row, uint8_t col);

	char functions_[3][64];
	uint8_t selected_;
	uint16_t last_mask_;
	bool dirty_;
	bool done_;
	bool cancel_;
	bool clear_;
};

#endif