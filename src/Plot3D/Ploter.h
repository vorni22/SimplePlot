#ifndef PLOTER_H
#define PLOTER_H

#include <UTFT.h>
#include "PlotFunction.h"
#include "Geometry.h"

class Ploter {
public:
    // step_q16 = 0 → auto-compute so ~80 world-units span the whole grid width.
    // Camera is placed automatically so the entire grid is visible.
    Ploter(PlotFunction &function, uint8_t samples_x, uint8_t samples_y,
           int32_t step_q16 = 0);

    ~Ploter();

    // ── CAMERA ORBIT ─────────────────────────────────────────
    // Move camera anywhere on a sphere of radius radius_q16 centred on center_.
    //   angle_deg  : azimuth  (0…359, horizontal rotation around Y axis)
    //   height_q16 : elevation in Q16.16  (positive = above grid plane)
    //                height is clamped to ±radius so camera stays on the sphere.
    // The camera always looks at center_.
    void set_orbit(uint16_t angle_deg, int32_t height_q16);

    // Change the orbital radius (zoom in / zoom out).
    void set_radius(int32_t radius_q16);
    void zoom_in (int32_t delta_q16);   // decrease radius
    void zoom_out(int32_t delta_q16);   // increase radius

    // Move the world-space pivot the camera orbits around.
    void set_center(ivec3_t center_q16);

    // Recalculate function values and redraw.
    void draw(UTFT &myGLCD);
    void draw_fast(UTFT &myGLCD);

    int32_t get_radius() const { return radius_q16_; }

private:
    // Rebuild func_values[] from the current PlotFunction.
    void refresh_grid();

    void draw_axes(UTFT &myGLCD, const mat3_t &basis) const;

    // Compute a good default radius so the whole grid fits in view.
    int32_t auto_radius() const;

    // Draw a single grid line from world-point a to b, with clipping.
    void draw_edge(UTFT &myGLCD,
               const ivec3_t &a, const ivec3_t &b,
               const mat3_t &basis) const;

    void draw_edge_fast(UTFT &myGLCD,
                       const ivec3_t &a, const ivec3_t &b,
                       const mat3_t &basis) const;

    ivec3_t      *grid_points_;

    ivec2_t *screen_points_;
    uint8_t *screen_visible_;

    PlotFunction *function_;
    uint8_t       samples_x_;
    uint8_t       samples_y_;
    int32_t       step_q16_;      // grid cell size in world units, Q16.16
    ivec3_t       center_;        // pivot / look-at target, Q16.16
    camera_t      cam_;           // pos & target, Q16.16
    int32_t       radius_q16_;    // orbital radius, Q16.16
    uint16_t      angle_deg_;     // current azimuth
    int32_t       height_q16_;    // current elevation (Q16.16)
};

#endif // PLOTER_H