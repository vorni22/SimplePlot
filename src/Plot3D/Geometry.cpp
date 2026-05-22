// Geometry.cpp
// All values in Q16.16 fixed-point unless noted otherwise.
// ONE  = 65536  (1.0 in Q16.16)

#include "Geometry.h"

#include <math.h>
#include <limits.h>

// ─────────────────────────────────────────────────────────────
// SCALAR
// ─────────────────────────────────────────────────────────────

// Q16.16 × Q16.16 → Q16.16   (uses 64-bit intermediate)
int32_t fx_mul(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * b) >> 16);
}

// Q16.16 / Q16.16 → Q16.16
int32_t fx_div(int32_t a, int32_t b)
{
    if (b == 0) return (a >= 0) ? INT32_MAX : INT32_MIN;
    return (int32_t)(((int64_t)a << 16) / b);
}

// Sine table: sin(0°)…sin(90°) in Q16.16, 1-degree steps (91 entries)
static const int32_t sin_table[91] = {
         0,   1143,   2287,   3429,   4571,   5711,   6850,   7986,
      9120,  10252,  11380,  12504,  13625,  14742,  15854,  16961,
     18064,  19160,  20251,  21336,  22414,  23486,  24550,  25606,
     26655,  27696,  28729,  29752,  30767,  31772,  32768,  33753,
     34728,  35693,  36647,  37589,  38521,  39440,  40347,  41243,
     42126,  42995,  43852,  44695,  45525,  46340,  47142,  47929,
     48702,  49460,  50203,  50931,  51643,  52339,  53020,  53684,
     54332,  54963,  55578,  56175,  56756,  57319,  57865,  58393,
     58903,  59395,  59870,  60326,  60763,  61183,  61583,  61965,
     62328,  62672,  62997,  63303,  63589,  63856,  64104,  64332,
     64540,  64729,  64898,  65047,  65176,  65286,  65376,  65446,
     65496,  65526,  65536
};

int32_t sin_deg_q16(int16_t deg)
{
    deg = ((deg % 360) + 360) % 360;   // normalise 0…359
    if (deg <= 90)  return  sin_table[deg];
    if (deg <= 180) return  sin_table[180 - deg];
    if (deg <= 270) return -sin_table[deg - 180];
    return -sin_table[360 - deg];
}

static int32_t clamp_q16(double value) {
    double scaled = value * 65536.0;
    if (scaled > (double)INT32_MAX) return INT32_MAX;
    if (scaled < (double)INT32_MIN) return INT32_MIN;
    return (int32_t)scaled;
}

int32_t cos_deg_q16(int16_t deg)
{
    return sin_deg_q16(deg + 90);
}

int32_t sin_rad_q16(int32_t rad_q16)
{
    double x = (double)rad_q16 / 65536.0;
    return clamp_q16(sin(x));
}

int32_t cos_rad_q16(int32_t rad_q16)
{
    double x = (double)rad_q16 / 65536.0;
    return clamp_q16(cos(x));
}

int32_t fx_exp_q16(int32_t x_q16)
{
    double x = (double)x_q16 / 65536.0;
    return clamp_q16(exp(x));
}

int32_t fx_ln_q16(int32_t x_q16)
{
    if (x_q16 <= 0) {
        return INT32_MIN;
    }
    double x = (double)x_q16 / 65536.0;
    return clamp_q16(log(x));
}

int32_t fx_pow_q16(int32_t base_q16, int32_t exp_q16)
{
    if (base_q16 <= 0) {
        return INT32_MIN;
    }
    double b = (double)base_q16 / 65536.0;
    double e = (double)exp_q16 / 65536.0;
    return clamp_q16(pow(b, e));
}

// ─────────────────────────────────────────────────────────────
// VECTOR
// ─────────────────────────────────────────────────────────────

