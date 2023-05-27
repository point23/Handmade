#include "handmade.h"
#include "handmade_intrinsics.h"

#define NUM_TILEMAP_COLS 17
#define NUM_TILEMAP_ROWS 9

#define NUM_WORLD_COLS 2
#define NUM_WORLD_ROWS 2

// @Note We shouldn't setup the game world by something global since we are unloading and loading this lib quite often...
global Game_State *global_game_state;

void canonicalize_coord(World* world, s32 tilemap_count, s32 tile_count, s32* tilemap, s32* tile, real32* offset) {
    s32 tile_offset = floor_real32_to_s32(*offset / world->tile_side_in_meters);
    *tile += tile_offset;
    *offset -= (real32)tile_offset * world->tile_side_in_meters;

    if (*tile < 0) {
        *tile = wrap_s32(*tile, 0, tile_count);
        *tilemap = wrap_s32(*tilemap - 1, 0, tilemap_count);
    }
    
    if (*tile >= tile_count) {
        *tile = wrap_s32(*tile, 0, tile_count);
        *tilemap = wrap_s32(*tilemap + 1, 0, tilemap_count);
    }
}

void canonicalize_position(World* world, Canonical_Position* pos) {
    canonicalize_coord(world, world->num_world_cols, world->num_tilemap_cols, &pos->tilemap_x, &pos->tile_x, &pos->offset_x);
    canonicalize_coord(world, world->num_world_rows, world->num_tilemap_rows, &pos->tilemap_y, &pos->tile_y, &pos->offset_y);
}

u32 get_tilemap_value(World* world, Tilemap* tilemap, s32 tile_x, s32 tile_y) {
    u32 val = tilemap->tiles[tile_y * world->num_tilemap_cols + tile_x];
    return val;
}

Tilemap* get_tilemap(World* world, s32 tilemap_x, s32 tilemap_y) {
    Tilemap* tilemap = NULL;
    if (tilemap_x >= 0 && tilemap_x < world->num_world_cols && tilemap_y >= 0 && tilemap_y < world->num_world_rows) {
        tilemap = &world->tilemaps[tilemap_y * world->num_world_cols + tilemap_x];        
    }
    return tilemap;
}

bool is_world_point_empty(World* world, Canonical_Position pos) {
    Tilemap* tilemap = get_tilemap(world, pos.tilemap_x, pos.tilemap_y);

    if (!tilemap) return false;

    if (pos.tile_x < 0 || pos.tile_y < 0 
        || pos.tile_x >= world->num_tilemap_cols
        || pos.tile_y >= world->num_tilemap_rows) {
        return false;
    }

    u32 val = get_tilemap_value(world, tilemap, pos.tile_x, pos.tile_y);
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

        real32 dt = input->dt_per_frame;
        dt *= 5.0f; // @Note Speed.

        Canonical_Position new_center = global_game_state->hero_position;
        new_center.offset_x += dt * dx;
        new_center.offset_y += dt * dy;

        real32 player_width = 0.75f * world->tile_side_in_meters; // @Todo Extract it somewhere.

        Canonical_Position new_left = new_center;
        new_left.offset_x -= 0.5f * player_width;
        
        Canonical_Position new_right = new_center;
        new_right.offset_x += 0.5f * player_width;

        canonicalize_position(world, &new_center);
        canonicalize_position(world, &new_left);
        canonicalize_position(world, &new_right);

        if (is_world_point_empty(world, new_center)
            && is_world_point_empty(world, new_left)
            && is_world_point_empty(world, new_right)) {
            global_game_state->hero_position = new_center;
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
        world.num_tilemap_rows = NUM_TILEMAP_ROWS;
        world.num_world_cols = NUM_WORLD_COLS;
        world.num_world_rows = NUM_WORLD_ROWS;

        world.tile_side_in_meters = 1.4f; // @Note Avg 10yo child height.
        world.tile_side_in_pixels = 60;
        world.meters_to_pixels = (real32)world.tile_side_in_pixels/world.tile_side_in_meters;

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
            Canonical_Position* hero = &global_game_state->hero_position;
            hero->tilemap_x = 0;
            hero->tilemap_y = 0;
            
            hero->tile_x = 3;
            hero->tile_y = 3;

            hero->offset_x = 0.0f;
            hero->offset_y = 0.0f;
        }
    }

    // Pinkish Debug BG
    Draw_Rectangle(back_buffer, 0.0f, (real32)back_buffer->width, 0.0f, (real32)back_buffer->height, 1.0, 0.0, 1.0f);

    Tilemap* tilemap = get_tilemap(&world, global_game_state->hero_position.tilemap_x, global_game_state->hero_position.tilemap_y);
    { // @Note Draw tilemap
        for (s32 row = 0; row < world.num_tilemap_rows; row++) {
            for (s32 col = 0; col < world.num_tilemap_cols; col++) {
                real32 greyish = (get_tilemap_value(&world, tilemap, col, row) == 1) ? 1.0f : 0.5f;
                
                if (row == global_game_state->hero_position.tile_y && col == global_game_state->hero_position.tile_x) { // @Note Debug draw hero's tile
                    greyish = 0.0f;
                }

                real32 l = world.upper_left_x + ((real32)col * world.tile_side_in_pixels);
                real32 r = l + world.tile_side_in_pixels;
                real32 b = world.upper_left_y + ((real32)row * world.tile_side_in_pixels);
                real32 t = b + world.tile_side_in_pixels;

                Draw_Rectangle(back_buffer, l, r, b, t, greyish, greyish, greyish);
            }
        }
    }

    { // @Note Draw hero.
        real32 R = 1.0f, G = 0.0f, B = 0.0f;
        Canonical_Position hero_position = global_game_state->hero_position;

        real32 hero_x = world.upper_left_x + hero_position.tile_x * world.tile_side_in_pixels + hero_position.offset_x * world.meters_to_pixels;
        real32 hero_y = world.upper_left_y + hero_position.tile_y * world.tile_side_in_pixels + hero_position.offset_y * world.meters_to_pixels;
        
        real32 hero_width = 0.75f * (real32)world.tile_side_in_pixels;
        real32 hero_height = (real32)world.tile_side_in_pixels;
        
        real32 l = hero_x - 0.5f * hero_width;
        real32 r = hero_x + 0.5f * hero_width;
        real32 b = hero_y - 0.5f * hero_height;
        real32 t = hero_y + 0.5f * hero_height;
        Draw_Rectangle(back_buffer, l, r, b, t, R, G, B);
    }

    Handle_Game_Input(input, &world);
}

extern "C" GET_SOUND_SAMPLES(Get_Sound_Samples) { Output_Sound(sound_buffer); }