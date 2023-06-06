#if !defined(HANDMADE_H)
#include "handmade_platform.h"
#include "handmade_intrinsics.h"

/*
 @todo
 Services that the platform provices to the game:
 - Timing
 - Controller/Keyboard Input
 - Bitmap Buffer
 - Sound Buffer
 - File I/O

 @note
 HANDMADE_INTERNAL:
  0 - Build for public
  1 - Build for developer only

 HANDMADE_SLOW
  0 - Not slow code allowed
  1 - Slow code welcome
 */
#include "math.h"

#include "handmade_tile.h"
struct World {
    Tilemap* tilemap;
};

// ===== Game Memory 'Stack' = ====
struct Memory_Arena {
    u64 size;
    u64 used;
    u8* base;
};

#define PUSH_STRUCT(arena, type) (type*) Push_(arena, sizeof(type))
#define PUSH_ARRAY(arena, count, type) (type*) Push_(arena, (count * sizeof(type)))
void* Push_(Memory_Arena* arena, u64 size) {
    assert(arena->used + size <= arena->size);

    u8* result = arena->base + arena->used;
    arena->used += size;
    return (void*)result;
}

internal void Init_Memory_Arena(Memory_Arena* arena, u64 size, u8* base) {
    arena->size = size;
    arena->used = 0;
    arena->base = base;
}

struct Loaded_Bitmap {
    u32 *pixels;
    s32 width;
    s32 height;
};

struct Hero_Bitmap {
  real32 align_x;
  real32 align_y;
  Loaded_Bitmap head;
  Loaded_Bitmap cape;
  Loaded_Bitmap torso;
};

struct Game_State {
    World* world;
    Memory_Arena memory_arena;
    Tilemap_Position hero_position;
    Tilemap_Position camera_position;

    Loaded_Bitmap background;

    u32 hero_orientation;
    Hero_Bitmap hero_bitmaps[4];
};

#define HANDMADE_H
#endif