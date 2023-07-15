#if !defined(HANDMADE_INTRINSICS_H)
#include "math.h"

real32 abs_real32(real32 value) {
    return fabsf(value); // fabs is for double...
}

u32 rotate_left(u32 value, u32 shift) {
    return _rotl(value, shift);
}

u32 rotate_right(u32 value, u32 shift) {
    return _rotr(value, shift);
}

s32 wrap_s32(s32 num, s32 lower_bound, s32 upper_bound) {
    // [lower, upper)

    s32 range = upper_bound - lower_bound;
    s32 offset = (num - lower_bound) % range;
    if (offset < 0) {
        return upper_bound + offset;
    }
    return lower_bound + offset;
}

s32 floor_real32_to_s32(real32 val) {
    s32 res = (s32)floorf(val);
    return res;
}

u32 round_real32_to_u32(real32 val) {
    u32 res = (u32)roundf(val);
    return res;
}

s32 round_real32_to_s32(real32 val) {
    s32 res = (s32)roundf(val);
    return res;
}

s32 clamp_s32(s32 val, s32 min, s32 max) {
    // [min, max)

    if (val < min) return min;
    if (val >= max) return max;
    
    return val;
}

struct Bit_Scan_Result {
    bool found;
    u32 index;
};

inline Bit_Scan_Result bit_scan_forward(u32 value) {
    Bit_Scan_Result result = {};

#if COMPILER_MSVC
    result.found = _BitScanForward((unsigned long*)&result.index, value);
#else
    for (u32 test = 0; test < 32; test++) {
        if ((value & (1 << test)) == 1) {
            result.found = true;
            result.index = test;
        }
    }
#endif
    return result;
}

#define HANDMADE_INTRINSICS_H
#endif 