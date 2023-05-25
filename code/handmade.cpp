#include "handmade.h"

#define NUM_TILEMAP_COLS 17
#define NUM_TILEMAP_ROWS 9

#define NUM_WORLD_COLS 2
#define NUM_WORLD_ROWS 2

global Game_State *global_game_state;
// @Note !!! We shouldn't setup by something global.
// Beacause we are unload and load this lib quiet often...

s32 wrap_s32(s32 num, s32 lower_bound, s32 upper_bound) {
    // @Note [lower, upper)

    s32 range = upper_bound - lower_bound;
    s32 offset = (num - lower_bound) % range;
    if (offset < 0) {
        return upper_bound + offset;
    }
    return lower_bound + offset;
}

#include "math.h"
s32 floor_real32_to_s32(real32 val) {
    s32 res = (s32)floorf(val);
    return res;
}

u32 round_real32_to_u32(real32 val) {
    u32 res = (u32)(val + 0.5f);
    return res;
}

s32 round_real32_to_s32(real32 val) {
    s32 res = (s32)(val + 0.5f);
    return res;
}

Canonical_Position get_canonical_position(World* world, Raw_Position raw) {
    s32 tilemap_x = raw.tilemap_x;
    s32 tilemap_y = raw.tilemap_y;

    real32 x = raw.x - world->upper_left_x;
    real32 y = raw.y - world->upper_left_y;

    s32 tile_x = floor_real32_to_s32(x / world->tile_width);
    s32 tile_y = floor_real32_to_s32(y / world->tile_height);

    real32 tile_offset_x = x - ((real32)tile_x * world->tile_width);
    real32 tile_offset_y = y - ((real32)tile_y * world->tile_height);

    if (tile_x < 0) {
        tile_x = wrap_s32(tile_x, 0, world->num_tilemap_cols);
        tilemap_x = wrap_s32(tilemap_x - 1, 0, world->num_world_cols);
    }
    
    if (tile_x >= world->num_tilemap_cols) {
        tile_x = wrap_s32(tile_x, 0, world->num_tilemap_cols);
        tilemap_x = wrap_s32(tilemap_x + 1, 0, world->num_world_cols);
    }

    if (tile_y < 0) {
        tile_y = wrap_s32(tile_y, 0, world->num_tilmap_rows);
        tilemap_y = wrap_s32(tilemap_y - 1, 0, world->num_world_rows);
    }

    if (tile_y >= world->num_tilmap_rows) {
        tile_y = wrap_s32(tile_y, 0, world->num_tilmap_rows);
        tilemap_y = wrap_s32(tilemap_y + 1, 0, world->num_world_rows);
    }

    Canonical_Position can = {};
    can.tilemap_x = tilemap_x;
    can.tilemap_y = tilemap_y;
    can.tile_x = tile_x;
    can.tile_y = tile_y;
    can.offset_x = tile_offset_x;
    can.offset_y = tile_offset_y;

    return can;
}

u32 get_tilemap_value(World* world, Tilemap* tilemap, s32 tile_x, s32 tile_y) {
    u32 val = tilemap->tiles[tile_y * world->num_tilemap_cols + tile_x];
    return val;
}

Tilemap* get_tilemap(World* world, s32 x, s32 y) {
    Tilemap* tilemap = NULL;
    if (x >= 0 && x < world->num_world_cols && y >= 0 && y < world->num_world_rows) {
        tilemap = &world->tilemaps[y * world->num_world_cols + x];        
    }
    return tilemap;
}

bool is_world_point_empty(World* world, Raw_Position raw) {
    Canonical_Position can = get_canonical_position(world, raw);
    Tilemap* tilemap = get_tilemap(world, can.tilemap_x, can.tilemap_y);

    if (!tilemap) return false;

    if (can.tile_x < 0 || can.tile_y < 0 
        || can.tile_x >= world->num_tilemap_cols
        || can.tile_y >= world->num_tilmap_rows) {
        return false;
    }

    u32 val = get_tilemap_value(world, tilemap, can.tile_x, can.tile_y);
    return val != 1; // @Fixme Don't mess up with magic numbers.
}

internal void Output_Sound(Game_Sound_Buffer *buffer) {}

internal void Handle_Game_Input(Game_Input *input, World* world) {
    for (s32 i = 0; i < 5; i++) {
        Game_Controller_Input controller = input->controllers[i];
        real32 dx = 0.0f, dy = 0.0f;
        if (controller.move_down.ended_down)  dy = 1.0f;
        if (controller.move_up.ended_down)    dy = -1.0f;
        if (controller.move_left.ended_down)  dx = -1.0f;
        if (controller.move_right.ended_down) dx = 1.0f;

        // dy = 1.0f;
        dx *= 128.0f, dy *= 128.0f; // @Note Amplification

        Game_State* gs = global_game_state;
        Tilemap *tilemap = get_tilemap(world, gs->tilemap_x, gs->tilemap_y);

        real32 dt = input->dt_per_frame;
        real32 new_hero_x = gs->hero_x + dt * dx;
        real32 new_hero_y = gs->hero_y + dt * dy;

        real32 player_width = 0.75f * world->tile_width; // @Todo Extract it somewhere.

        Raw_Position new_center;
        new_center.tilemap_x = gs->tilemap_x;
        new_center.tilemap_y = gs->tilemap_y;
        new_center.x = new_hero_x;
        new_center.y = new_hero_y;

        Raw_Position new_left = new_center;
        new_left.x -= 0.5f * player_width;
        
        Raw_Position new_right = new_center;
        new_right.x += 0.5f * player_width;

        if (is_world_point_empty(world, new_center)
            && is_world_point_empty(world, new_left)
            && is_world_point_empty(world, new_right)) {
            Canonical_Position can = get_canonical_position(world, new_center);

            gs->tilemap_x = can.tilemap_x;
            gs->tilemap_y = can.tilemap_y;
            gs->hero_x = world->upper_left_x + (real32)can.tile_x * world->tile_width + can.offset_x;
            gs->hero_y = world->upper_left_y + (real32)can.tile_y * world->tile_height + can.offset_y;
        }
    }
}

