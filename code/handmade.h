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
#include "hanmade_math.h"
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

struct Entity {
    bool exist;
    Tilemap_Position position;
    Vector2 velocity; // @Note Or you can say: dPosition.
    
    u32 orientation; // Facing direction.

    // @Note Properties below are in meters.
    real32 height;
    real32 width;
};

struct Game_State {
    World* world;
    Memory_Arena memory_arena;
    Tilemap_Position camera_position;
    Loaded_Bitmap background;
    Hero_Bitmap hero_bitmaps[4];

    u32 camera_following_entity_index;

    u32 player_index_for_controller[get_array_size(((Game_Input*) 0)->controllers)];
    u32 entity_count;
    Entity entities[256];
};

#define HANDMADE_H
#endif