ivec3_t vec_sub(ivec3_t a, ivec3_t b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

// Cross product of two Q16.16 vectors → result also Q16.16
// cross = (a × b), each component: a.y*b.z - a.z*b.y etc.
// To avoid overflow we shift each 64-bit product right by 16 bits.
ivec3_t vec_cross(ivec3_t a, ivec3_t b)
{
    ivec3_t r;
    r.x = (int32_t)((((int64_t)a.y * b.z) - ((int64_t)a.z * b.y)) >> 16);
    r.y = (int32_t)((((int64_t)a.z * b.x) - ((int64_t)a.x * b.z)) >> 16);
    r.z = (int32_t)((((int64_t)a.x * b.y) - ((int64_t)a.y * b.x)) >> 16);
    return r;
}

// Magnitude squared in Q32.32 (before shifting) → we only need Q16.16 length
// Uses Newton-Raphson sqrt on 64-bit.
static int32_t vec_len_q16(ivec3_t v)
{
    int64_t lsq = (int64_t)v.x*v.x + (int64_t)v.y*v.y + (int64_t)v.z*v.z; // Q32.32
    if (lsq <= 0) return 0;

    // Work entirely in Q32.32, take sqrt, result is Q16.16
    // sqrt(Q32.32) = Q16.16  ✓
    int64_t r = lsq >> 32;
    if (r == 0) r = 1;  // sub-unit: clamp, vec_norm will handle it
    int64_t s = r, s1 = (s + 1) >> 1;
    while (s1 < s) { s = s1; s1 = (s + r / s) >> 1; }
    return (int32_t)(s << 16);
}

// Returns unit vector (Q16.16) or zero vector if degenerate
ivec3_t vec_norm_q16(ivec3_t v)
{
    int32_t len = vec_len_q16(v);
    if (len < 16) return {0, 0, 0};
    ivec3_t r;
    r.x = fx_div(v.x, len);
    r.y = fx_div(v.y, len);
    r.z = fx_div(v.z, len);
    return r;
}

// ─────────────────────────────────────────────────────────────
// CAMERA BASIS  (right-handed, Y-up world)
// ─────────────────────────────────────────────────────────────
//
// Returns a 3×3 matrix whose columns are [right, up, forward],
// all unit vectors in Q16.16.
//   forward = normalize(target - pos)
//   right   = normalize(forward × world_up)
//   up      = right × forward        (already unit)
//
mat3_t look_at_q16(camera_t cam)
{
    ivec3_t fwd = vec_norm_q16(vec_sub(cam.target, cam.pos));

    // Gram-Schmidt: project world_up (0,1,0) onto the plane perpendicular to fwd
    // up_raw = world_up - dot(world_up, fwd) * fwd
    // dot(world_up=(0,1,0), fwd) = fwd.y
    int32_t d = fwd.y;
    ivec3_t up_raw;
    up_raw.x =              - fx_mul(d, fwd.x);
    up_raw.y = (1LL << 16)    - fx_mul(d, fwd.y);
    up_raw.z =              - fx_mul(d, fwd.z);

    // Degenerate case: camera looking straight up or down
    // Fall back to Z-up hint
    ivec3_t up;
    int32_t up_len_sq = fx_mul(up_raw.x, up_raw.x)
                      + fx_mul(up_raw.y, up_raw.y)
                      + fx_mul(up_raw.z, up_raw.z);
    if (up_len_sq < 256) {   // ~0.002 — essentially zero
        // Camera pointing straight up/down: use world Z as up hint
        // up_raw = (0,0,1) - dot((0,0,1), fwd)*fwd = -fwd.z*fwd, with z component = 1 - fwd.z²
        up_raw.x = -fx_mul(fwd.z, fwd.x);
        up_raw.y = -fx_mul(fwd.z, fwd.y);
        up_raw.z = (1LL << 16) - fx_mul(fwd.z, fwd.z);
    }
    up = vec_norm_q16(up_raw);

    // right = normalize(fwd × up)  — gives rightward screen vector
    ivec3_t right = vec_norm_q16(vec_cross(fwd, up));

    mat3_t m;
    // Column-major: columns are [right, up, fwd]
    m.m[0][0] = right.x;  m.m[0][1] = up.x;  m.m[0][2] = fwd.x;
    m.m[1][0] = right.y;  m.m[1][1] = up.y;  m.m[1][2] = fwd.y;
    m.m[2][0] = right.z;  m.m[2][1] = up.z;  m.m[2][2] = fwd.z;
    return m;
}

// ─────────────────────────────────────────────────────────────
// PROJECTION
// ─────────────────────────────────────────────────────────────
//
// Projects world-space point p (Q16.16) into screen pixel (sx, sy).
// Returns false if the point is behind the camera (should not be drawn).
//
// Pipeline:
//   1. Translate: p_rel = p - cam.pos
//   2. Rotate into view space using basis
//   3. Perspective divide:  sx = cx + focal * view.x / view.z
//
bool project_point(const ivec3_t &p, const camera_t &cam, const mat3_t &basis,
                   int16_t cx, int16_t cy, int16_t focal,
                   int16_t &sx, int16_t &sy)
{
    ivec3_t rel = vec_sub(p, cam.pos);

    int32_t vx = fx_mul(basis.m[0][0], rel.x) + fx_mul(basis.m[1][0], rel.y) + fx_mul(basis.m[2][0], rel.z);
    int32_t vy = fx_mul(basis.m[0][1], rel.x) + fx_mul(basis.m[1][1], rel.y) + fx_mul(basis.m[2][1], rel.z);
    int32_t vz = fx_mul(basis.m[0][2], rel.x) + fx_mul(basis.m[1][2], rel.y) + fx_mul(basis.m[2][2], rel.z);

    if (vz <= 0) return false;

    // Perspective divide: tan = v/vz (Q16.16), then scale by focal (pixels)
    int32_t tx = fx_div(vx, vz);
    int32_t ty = fx_div(vy, vz);
    sx = (int16_t)(cx + (((int32_t)focal * tx) >> 16));
    sy = (int16_t)(cy - (((int32_t)focal * ty) >> 16));
    return true;
}

// ─────────────────────────────────────────────────────────────
// Cohen-Sutherland line clipping
// ─────────────────────────────────────────────────────────────

#define CS_LEFT   1
#define CS_RIGHT  2
#define CS_BOTTOM 4
#define CS_TOP    8

static uint8_t cs_code(int32_t x, int32_t y, int32_t xmin, int32_t xmax, int32_t ymin, int32_t ymax)
{
    uint8_t c = 0;
    if (x < xmin) c |= CS_LEFT;
    if (x > xmax) c |= CS_RIGHT;
    if (y < ymin) c |= CS_TOP;
    if (y > ymax) c |= CS_BOTTOM;
    return c;
}

// Clips line (x0,y0)-(x1,y1) to [xmin,xmax]×[ymin,ymax].
// Returns true if any part of the line is visible; modifies coords in place.
bool clip_line(int32_t &x0, int32_t &y0, int32_t &x1, int32_t &y1)
{
    uint8_t c0 = cs_code(x0, y0, 0, SCREEN_W - 1, 0, SCREEN_H - 1);
    uint8_t c1 = cs_code(x1, y1, 0, SCREEN_W - 1, 0, SCREEN_H - 1);

    for (;;) {
        if (!(c0 | c1)) return true;
        if (  c0 & c1 ) return false;

        uint8_t c_out = c0 ? c0 : c1;
        int32_t x, y;
        int32_t dx = x1 - x0, dy = y1 - y0;

        if (c_out & CS_LEFT) {
            y = y0 + dy * (0 - x0) / dx;
            x = 0;
        } else if (c_out & CS_RIGHT) {
            y = y0 + dy * (SCREEN_W - 1 - x0) / dx;
            x = SCREEN_W - 1;
        } else if (c_out & CS_TOP) {
            x = x0 + dx * (0 - y0) / dy;
            y = 0;
        } else {
            x = x0 + dx * (SCREEN_H - 1 - y0) / dy;
            y = SCREEN_H - 1;
        }

        if (c_out == c0) { x0 = x; y0 = y; c0 = cs_code(x0, y0, 0, SCREEN_W-1, 0, SCREEN_H-1); }
        else             { x1 = x; y1 = y; c1 = cs_code(x1, y1, 0, SCREEN_W-1, 0, SCREEN_H-1); }
    }
}