#ifndef MAINMENU_H
#define MAINMENU_H

#include <Arduino.h>
#include <UTFT.h>

#include "Keyboard/Keyboard.h"

#define SCREEN_W  320
#define SCREEN_H  240

class MainMenu {
public:
    MainMenu();

    void draw(UTFT &screen);
    void poll_keyboard(KeyboardMatrix &keyboard);

    const char *get_current_function() const;
    void set_current_function(const char *value);
    void request_redraw();

    bool is_ready() const;
    bool consume_ready();
    bool consume_plot_requested();
    bool consume_sd_save_toggled();
    bool consume_sd_menu_requested();

private:
    void append_char(char c);
    void append_token(const char *token);
    void delete_last_char();
    void handle_key_press(uint8_t row, uint8_t col);

    uint8_t layout_;
    uint8_t func_len_;
    char function_[64];
    uint16_t last_mask_;
    bool ready_;
    bool dirty_;
    bool plot_requested_;
    bool sd_save_toggled_;
    bool sd_menu_requested_;
};

#endif