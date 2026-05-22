#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include <Arduino.h>
#include <UTFT.h>

#include <inttypes.h>
#include <stdio.h>

#include "usart.h"
#include "adc.h"
#include "Keyboard/Keyboard.h"
#include "MainMenu/MenuMaster.h"
#include "Parser/Parser.h"

#include <SD/pff.h>
#include <SD/sd.h>
#include <SD/TestSD.h>
#include <Plot3D/PlotController.h>

UTFT myGLCD(ILI9341_16, 38, 39, 40, 41);
PlotFunction test_func;

// Port K bit indices (AD8..AD15 => PK0..PK7)
static const uint8_t kbd_rows[] = {3, 1, 2, 0};
static const uint8_t kbd_cols[] = {4, 5, 6, 7};
static KeyboardMatrix keyboard(kbd_rows, 4, kbd_cols, 4);
static MenuMaster *menu = 0;
static Parser parser;
static Parser::TokenBuffer rpn_tokens;
static Ploter *plot = 0;
static PlotController *contr = 0;

ISR(PCINT2_vect) {
    keyboard.onPinChange();
}

void tft_init() {
    myGLCD.InitLCD();
    myGLCD.clrScr();
}

void tft_draw_center_square(uint16_t size, uint16_t color) {
    uint16_t w = myGLCD.getDisplayXSize();
    uint16_t h = myGLCD.getDisplayYSize();
    uint16_t x = (w - size) / 2;
    uint16_t y = (h - size) / 2;
    myGLCD.setColor(color);
    myGLCD.fillRect(x, y, x + size - 1, y + size - 1);
}

void setup() {
    usart_init(9600);
    adc_init_freerun_interrupt();
    tft_init();
    keyboard.begin();
    Parser::init_buffer(rpn_tokens, 64);
    sei();
    
    menu = new MenuMaster();
    menu->init_from_sd(myGLCD);
}

void UI_logic() {
    keyboard.poll();
    if (!menu) {
        return;
    }
    menu->poll_keyboard(keyboard, myGLCD);
    menu->draw(myGLCD);

    if (menu->consume_plot_requested()) {
        const char *expr = menu->get_current_function();
        bool ok = parser.parse(expr, rpn_tokens);
        printf("Plot requested: %s\n", expr);
        if (ok) {
            Parser::print_token_buffer(rpn_tokens);
            test_func.set_tokens(&rpn_tokens);

            delete menu;
            menu = 0;
            plot = new Ploter(test_func, 12, 12);
            contr = new PlotController(*plot);
        }
    }
}

void loop() {
    UI_logic();

    uint16_t x = adc_get_channel(1);
    uint16_t y = adc_get_channel(0);

    uint16_t dx = adc_get_channel(3);
    uint16_t dy = adc_get_channel(2);

    // Erase previous frame by redrawing in black
    if (contr) {
        contr->rotate_x(x);
        contr->rotate_y(y);

        contr->move_origin_x(dx);
        contr->move_origin_y(dy);
    }

    myGLCD.setColor(255, 255, 255);
    if (contr) {
        contr->draw(myGLCD);
    }

    _delay_ms(10);
}

int main() {
    init(); 
    setup();

    while(1)
        loop();
    
    return 0;
}
