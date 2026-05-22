#include "Ploter.h"
#include <stdlib.h>   // malloc / free

// ── helpers ──────────────────────────────────────────────────

static inline int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }
static inline int32_t imax32(int32_t a, int32_t b) { return a > b ? a : b; }

// Integer square-root of a non-negative 32-bit value (result is floor).
static int32_t isqrt32(int32_t v)
{
    if (v <= 0) return 0;
    int32_t r = v, r1 = (v + 1) >> 1;
    while (r1 < r) { r = r1; r1 = (r + v / r) >> 1; }
    return r;
}

// ── constructor / destructor ──────────────────────────────────

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

    // Auto step: aim for ~80 world-units total grid width along X.
    // 80 world-units = 80 << 16 in Q16.16.
    if (step_q16_ == 0) {
        step_q16_ = (int32_t)(0.35 * 65536.0 + 0.5);
        //step_q16_ = estimate_step_q16();
    }

    // Default radius so the full grid diagonal fits in view.
    radius_q16_ = auto_radius();

    // Default height: 45° elevation (r * sin(45°) ≈ r * 0.707).
    height_q16_ = fx_mul(radius_q16_, sin_deg_q16(45));

    // Initial camera placement.
    set_orbit(angle_deg_, height_q16_);

    // Fill value grid.
    refresh_grid();
}

Ploter::~Ploter()
{
    free(grid_points_);
}

static const int32_t PI_Q16 = 205887; // 3.1415926 * 65536

// ── auto-radius ───────────────────────────────────────────────
// The grid spans grid_w = (samples_x-1)*step along X
//              grid_d = (samples_y-1)*step along Z.
// Half-diagonal = sqrt(grid_w²+grid_d²)/2.
// We place the camera so the half-diagonal subtends ~30° from the camera,
// which means radius ≈ half_diagonal / tan(30°) ≈ half_diagonal * 1.73.
// This gives comfortable framing with the default 45° elevation.
int32_t Ploter::auto_radius() const
{
    // grid width/depth in Q16.16
    int32_t gw = fx_mul(step_q16_, (int32_t)(samples_x_ - 1) << 16);
    int32_t gd = fx_mul(step_q16_, (int32_t)(samples_y_ - 1) << 16);

    // half-diagonal in Q16.16:  hd = sqrt(gw^2 + gd^2) / 2
    // gw, gd are Q16.16 → gw*gw is Q32.32 → shift right 32 to get integer part
    int64_t hd_sq_int = (((int64_t)gw * gw) + ((int64_t)gd * gd)) >> 34; // /4 then >>32
    int32_t hd_int    = isqrt32((int32_t)hd_sq_int);   // plain integer world-units
    int32_t hd_q16    = hd_int << 16;                  // back to Q16.16

    // radius = hd * sqrt(3)
    const int32_t SQRT3_Q16 = 113512; // 1.7320508 * 65536
    return fx_mul(hd_q16, SQRT3_Q16);
}

// ── set_orbit ─────────────────────────────────────────────────
// Moves the camera to a point on the sphere of radius_q16_ centred at center_,
// at azimuth angle_deg (around Y) and the given elevation height_q16.
//
// Sphere parameterisation:
//   y    = height_q16  (clamped to [-r, r])
//   r_xz = sqrt(r² - y²)      (horizontal projection of radius)
//   x    = center.x + r_xz * cos(angle)
//   z    = center.z + r_xz * sin(angle)
void Ploter::set_orbit(uint16_t angle_deg, int32_t height_q16)
{
    angle_deg_  = angle_deg % 360;
    height_q16_ = height_q16;

    // Clamp height to the sphere surface.
    if (height_q16_ >  radius_q16_) height_q16_ =  radius_q16_;
    if (height_q16_ < -radius_q16_) height_q16_ = -radius_q16_;

    // Horizontal radius: r_xz = sqrt(R² - h²)
    // All in Q16.16.  r_xz² is in Q32.32 so shift right 16 before sqrt.
    // R and h are Q16.16 → R*R is Q32.32 → need to work carefully
    int64_t r_sq   = (int64_t)radius_q16_ * radius_q16_;   // Q32.32
    int64_t h_sq   = (int64_t)height_q16_ * height_q16_;   // Q32.32
    int64_t rxz_sq = r_sq - h_sq;                           // Q32.32
    if (rxz_sq < 0) rxz_sq = 0;
    // isqrt32 of the integer part, then put back in Q16.16
    int32_t r_xz = (int32_t)isqrt32((int32_t)(rxz_sq >> 32)) << 16;

    // Camera position.
    int32_t c = cos_deg_q16((int16_t)angle_deg_);
    int32_t s = sin_deg_q16((int16_t)angle_deg_);

    cam_.pos.x = center_.x + fx_mul(r_xz, c);
    cam_.pos.y = center_.y + height_q16_;
    cam_.pos.z = center_.z + fx_mul(r_xz, s);

    // Camera always looks at center_.
    cam_.target = center_;
}

