#include "handmade.h"
#include "handmade_intrinsics.h"

#define NUM_TILEMAP_COLS 17
#define NUM_TILEMAP_ROWS 9

#define NUM_WORLD_COLS 2
#define NUM_WORLD_ROWS 2

#define TILE_CHUNK_DIM 256

// @Note We shouldn't setup the game world by something global since we are unloading and loading this lib quite often...
global Game_State *global_game_state;

void canonicalize_coord(World* world, u32* coord, real32* offset) {
    s32 tile_offset = floor_real32_to_s32(*offset / world->tile_side_in_meters);
    *coord += tile_offset;
    *offset -= (real32)tile_offset * world->tile_side_in_meters;
}

void canonicalize_position(World* world, World_Position* pos) {
    canonicalize_coord(world, &pos->x, &pos->offset_x);
    canonicalize_coord(world, &pos->y, &pos->offset_y);
}

u32 get_tile_value(World* world, Tile_Chunk* chunk, u32 x, u32 y) {
    u32 val = chunk->tiles[y * world->chunk_dim + x];
    return val;
}

Tile_Chunk* get_tile_chunk(World* world, u32 x, u32 y) {
    Tile_Chunk* result = NULL;
    if (x < world->num_world_cols && y < world->num_world_rows) {
        result = &world->tile_chunk[y * world->chunk_dim + y];        
    }
    return result;
}

Tile_Position get_tile_position(World* world, World_Position* pos) {
    Tile_Position result;

    result.chunk_x = pos->x >> world->chunk_shift;
    result.chunk_y = pos->y >> world->chunk_shift;

    result.tile_x = pos->x & world->chunk_mask;
    result.tile_y = pos->y & world->chunk_mask;

    return result;
}

bool is_world_point_empty(World* world, World_Position* world_pos) {
    Tile_Position tile_pos = get_tile_position(world, world_pos);
    Tile_Chunk* tilemap = get_tile_chunk(world, tile_pos.chunk_x, tile_pos.chunk_y);

    if (tilemap) {
        // @Note Since we use unsigned value to represent the tile_x and tile_y, there's no need to check it's negative.
        if (tile_pos.tile_x >= world->num_tilemap_cols || tile_pos.tile_y >= world->num_tilemap_rows) {
            return false;
        }

        u32 val = get_tile_value(world, tilemap, tile_pos.tile_x, tile_pos.tile_y);
        
        if (val != 1) {
            return true;
        }
    }

    return false;
}

internal void Output_Sound(Game_Sound_Buffer *buffer) {}

internal void Handle_Game_Input(Game_Input *input, World* world) {
    for (s32 i = 0; i < 5; i++) {
        Game_Controller_Input controller = input->controllers[i];
        real32 dx = 0.0f, dy = 0.0f;
        if (controller.move_up.ended_down)    dy =  1.0f;
        if (controller.move_down.ended_down)  dy = -1.0f;
        if (controller.move_right.ended_down) dx =  1.0f;
        if (controller.move_left.ended_down)  dx = -1.0f;

        real32 dt = input->dt_per_frame;
        dt *= 5.0f; // @Note Speed.

        World_Position new_center = global_game_state->hero_position;
        new_center.offset_x += dt * dx;
        new_center.offset_y += dt * dy;

        real32 player_width = 0.75f * world->tile_side_in_meters; // @Todo Extract it somewhere.

        World_Position new_left = new_center;
        new_left.offset_x -= 0.5f * player_width;
        
        World_Position new_right = new_center;
        new_right.offset_x += 0.5f * player_width;

        canonicalize_position(world, &new_center);
        canonicalize_position(world, &new_left);
        canonicalize_position(world, &new_right);

        if (is_world_point_empty(world, &new_center)
            && is_world_point_empty(world, &new_left)
            && is_world_point_empty(world, &new_right)) {
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
    u32 tiles[TILE_CHUNK_DIM][TILE_CHUNK_DIM] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    };

    { // @Note Setup game world data.
        world.num_tilemap_cols = NUM_TILEMAP_COLS;
        world.num_tilemap_rows = NUM_TILEMAP_ROWS;

        world.num_world_cols = NUM_WORLD_COLS;
        world.num_world_rows = NUM_WORLD_ROWS;

        world.chunk_dim = TILE_CHUNK_DIM;

        world.chunk_mask = 0xFF; // @Note 8 bits tile pos.
        world.chunk_shift = 8;

        world.tile_side_in_meters = 1.4f; // @Note Avg 10yo child height.
        world.tile_side_in_pixels = 60;
        world.meters_to_pixels = (real32)world.tile_side_in_pixels/world.tile_side_in_meters;

        // @Note lower left x/y are measured in pixels.
        world.lower_left_x = -30.0f;
        world.lower_left_y = (real32)back_buffer->height;

        Tile_Chunk tile_chunk = {};
        tile_chunk.tiles = (u32*)tiles;
        world.tile_chunk = &tile_chunk;
    }

    if (!memory->is_initialized) {
        memory->is_initialized = true;
        { // @Note Setup hero game state.
            World_Position* init_pos = &global_game_state->hero_position;
            init_pos->x = 3;
            init_pos->y = 3;

            init_pos->offset_x = 0.0f;
            init_pos->offset_y = 0.0f;
        }
    }

    // Pinkish Debug BG
    Draw_Rectangle(back_buffer, 0.0f, (real32)back_buffer->width, 0.0f, (real32)back_buffer->height, 1.0, 0.0, 1.0f);

    Tile_Position hero_pos = get_tile_position(&world, &global_game_state->hero_position);
    { // @Note Draw world.
        Tile_Chunk* tile_chunk = get_tile_chunk(&world, hero_pos.chunk_x, hero_pos.chunk_y);
        for (u32 row = 0; row < world.num_tilemap_rows; row++) {
            for (u32 col = 0; col < world.num_tilemap_cols; col++) {
                real32 greyish = (get_tile_value(&world, tile_chunk, col, row) == 1) ? 1.0f : 0.5f;
                
                if (row == hero_pos.tile_y && col == hero_pos.tile_x) {
                    // @Note Debug draw hero's tile
                    greyish = 0.0f;
                }

                real32 l = world.lower_left_x + ((real32)col * world.tile_side_in_pixels);
                real32 r = l + world.tile_side_in_pixels;
                real32 t = world.lower_left_y - ((real32)row * world.tile_side_in_pixels);
                real32 b = t - world.tile_side_in_pixels;

                Draw_Rectangle(back_buffer, l, r, b, t, greyish, greyish, greyish);
            }
        }
    }

    { // @Note Draw hero.
        real32 R = 1.0f, G = 0.0f, B = 0.0f;
        real32 hero_x = world.lower_left_x + (hero_pos.tile_x * world.tile_side_in_pixels + global_game_state->hero_position.offset_x * world.meters_to_pixels);
        real32 hero_y = world.lower_left_y - (hero_pos.tile_y * world.tile_side_in_pixels + global_game_state->hero_position.offset_y * world.meters_to_pixels);
        
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