internal void Draw_Rectangle(Game_Back_Buffer *buffer, 
                             real32 l_, real32 r_, real32 b_, real32 t_,
                             real32 R, real32 G, real32 B) {
    s32 l = round_real32_to_u32(l_);
    s32 r = round_real32_to_u32(r_);
    s32 b = round_real32_to_u32(b_);
    s32 t = round_real32_to_u32(t_);

    if (l > r) {
        // swap(l, r); // @ImplementMe
    }
    if (b > t) {
        // swap(b, t); // @ImplementMe
    }

    if (l < 0) l = 0;
    if (r > buffer->width) r = buffer->width;
    if (b < 0) b = 0;
    if (t > buffer->height) t = buffer->height;

    // @Note ARGB
    u32 color = (round_real32_to_u32(R * 255.0f)) << 16
                | (round_real32_to_u32(G * 255.0f)) << 8
                | (round_real32_to_u32(B * 255.0f)) << 0;
    u8 *row = (u8 *)buffer->bitmap + l * buffer->bytes_per_pixel + b * buffer->pitch;
    for (s32 y = b; y < t; y++) {
        u32 *pixel = (u32 *)row;
        for (s32 x = l; x < r; x++) {
            *pixel++ = color; // @Note For now we choose not to blend the color by '|='
        }
        row += buffer->pitch;
    }
}

// @Note Function signare is define in handmade.h
extern "C" GAME_UPDATE_AND_RENDER(Game_Update_And_Render) {
    global_game_state = (Game_State *)memory->permanent_storage;

    World world = {};
    Tilemap tilemaps[NUM_WORLD_COLS * NUM_WORLD_ROWS];

    u32 tiles_0[NUM_TILEMAP_ROWS][NUM_TILEMAP_COLS] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0},
        {1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    u32 tiles_2[NUM_TILEMAP_ROWS][NUM_TILEMAP_COLS] = {
        {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    u32 tiles_1[NUM_TILEMAP_ROWS][NUM_TILEMAP_COLS] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    u32 tiles_3[NUM_TILEMAP_ROWS][NUM_TILEMAP_COLS] = {
        {1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    { // @Note Setup game world data.
        world.num_tilemap_cols = NUM_TILEMAP_COLS;
        world.num_tilmap_rows = NUM_TILEMAP_ROWS;
        world.num_world_cols = NUM_WORLD_COLS;
        world.num_world_rows = NUM_WORLD_ROWS;

        world.tile_width = 60;
        world.tile_height = 60;

        world.upper_left_x = -30.0f;
        world.upper_left_y =   0.0f;

        tilemaps[0].tiles = (u32*)tiles_0;  
        tilemaps[1].tiles = (u32*)tiles_1;
        tilemaps[2].tiles = (u32*)tiles_2;
        tilemaps[3].tiles = (u32*)tiles_3;
        world.tilemaps = tilemaps;
    }

    if (!memory->is_initialized) {
        memory->is_initialized = true;
        { // @Note Setup hero game state.
            global_game_state->tilemap_x = 0;
            global_game_state->tilemap_y = 0;

            // global_game_state->hero_x = 800;
            // global_game_state->hero_y = 210;
            global_game_state->hero_x = 240;
            global_game_state->hero_y = 400;
        }
    }

    // Pinkish Debug BG
    Draw_Rectangle(back_buffer, 0.0f, (real32)back_buffer->width, 0.0f, (real32)back_buffer->height, 1.0, 0.0, 1.0f);

    Tilemap* tilemap = get_tilemap(&world, global_game_state->tilemap_x, global_game_state->tilemap_y);
    { // @Note Draw tilemap
        for (s32 row = 0; row < world.num_tilmap_rows; row++) {
            for (s32 col = 0; col < world.num_tilemap_cols; col++) {
                real32 greyish = (get_tilemap_value(&world, tilemap, col, row) == 1) ? 1.0f : 0.5f;
                
                real32 l = world.upper_left_x + ((real32)col * world.tile_width);
                real32 r = l + world.tile_width;
                real32 b = world.upper_left_y + ((real32)row * world.tile_height);
                real32 t = b + world.tile_height;

                Draw_Rectangle(back_buffer, l, r, b, t, greyish, greyish, greyish);
            }
        }
    }

    { // @Note Draw hero.
        real32 R = 1.0f, G = 0.0f, B = 0.0f;
        real32 hero_x = global_game_state->hero_x;
        real32 hero_y = global_game_state->hero_y;
        
        real32 hero_width = 0.75f * world.tile_width;
        real32 hero_height = world.tile_height;
        
        real32 l = hero_x - 0.5f * hero_width;
        real32 r = hero_x + 0.5f * hero_width;
        real32 b = hero_y - 0.5f * hero_height;
        real32 t = hero_y + 0.5f * hero_height;
        Draw_Rectangle(back_buffer, l, r, b, t, R, G, B);
    }

    Handle_Game_Input(input, &world);
}

extern "C" GET_SOUND_SAMPLES(Get_Sound_Samples) { Output_Sound(sound_buffer); }