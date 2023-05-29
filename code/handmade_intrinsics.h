#if !defined(HANDMADE_INTRINSICS_H)
#include "math.h"

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

#define HANDMADE_INTRINSICS_H
#endif 