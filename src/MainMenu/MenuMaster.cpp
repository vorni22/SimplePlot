#include "MenuMaster.h"

#include <string.h>

#include <SD/pff.h>
#include <SD/sd.h>

extern uint8_t SmallFont[];

namespace {
const char *kSdFileName = "data.txt";
FATFS sd_fs;
}

MenuMaster::MenuMaster()
	: main_menu_(),
	  sd_menu_("", "", ""),
	  active_(ActiveMenu::Main),
	  sd_action_(SdAction::None),
	  sd_mounted_(false) {}

void MenuMaster::draw(UTFT &screen) {
	if (active_ == ActiveMenu::Sd) {
		sd_menu_.draw(screen);
		return;
	}

	main_menu_.draw(screen);
}

void MenuMaster::poll_keyboard(KeyboardMatrix &keyboard, UTFT &screen) {
	if (active_ == ActiveMenu::Sd) {
		sd_menu_.poll_keyboard(keyboard);

		if (sd_menu_.consume_clear()) {
			sd_menu_.clear_all();
			save_functions_to_sd(screen);
			sd_menu_.request_redraw();
			return;
		}

		if (sd_menu_.consume_cancel()) {
			active_ = ActiveMenu::Main;
			sd_action_ = SdAction::None;
			main_menu_.request_redraw();
			return;
		}

		if (sd_menu_.consume_done()) {
			if (sd_action_ == SdAction::Load) {
				main_menu_.set_current_function(sd_menu_.get_selected_function());
			} else if (sd_action_ == SdAction::Save) {
				sd_menu_.set_function(sd_menu_.get_selected_index(), main_menu_.get_current_function());
				save_functions_to_sd(screen);
			}

			active_ = ActiveMenu::Main;
			sd_action_ = SdAction::None;
			main_menu_.request_redraw();
			return;
		}

		return;
	}

	main_menu_.poll_keyboard(keyboard);

	if (main_menu_.consume_sd_menu_requested()) {
		active_ = ActiveMenu::Sd;
		sd_action_ = SdAction::Load;
		sd_menu_.request_redraw();
		return;
	}

	if (main_menu_.consume_sd_save_toggled()) {
		active_ = ActiveMenu::Sd;
		sd_action_ = SdAction::Save;
		sd_menu_.request_redraw();
		return;
	}
}

void MenuMaster::init_from_sd(UTFT &screen) {
	load_functions_from_sd(screen);
	sd_menu_.request_redraw();
}

const char *MenuMaster::get_current_function() const {
	return main_menu_.get_current_function();
}

bool MenuMaster::consume_plot_requested() {
	return main_menu_.consume_plot_requested();
}

bool MenuMaster::mount_sd_if_needed() {
	if (sd_mounted_) {
		return true;
	}
	FRESULT mount_result = pf_mount(&sd_fs);
	if (mount_result != FR_OK) {
		printf("SD mount failed (%d)\n", (int)mount_result);
		return false;
	}
	sd_mounted_ = true;
	printf("SD mount ok\n");
	return true;
}

void MenuMaster::draw_loading(UTFT &screen, const char *label) {
	screen.setFont(SmallFont);
	screen.clrScr();
	screen.setColor(0, 0, 0);
	screen.fillRect(0, 0, 319, 239);
	screen.setBackColor(0, 0, 0);
	screen.setColor(255, 255, 255);
	screen.print(label, 10, 10);
}

bool MenuMaster::load_functions_from_sd(UTFT &screen) {
	draw_loading(screen, "LOADING SD...");
	printf("SD load: starting\n");
	if (!mount_sd_if_needed()) {
		return false;
	}

	FRESULT open_result = pf_open(kSdFileName);
	if (open_result != FR_OK) {
		printf("SD load: open '%s' failed (%d)\n", kSdFileName, (int)open_result);
		sd_menu_.clear_all();
		return false;
	}

	FRESULT seek_result = pf_lseek(0);
	if (seek_result != FR_OK) {
		printf("SD load: seek failed (%d)\n", (int)seek_result);
		return false;
	}

	char line[64];
	for (uint8_t idx = 0; idx < 3; ++idx) {
		WORD br = 0;
		FRESULT read_result = pf_read(line, sizeof(line), &br);
		if (read_result != FR_OK) {
			printf("SD load: read failed (%d)\n", (int)read_result);
			break;
		}
		if (br < sizeof(line)) {
			memset(line + br, 0, sizeof(line) - br);
		}
		line[sizeof(line) - 1] = '\0';
		sd_menu_.set_function(idx, line);
	}

	printf("SD load: done\n");

	return true;
}

bool MenuMaster::save_functions_to_sd(UTFT &screen) {
	draw_loading(screen, "SAVING SD...");
	printf("SD save: starting\n");
	if (!mount_sd_if_needed()) {
		return false;
	}

	FRESULT open_result = pf_open(kSdFileName);
	if (open_result != FR_OK) {
		printf("SD save: open '%s' failed (%d)\n", kSdFileName, (int)open_result);
		return false;
	}

	FRESULT seek_result = pf_lseek(0);
	if (seek_result != FR_OK) {
		printf("SD save: seek failed (%d)\n", (int)seek_result);
		return false;
	}

	WORD bw = 0;
	for (uint8_t idx = 0; idx < 3; ++idx) {
		const char *value = sd_menu_.get_function(idx);
		if (!value) {
			value = "";
		}
		char slot[64];
		memset(slot, 0, sizeof(slot));
		strncpy(slot, value, sizeof(slot) - 1);
		FRESULT write_result = pf_write(slot, sizeof(slot), &bw);
		if (write_result != FR_OK || bw != sizeof(slot)) {
			printf("SD save: slot %u write failed (%d) bytes %u/%u\n",
				(unsigned)idx, (int)write_result, (unsigned)bw, (unsigned)sizeof(slot));
		}
	}
	FRESULT finalize_result = pf_write(0, 0, &bw);
	if (finalize_result != FR_OK) {
		printf("SD save: finalize failed (%d)\n", (int)finalize_result);
		return false;
	}

	printf("SD save: done\n");

	return true;
}
