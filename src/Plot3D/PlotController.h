#ifndef PLOT_CONTROLER_H
#define PLOT_CONTROLER_H

#include "Ploter.h"

class PlotController {
public:
    PlotController(Ploter& ploter);

    void rotate_x(uint16_t x);
    void rotate_y(uint16_t y);

    void move_origin_x(uint16_t dx);
    void move_origin_y(uint16_t dy);

    void zoom_in();
    void zoom_out();

    void draw(UTFT &myGLCD);

private:
    Ploter *ploter_ptr;
    bool changed_pos;

    uint16_t angle_x = 0; // from 0 degrees to 360 degrees.
    int32_t height_y = 0; // from -R to R.
    int32_t origin_x = 0;
    int32_t origin_z = 0;
};

#endif