// ── radius / zoom ─────────────────────────────────────────────

void Ploter::set_radius(int32_t radius_q16)
{
    if (radius_q16 < (1LL << 16)) radius_q16 = (1LL << 16);  // min 1 world-unit
    radius_q16_ = radius_q16;
    set_orbit(angle_deg_, height_q16_);  // reposition camera
}

void Ploter::zoom_in(int32_t delta_q16)
{
    set_radius(radius_q16_ - delta_q16);
}

void Ploter::zoom_out(int32_t delta_q16)
{
    set_radius(radius_q16_ + delta_q16);
}

// ── set_center ────────────────────────────────────────────────

void Ploter::set_center(ivec3_t center_q16)
{
    center_ = center_q16;
    set_orbit(angle_deg_, height_q16_);  // reposition camera around new pivot
    refresh_grid();
}

// ── refresh_grid ──────────────────────────────────────────────
// Grid origin (sample [0,0]) is at:
//   world_x = center_.x - half_width
//   world_z = center_.z - half_depth
// Sample [ix, iz] is at:
//   world_x = origin_x + ix * step_q16_
//   world_z = origin_z + iz * step_q16_
//   world_y = PlotFunction::get_value(world_x, world_z)

void Ploter::refresh_grid()
{
    int32_t half_w = fx_mul(step_q16_, (int32_t)(samples_x_ - 1) << 16) >> 1;
    int32_t half_d = fx_mul(step_q16_, (int32_t)(samples_y_ - 1) << 16) >> 1;

    int32_t origin_x = center_.x - half_w;
    int32_t origin_z = center_.z - half_d;

    // Fill grid points (x,z from grid, y from function)
    for (uint8_t iz = 0; iz < samples_y_; ++iz) {
        int32_t wz = origin_z + fx_mul(step_q16_, (int32_t)iz << 16);
        for (uint8_t ix = 0; ix < samples_x_; ++ix) {
            int32_t wx = origin_x + fx_mul(step_q16_, (int32_t)ix << 16);
            int32_t wy = function_->get_value(wx, wz);
            grid_points_[iz * samples_x_ + ix] = { wx, wy, wz };
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

// ── draw_edge ─────────────────────────────────────────────────
// Projects two world-space vertices and draws the connecting segment
// after Cohen-Sutherland clipping to the display bounds.

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

// ── draw ──────────────────────────────────────────────────────
// Draws the full grid as a wire mesh.
// Horizontal lines (constant Z): connect (ix, iz) → (ix+1, iz).
// Vertical   lines (constant X): connect (ix, iz) → (ix, iz+1).


void Ploter::draw(UTFT &myGLCD)
{
    mat3_t basis = look_at_q16(cam_);

    for (uint8_t iz = 0; iz < samples_y_; ++iz) {
        for (uint8_t ix = 0; ix < samples_x_; ++ix) {
            ivec3_t &p = grid_points_[iz * samples_x_ + ix];

            if (ix + 1 < samples_x_) {
                draw_edge(myGLCD, p, grid_points_[iz * samples_x_ + (ix + 1)], basis);
            }
            if (iz + 1 < samples_y_) {
                draw_edge(myGLCD, p, grid_points_[(iz + 1) * samples_x_ + ix], basis);
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
    int32_t half_w = fx_mul(step_q16_, (int32_t)(samples_x_ - 1) << 16) >> 1;
    int32_t half_d = fx_mul(step_q16_, (int32_t)(samples_y_ - 1) << 16) >> 1;

    // Origin: one cell outward from the +X / +Z corner of the grid.
    // All coordinates are Q16.16; plain addition is correct.
    ivec3_t org;
    org.x = center_.x - half_w - (step_q16_ >> 1);
    org.y = center_.y;
    org.z = center_.z - half_d - (step_q16_ >> 1);

    // Arm length = 2 grid steps (Q16.16 + Q16.16 is still Q16.16).
    int32_t len = fx_mul(step_q16_, (12LL << 16)) + (1LL << 16);

    ivec3_t tip_x = { org.x + len, org.y,       org.z       };  // +X
    ivec3_t tip_y = { org.x,       org.y + len, org.z       };  // +Y  ← was missing + len
    ivec3_t tip_z = { org.x,       org.y,       org.z + len };  // +Z

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
    int32_t half_w = fx_mul(step_q16_, (int32_t)(samples_x_ - 1) << 16) >> 1;
    int32_t half_d = fx_mul(step_q16_, (int32_t)(samples_y_ - 1) << 16) >> 1;

    ivec3_t org;
    org.x = center_.x - half_w - (step_q16_ >> 1);
    org.y = center_.y;
    org.z = center_.z - half_d - (step_q16_ >> 1);

    char buf[64];

    myGLCD.setColor(255, 255, 255);
    myGLCD.setBackColor(0, 0, 0);

    // ---- Origin with 2 digits after decimal point ----

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
}