#ifndef MENU_MASTER_H
#define MENU_MASTER_H

#include <Arduino.h>
#include <UTFT.h>

#include "Keyboard/Keyboard.h"
#include "MainMenu.h"
#include "SdCardMenu.h"

class MenuMaster {
public:
	MenuMaster();

	void draw(UTFT &screen);
	void poll_keyboard(KeyboardMatrix &keyboard, UTFT &screen);
	void init_from_sd(UTFT &screen);

	const char *get_current_function() const;
	bool consume_plot_requested();

private:
	enum class ActiveMenu : uint8_t {
		Main,
		Sd
	};

	enum class SdAction : uint8_t {
		None,
		Load,
		Save
	};

	MainMenu main_menu_;
	SdCardMenu sd_menu_;
	ActiveMenu active_;
	SdAction sd_action_;
	bool sd_mounted_;

	void draw_loading(UTFT &screen, const char *label);
	bool mount_sd_if_needed();
	bool load_functions_from_sd(UTFT &screen);
	bool save_functions_to_sd(UTFT &screen);
};

#endif
