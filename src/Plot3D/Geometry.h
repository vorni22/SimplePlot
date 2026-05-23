#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <inttypes.h>

#define SCREEN_W  320
#define SCREEN_H  240
#define SCREEN_CX (SCREEN_W / 2)
#define SCREEN_CY (SCREEN_H / 2)
#define SCREEN_FOCAL (SCREEN_H / 2)

typedef struct { int32_t x, y;          } ivec2_t;
typedef struct { int32_t x, y, z;       } ivec3_t;
typedef struct { int32_t x, y, z, w;    } ivec4_t;
typedef struct { ivec3_t pos, target;   } camera_t;
typedef struct { int32_t m[3][3];       } mat3_t; 

// ── SCALAR ───────────────────────────────────────────────────
int32_t fx_mul(int32_t a, int32_t b);       // Q16.16 × Q16.16 → Q16.16
int32_t fx_div(int32_t a, int32_t b);       // Q16.16 / Q16.16 → Q16.16
int32_t sin_deg_q16(int16_t deg);           // → Q16.16  (1.0 = 65536)
int32_t cos_deg_q16(int16_t deg);           // → Q16.16
int32_t sin_rad_q16(int32_t rad_q16);        // → Q16.16 (radians in Q16.16)
int32_t cos_rad_q16(int32_t rad_q16);        // → Q16.16 (radians in Q16.16)
int32_t fx_exp_q16(int32_t x_q16);           // exp(x) in Q16.16
int32_t fx_ln_q16(int32_t x_q16);            // ln(x) in Q16.16
int32_t fx_pow_q16(int32_t base_q16, int32_t exp_q16); // base^exp in Q16.16

// ── VECTOR ───────────────────────────────────────────────────
ivec3_t vec_sub     (ivec3_t a, ivec3_t b);
ivec3_t vec_cross   (ivec3_t a, ivec3_t b);
ivec3_t vec_norm_q16(ivec3_t v);
mat3_t  look_at_q16 (camera_t cam);

bool project_point(const ivec3_t &p, const camera_t &cam, const mat3_t &basis,
                   int16_t cx, int16_t cy, int16_t focal,
                   int16_t &sx, int16_t &sy);

// Cohen-Sutherland clip. Modifies endpoints in place.
// Returns true if any segment remains after clipping.
bool clip_line(int32_t &x0, int32_t &y0, int32_t &x1, int32_t &y1);

#endif // GEOMETRY_H