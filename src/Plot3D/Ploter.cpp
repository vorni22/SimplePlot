#include "Ploter.h"
#include <stdlib.h>   // malloc / free

// ── helpers ──────────────────────────────────────────────────

static inline int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }
static inline int32_t imax32(int32_t a, int32_t b) { return a > b ? a : b; }

static int32_t isqrt32(int32_t v)
{
    if (v <= 0) return 0;
    int32_t r = v, r1 = (v + 1) >> 1;
    while (r1 < r) { r = r1; r1 = (r + v / r) >> 1; }
    return r;
}

Ploter::Ploter(PlotFunction &function, uint8_t samples_x, uint8_t samples_y,
               int32_t step_q16)
    : function_(&function),
      samples_x_(samples_x < 2 ? 2 : samples_x),
      samples_y_(samples_y < 2 ? 2 : samples_y),
      step_q16_(step_q16),
      center_({0, 0, 0}),
      angle_deg_(45),
      height_q16_(0)
{
    // Allocate value grid.
    grid_points_ = (ivec3_t *)malloc((int)samples_x_ * samples_y_ * sizeof(ivec3_t));
    screen_points_ = (ivec2_t *)malloc((int)samples_x_ * samples_y_ * sizeof(ivec2_t));
    screen_visible_ = (uint8_t *)malloc((int)samples_x_ * samples_y_ * sizeof(uint8_t));

    if (step_q16_ == 0) {
        step_q16_ = (int32_t)(0.35 * 65536.0 + 0.5);
    }

    radius_q16_ = auto_radius();

    height_q16_ = fx_mul(radius_q16_, sin_deg_q16(45));

    set_orbit(angle_deg_, height_q16_);

    refresh_grid();
}

Ploter::~Ploter()
{
    free(grid_points_);
    free(screen_points_);
    free(screen_visible_);
}

int32_t Ploter::auto_radius() const
{
    return (15LL << 16);
}

void Ploter::set_orbit(uint16_t angle_deg, int32_t height_q16)
{
    angle_deg_  = angle_deg % 360;
    height_q16_ = height_q16;

    // Clamp height to the sphere surface.
    if (height_q16_ >  radius_q16_) height_q16_ =  radius_q16_;
    if (height_q16_ < -radius_q16_) height_q16_ = -radius_q16_;

    int64_t r_sq   = (int64_t)radius_q16_ * radius_q16_;   // Q32.32
    int64_t h_sq   = (int64_t)height_q16_ * height_q16_;   // Q32.32
    int64_t rxz_sq = r_sq - h_sq;                           // Q32.32
    if (rxz_sq < 0) rxz_sq = 0;
    int32_t r_xz = (int32_t)isqrt32((int32_t)(rxz_sq >> 32)) << 16;

    // Camera position.
    int32_t c = cos_deg_q16((int16_t)angle_deg_);
    int32_t s = sin_deg_q16((int16_t)angle_deg_);

    ivec3_t orbit_center = {
        (int32_t)(8LL << 16),
        0LL,
        (int32_t)(8LL<< 16) 
    };

    cam_.pos.x = orbit_center.x + fx_mul(r_xz, c);
    cam_.pos.y = orbit_center.y + height_q16_;
    cam_.pos.z = orbit_center.z + fx_mul(r_xz, s);

    cam_.target = orbit_center;
}

// ── radius / zoom ─────────────────────────────────────────────

void Ploter::set_radius(int32_t radius_q16)
{
    if (radius_q16 < (1LL << 16)) radius_q16 = (1LL << 16);
    radius_q16_ = radius_q16;
    set_orbit(angle_deg_, height_q16_);
}

void Ploter::zoom_in(int32_t delta_q16)
{
    step_q16_ -= delta_q16;
    if (step_q16_ < 100)
        step_q16_ = 100;
    radius_q16_ = auto_radius();
    refresh_grid();
}

void Ploter::zoom_out(int32_t delta_q16)
{
    step_q16_ += delta_q16;
    radius_q16_ = auto_radius();
    refresh_grid();
}

void Ploter::set_center(ivec3_t center_q16)
{
    center_ = center_q16;
    set_orbit(angle_deg_, height_q16_);
    refresh_grid();
}

