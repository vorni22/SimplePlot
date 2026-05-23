#include "PlotController.h"

#define MIDDLE_ROT_X   553
#define MIDDLE_ROT_Y   550

#define MIDDLE_MOVE_X  535
#define MIDDLE_MOVE_Y  542

#define HEIGHT_STEPS 240  // fixed total steps from -R to +R

static inline uint16_t map_abs_to_0_10(int16_t delta, int16_t max_range) {
    if (max_range <= 0) return 0;
    int16_t ad = (delta < 0) ? -delta : delta;
    uint16_t v = (uint16_t)((ad * 10) / max_range);
    return (v > 10) ? 10 : v;
}

static inline int32_t map_axis_to_q16(uint16_t value, uint16_t midpoint) {
    int16_t delta = (int16_t)value - (int16_t)midpoint;
    if (delta == 0) return 0;

    int16_t max_range = (delta > 0)
        ? (int16_t)(1023 - midpoint)
        : (int16_t)midpoint;

    if (max_range <= 0) return 0;

    int32_t scaled = ((int32_t)delta * (1LL << 16)) / max_range;
    if (scaled > (1LL << 16)) return (1LL << 16);
    if (scaled < -(1LL << 16)) return -(1LL << 16);
    return scaled;
}


PlotController::PlotController(Ploter *ploter) {
    changed_pos = true;
    ploter_ptr = ploter;
    angle_x = 0;
    height_y = 0;
    last_mask_ = 0;
    should_go_back = false;
}

void PlotController::rotate_x(uint16_t x) {
    int16_t dx = (int16_t)x - (int16_t)MIDDLE_ROT_X;
    if (dx == 0) return;

    int16_t max_range = (dx > 0)
        ? (int16_t)(1023 - MIDDLE_ROT_X)
        : (int16_t)MIDDLE_ROT_X;

    uint16_t delta_deg = map_abs_to_0_10(dx, max_range);
    if (delta_deg == 0) return;

    if (dx > 0)
        angle_x = (angle_x + delta_deg) % 360;
    else
        angle_x = (angle_x + 360 - delta_deg) % 360;

    changed_pos = true;
}

void PlotController::move_origin_x(uint16_t dx) {
    int32_t offset = map_axis_to_q16(dx, MIDDLE_MOVE_Y);
    if (offset < 1500 && offset > -1500) {
        return;
    }

    printf("dx==%ld\n", offset);

    origin_x += offset;
    ploter_ptr->set_center({ origin_x, 0, origin_z });
    changed_pos = true;
}

void PlotController::rotate_y(uint16_t y) {
    int16_t dy = (int16_t)y - (int16_t)MIDDLE_ROT_Y;
    if (dy == 0) return;

    int16_t max_range = (dy > 0)
        ? (int16_t)(1023 - MIDDLE_ROT_Y)
        : (int16_t)MIDDLE_ROT_Y;

    if (max_range <= 0) return;

    uint16_t steps = map_abs_to_0_10(dy, max_range);
    if (steps == 0) return;

    int32_t r = ploter_ptr->get_radius();
    int32_t max_h = (r * 9) / 10; // 90% of R

    // fixed step size for full range: 2R / HEIGHT_STEPS
    int32_t step_q16 = (r << 1) / HEIGHT_STEPS;
    int32_t delta_h = (int32_t)steps * step_q16;
    if (dy < 0) delta_h = -delta_h;

    int32_t h = height_y + delta_h;
    if (h >  max_h) h =  max_h;
    if (h < -max_h) h = -max_h;
    height_y = h;

    changed_pos = true;
}

void PlotController::move_origin_y(uint16_t dy) {
    int32_t offset = map_axis_to_q16(dy, MIDDLE_MOVE_Y);
    if (offset < 1200 && offset > -1200) {
        return;
    }
    origin_z += offset;
    ploter_ptr->set_center({ origin_x, 0, origin_z });
    changed_pos = true;
}

void PlotController::zoom_in() {
    ploter_ptr->zoom_in(2000);
    changed_pos = true;
}

void PlotController::zoom_out() {
    ploter_ptr->zoom_out(2000);
    changed_pos = true;
}

void PlotController::draw(UTFT &myGLCD) {
    if (!changed_pos) return;

    myGLCD.setColor(0, 0, 0);
    myGLCD.clrScr();
    //ploter_ptr->draw_fast(myGLCD);

    ploter_ptr->set_orbit(angle_x, height_y);

    myGLCD.setColor(255, 255, 255);
    ploter_ptr->draw_fast(myGLCD);
    changed_pos = false;
}

void PlotController::poll_keyboard(KeyboardMatrix &keyboard) {
    uint16_t mask = keyboard.getStateMask();
	uint16_t pressed = (uint16_t)(mask & (uint16_t)~last_mask_);
	last_mask_ = mask;

	if (pressed == 0) {
		return;
	}

    uint8_t zoom_in_idx = (uint8_t)(0);
	uint16_t zoom_in_bit = (uint16_t)(1u << zoom_in_idx);
    
    uint8_t zoom_out_idx = (uint8_t)(1);
	uint16_t zoom_out_bit = (uint16_t)(1u << zoom_out_idx);

    uint8_t y_axis_inc_idx = (uint8_t)(2);
	uint16_t y_axis_inc_bit = (uint16_t)(1u << y_axis_inc_idx);

    uint8_t y_axis_dec_idx = (uint8_t)(3);
	uint16_t y_axis_dec_bit = (uint16_t)(1u << y_axis_dec_idx);

    uint8_t exit_idx = (uint8_t)(15);
	uint16_t exit_bit = (uint16_t)(1u << exit_idx);

    if (pressed & zoom_in_bit) {
		zoom_in();
	}

    if (pressed & zoom_out_bit) {
        zoom_out();
    }

    if (pressed & y_axis_inc_bit) {
		add_to_y_axis_scale(6000LL);
	}

    if (pressed & y_axis_dec_bit) {
        add_to_y_axis_scale(-6000LL);
    }

    if (pressed & exit_bit) {
        should_go_back = true;
    }
}

void PlotController::add_to_y_axis_scale(int32_t add) {
    ploter_ptr->add_to_y_axis_scale(add);
    changed_pos = true;
}

bool PlotController::go_back() {
    return should_go_back;
}