void Ploter::refresh_grid()
{
    // Fill grid points (x,z from grid, y from function)
    for (uint8_t iz = 0; iz < samples_y_; ++iz) {
        int32_t wz = center_.z + fx_mul(step_q16_, (int32_t)iz << 16);
        for (uint8_t ix = 0; ix < samples_x_; ++ix) {
            int32_t wx = center_.x + fx_mul(step_q16_, (int32_t)ix << 16);
            int32_t wy = fx_mul(y_axis_scale, function_->get_value(wx, wz));
            grid_points_[iz * samples_x_ + ix] = { (((int32_t)ix) << 16), wy, (((int32_t)iz) << 16) };
        }
    }

    // Auto-centre Y at the midpoint of the function's value range.
    int32_t f_min = grid_points_[0].y;
    int32_t f_max = grid_points_[0].y;
    uint16_t n = (uint16_t)samples_x_ * samples_y_;
    for (uint16_t i = 1; i < n; ++i) {
        int32_t y = grid_points_[i].y;
        if (y < f_min) f_min = y;
        if (y > f_max) f_max = y;
    }
    center_.y = (f_min >> 1) + (f_max >> 1);
    center_.y = 0;

    set_orbit(angle_deg_, height_q16_);
}

void Ploter::draw_edge(UTFT &myGLCD,
                       const ivec3_t &a, const ivec3_t &b,
                       const mat3_t &basis) const
{
    int16_t ax, ay, bx, by;
    if (!project_point(a, cam_, basis, SCREEN_CX, SCREEN_CY, SCREEN_FOCAL, ax, ay)) return;
    if (!project_point(b, cam_, basis, SCREEN_CX, SCREEN_CY, SCREEN_FOCAL, bx, by)) return;

    int32_t x0 = ax, y0 = ay;
    int32_t x1 = bx, y1 = by;

    if (clip_line(x0, y0, x1, y1)) {
        myGLCD.drawLine((int)x0, (int)y0, (int)x1, (int)y1);
    }
}

void Ploter::draw(UTFT &myGLCD)
{
    mat3_t basis = look_at_q16(cam_);

    for (uint8_t iz = 0; iz < samples_y_; ++iz) {
        for (uint8_t ix = 0; ix < samples_x_; ++ix) {
            ivec3_t &p = grid_points_[iz * samples_x_ + ix];

            if (ix + 1 < samples_x_) {
                draw_edge_fast(myGLCD, p, grid_points_[iz * samples_x_ + (ix + 1)], basis);
            }
            if (iz + 1 < samples_y_) {
                draw_edge_fast(myGLCD, p, grid_points_[(iz + 1) * samples_x_ + ix], basis);
            }
        }
    }

    draw_axes(myGLCD, basis);
    myGLCD.setColor(255, 255, 255);   // restore default wire colour
}

void Ploter::draw_edge_fast(UTFT &myGLCD,
                       const ivec3_t &a, const ivec3_t &b,
                       const mat3_t &basis) const
{
    // ---- inline project_point for A ----
    int32_t rx = a.x - cam_.pos.x;
    int32_t ry = a.y - cam_.pos.y;
    int32_t rz = a.z - cam_.pos.z;

    int32_t vx = (int32_t)((((int64_t)basis.m[0][0] * rx) +
                            ((int64_t)basis.m[1][0] * ry) +
                            ((int64_t)basis.m[2][0] * rz)) >> 16);
    int32_t vy = (int32_t)((((int64_t)basis.m[0][1] * rx) +
                            ((int64_t)basis.m[1][1] * ry) +
                            ((int64_t)basis.m[2][1] * rz)) >> 16);
    int32_t vz = (int32_t)((((int64_t)basis.m[0][2] * rx) +
                            ((int64_t)basis.m[1][2] * ry) +
                            ((int64_t)basis.m[2][2] * rz)) >> 16);

    if (vz <= 0) return;

    int32_t tx = (int32_t)(((int64_t)vx << 16) / vz);
    int32_t ty = (int32_t)(((int64_t)vy << 16) / vz);
    int32_t x0 = (int32_t)SCREEN_CX + (((int32_t)SCREEN_FOCAL * tx) >> 16);
    int32_t y0 = (int32_t)SCREEN_CY - (((int32_t)SCREEN_FOCAL * ty) >> 16);

    // ---- inline project_point for B ----
    rx = b.x - cam_.pos.x;
    ry = b.y - cam_.pos.y;
    rz = b.z - cam_.pos.z;

    vx = (int32_t)((((int64_t)basis.m[0][0] * rx) +
                    ((int64_t)basis.m[1][0] * ry) +
                    ((int64_t)basis.m[2][0] * rz)) >> 16);
    vy = (int32_t)((((int64_t)basis.m[0][1] * rx) +
                    ((int64_t)basis.m[1][1] * ry) +
                    ((int64_t)basis.m[2][1] * rz)) >> 16);
    vz = (int32_t)((((int64_t)basis.m[0][2] * rx) +
                    ((int64_t)basis.m[1][2] * ry) +
                    ((int64_t)basis.m[2][2] * rz)) >> 16);

    if (vz <= 0) return;

    tx = (int32_t)(((int64_t)vx << 16) / vz);
    ty = (int32_t)(((int64_t)vy << 16) / vz);
    int32_t x1 = (int32_t)SCREEN_CX + (((int32_t)SCREEN_FOCAL * tx) >> 16);
    int32_t y1 = (int32_t)SCREEN_CY - (((int32_t)SCREEN_FOCAL * ty) >> 16);

    // Fast reject: discard any line with endpoints outside the screen
    if (x0 < 0 || x0 >= SCREEN_W || y0 < 0 || y0 >= SCREEN_H) return;
    if (x1 < 0 || x1 >= SCREEN_W || y1 < 0 || y1 >= SCREEN_H) return;

    myGLCD.drawLine((int)x0, (int)y0, (int)x1, (int)y1);
}

void Ploter::draw_fast(UTFT &myGLCD)
{
    mat3_t basis = look_at_q16(cam_);

    // Project each grid point once into screen space
    uint16_t n = (uint16_t)samples_x_ * samples_y_;
    for (uint16_t i = 0; i < n; ++i) {
        const ivec3_t &p = grid_points_[i];

        int32_t rx = p.x - cam_.pos.x;
        int32_t ry = p.y - cam_.pos.y;
        int32_t rz = p.z - cam_.pos.z;

        int32_t vx = (int32_t)((((int64_t)basis.m[0][0] * rx) +
                                ((int64_t)basis.m[1][0] * ry) +
                                ((int64_t)basis.m[2][0] * rz)) >> 16);
        int32_t vy = (int32_t)((((int64_t)basis.m[0][1] * rx) +
                                ((int64_t)basis.m[1][1] * ry) +
                                ((int64_t)basis.m[2][1] * rz)) >> 16);
        int32_t vz = (int32_t)((((int64_t)basis.m[0][2] * rx) +
                                ((int64_t)basis.m[1][2] * ry) +
                                ((int64_t)basis.m[2][2] * rz)) >> 16);

        if (vz <= 0) {
            screen_visible_[i] = 0;
            continue;
        }

        int32_t tx = (int32_t)(((int64_t)vx << 16) / vz);
        int32_t ty = (int32_t)(((int64_t)vy << 16) / vz);

        screen_points_[i].x = (int32_t)SCREEN_CX + (((int32_t)SCREEN_FOCAL * tx) >> 16);
        screen_points_[i].y = (int32_t)SCREEN_CY - (((int32_t)SCREEN_FOCAL * ty) >> 16);
        screen_visible_[i] = 1;
    }

    // Draw edges using cached projection (fast reject: endpoint outside screen)
    for (uint8_t iz = 0; iz < samples_y_; ++iz) {
        for (uint8_t ix = 0; ix < samples_x_; ++ix) {
            uint16_t i = (uint16_t)iz * samples_x_ + ix;

            if (ix + 1 < samples_x_) {
                uint16_t j = i + 1;
                if (screen_visible_[i] && screen_visible_[j]) {
                    int32_t x0 = screen_points_[i].x;
                    int32_t y0 = screen_points_[i].y;
                    int32_t x1 = screen_points_[j].x;
                    int32_t y1 = screen_points_[j].y;

                    if (x0 >= 0 && x0 < SCREEN_W && y0 >= 0 && y0 < SCREEN_H &&
                        x1 >= 0 && x1 < SCREEN_W && y1 >= 0 && y1 < SCREEN_H) {
                        myGLCD.drawLine((int)x0, (int)y0, (int)x1, (int)y1);
                    }
                }
            }

            if (iz + 1 < samples_y_) {
                uint16_t j = i + samples_x_;
                if (screen_visible_[i] && screen_visible_[j]) {
                    int32_t x0 = screen_points_[i].x;
                    int32_t y0 = screen_points_[i].y;
                    int32_t x1 = screen_points_[j].x;
                    int32_t y1 = screen_points_[j].y;

                    if (x0 >= 0 && x0 < SCREEN_W && y0 >= 0 && y0 < SCREEN_H &&
                        x1 >= 0 && x1 < SCREEN_W && y1 >= 0 && y1 < SCREEN_H) {
                        myGLCD.drawLine((int)x0, (int)y0, (int)x1, (int)y1);
                    }
                }
            }
        }
    }

    draw_axes(myGLCD, basis);
    myGLCD.setColor(255, 255, 255);   // restore default wire colour
}

void Ploter::draw_axes(UTFT &myGLCD, const mat3_t &basis) const
{
    ivec3_t org;
    org.x = 0LL-(1LL << 16);
    org.y = 0LL;
    org.z = 0LL-(1LL << 16);

    int32_t len = (16LL << 16);

    ivec3_t tip_x = { org.x + len, org.y,       org.z       };
    ivec3_t tip_y = { org.x,       org.y + len, org.z       };
    ivec3_t tip_z = { org.x,       org.y,       org.z + len };

    myGLCD.setColor(255, 0, 0);
    draw_edge(myGLCD, org, tip_x, basis);

    myGLCD.setColor(0, 255, 0);
    draw_edge(myGLCD, org, tip_y, basis);

    myGLCD.setColor(0, 0, 255);
    draw_edge(myGLCD, org, tip_z, basis);

    draw_ui(myGLCD);
}

void Ploter::draw_ui(UTFT &myGLCD) const
{
    ivec3_t org;
    org.x = fx_mul(center_.x, step_q16_);
    org.y = center_.y;
    org.z = fx_mul(center_.z, step_q16_);

    char buf[64];

    myGLCD.setColor(255, 255, 255);
    myGLCD.setBackColor(0, 0, 0);

    int32_t ax = org.x;
    int32_t ay = org.y;
    int32_t az = org.z;

    bool nx = ax < 0;
    bool ny = ay < 0;
    bool nz = az < 0;

    if (nx) ax = -ax;
    if (ny) ay = -ay;
    if (nz) az = -az;

    long ox_whole = ax >> 16;
    long oy_whole = ay >> 16;
    long oz_whole = az >> 16;

    long ox_frac = (((uint32_t)(ax & 0xFFFF)) * 100UL) >> 16;
    long oy_frac = (((uint32_t)(ay & 0xFFFF)) * 100UL) >> 16;
    long oz_frac = (((uint32_t)(az & 0xFFFF)) * 100UL) >> 16;

    sprintf(
        buf,
        "ORG:(%s%ld.%02ld,%s%ld.%02ld,%s%ld.%02ld)",
        nx ? "-" : "", ox_whole, ox_frac,
        ny ? "-" : "", oy_whole, oy_frac,
        nz ? "-" : "", oz_whole, oz_frac
    );
    myGLCD.print(buf, 0, 0);

    // ---- Cell size with 4 digits after decimal point ----

    long step_whole = step_q16_ >> 16;
    long step_frac =
        (((uint32_t)(step_q16_ & 0xFFFF)) * 10000UL) >> 16;

    sprintf(
        buf,
        "CELL:%ld.%04ld",
        step_whole,
        step_frac
    );
    myGLCD.print(buf, 0, 12);

    // ----y_axis_scale with 4 digits after decimal point ----
    long y_axis_scale_whole = y_axis_scale >> 16;
    long y_axis_scale_frac =
        (((uint32_t)(y_axis_scale & 0xFFFF)) * 10000UL) >> 16;

    sprintf(
        buf,
        "Z SCALE:%ld.%04ld",
        y_axis_scale_whole,
        y_axis_scale_frac
    );
    myGLCD.print(buf, 0, 24);
}

void Ploter::add_to_y_axis_scale(int32_t add) {
    y_axis_scale += add;
    if (y_axis_scale < 100) {
        y_axis_scale = 100;
    }
    refresh_grid();
}