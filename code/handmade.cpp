#include "handmade.h"
#include "handmade_intrinsics.h"
#include "handmade_tile.h"
#include "handmade_tile.cpp"
#include "handmade_random.h"

#define NUM_TILEMAP_COLS 17
#define NUM_TILEMAP_ROWS 9

#define NUM_WORLD_COLS 4
#define NUM_WORLD_ROWS 4

#define TILE_CHUNK_DIM 256

// @Note We shouldn't setup the game world by something global since we are unloading and loading this lib quite often...
global Game_State *global_game_state;

internal void Output_Sound(Game_Sound_Buffer *buffer) {}

internal void Handle_Game_Input(Game_Input *input) {
    for (s32 i = 0; i < 5; i++) {
        Game_Controller_Input controller = input->controllers[i];
        real32 dx = 0.0f, dy = 0.0f;
        if (controller.move_up.ended_down)    dy =  1.0f;
        if (controller.move_down.ended_down)  dy = -1.0f;
        if (controller.move_right.ended_down) dx =  1.0f;
        if (controller.move_left.ended_down)  dx = -1.0f;

        real32 dt = input->dt_per_frame;
        real32 speed = 5.0f;
        if (controller.action_up.ended_down) speed = 30.0f;
        dt *= speed;

        Tilemap_Position new_center = global_game_state->hero_position;
        new_center.offset_x += dt * dx;
        new_center.offset_y += dt * dy;

        Tilemap* tilemap = global_game_state->world->tilemap;
        real32 player_width = 0.75f * tilemap->tile_side_in_meters; // @Todo Extract it somewhere.

        Tilemap_Position new_left = new_center;
        new_left.offset_x -= 0.5f * player_width;
        
        Tilemap_Position new_right = new_center;
        new_right.offset_x += 0.5f * player_width;

        canonicalize_position(tilemap, &new_center);
        canonicalize_position(tilemap, &new_left);
        canonicalize_position(tilemap, &new_right);

        if (is_point_empty(tilemap, &new_center)
            && is_point_empty(tilemap, &new_left)
            && is_point_empty(tilemap, &new_right)) {
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
  
    if (!memory->is_initialized) {
        memory->is_initialized = true;
        
        Memory_Arena* memory_arena = &global_game_state->memory_arena;
        u64 memory_used =  sizeof(Game_State);
        Init_Memory_Arena(memory_arena, memory->permanent_storage_size - memory_used, (u8*)memory->permanent_storage + memory_used);
        global_game_state->world = PUSH_STRUCT(memory_arena, World);
        World* world = global_game_state->world;
        world->tilemap = PUSH_STRUCT(memory_arena, Tilemap);
        Tilemap* tilemap = world->tilemap;

        { // @Note Setup hero game state.
            Tilemap_Position* init_pos = &global_game_state->hero_position;
            init_pos->x = 3;
            init_pos->y = 3;

            init_pos->offset_x = 0.0f;
            init_pos->offset_y = 0.0f;
        }

        { // @Note Setup tilemap data.
            tilemap->num_tilemap_cols = NUM_TILEMAP_COLS;
            tilemap->num_tilemap_rows = NUM_TILEMAP_ROWS;

            tilemap->num_world_cols = NUM_WORLD_COLS;
            tilemap->num_world_rows = NUM_WORLD_ROWS;

            tilemap->chunk_dim = TILE_CHUNK_DIM;
            tilemap->chunk_mask = 0xFF; // @Note 8 bits tile pos.
            tilemap->chunk_shift = 8;

            tilemap->tile_side_in_meters = 1.4f; // @Note Avg 10yo child height.
        
            // @Note lower left x/y are measured in pixels.
            tilemap->lower_left_x = -30.0f;
            tilemap->lower_left_y = (real32)back_buffer->height;

            u32 tile_chunk_count = tilemap->num_world_cols * tilemap->num_world_rows;
            tilemap->tile_chunks = PUSH_ARRAY(memory_arena, tile_chunk_count, Tile_Chunk);

            { // @Note Setup tile chunks.
                bool left_door = false, right_door = false, top_door = false, bottom_door = false;
                u32 screen_x = 0, screen_y = 0;
                u32 z = 0;
                u32 random_number_idx = 0;
  
                for (u32 screen = 0; screen < 100; screen++) {
                    u32 random_number = random_numbers[random_number_idx++];
                    u32 dir = random_number % 2;
                    // if(top_door || bottom_door) {
                        // dir = random_number % 2;
                    // } else {
                        // dir = random_number % 3;
                    // }

                    if(dir == 0) {
                        right_door = true;
                    } else if(dir == 1) {
                        top_door = true;
                    } else if(dir == 2) {
                        // if(z == 0) {
                            // top_door = true;
                        // } else if(z == 1) {
                            // bottom_door = true;
                        // }
                    }
                    
                    u32 num_tilemap_rows = tilemap->num_tilemap_rows;
                    u32 num_tilemap_cols = tilemap->num_tilemap_cols;
                    
                    for(u32 tile_y = 0; tile_y < num_tilemap_rows; tile_y++) {
                        for(u32 tile_x = 0; tile_x < num_tilemap_cols; tile_x++) {
                            u32 x = screen_x * num_tilemap_cols + tile_x;
                            u32 y = screen_y * num_tilemap_rows + tile_y;
                            u32 value = 1;
                            
                            if(tile_x == 0) {
                                value = 2;
                                if(left_door && tile_y == (num_tilemap_rows / 2)) {
                                    value = 1;
                                }
                            }

                            if(tile_x == (num_tilemap_cols - 1)) {
                                value = 2;
                                if(right_door && tile_y == (num_tilemap_rows / 2)) {
                                    value = 1;
                                }
                            }

                            if(tile_y == 0) {
                                value = 2;
                                if(bottom_door && tile_x == (num_tilemap_cols / 2)) {
                                    value = 1;
                                }
                            }
                            if(tile_y == (num_tilemap_rows - 1)) {
                                value = 2;
                                if(top_door && tile_x == (num_tilemap_cols / 2)) {
                                    value = 1;
                                }
                            }

                            set_tile_value(memory_arena, tilemap, x, y, value);
                        }
                    }

                    { // @Temproray
                        left_door = right_door;
                        bottom_door = top_door;
                        right_door = false;
                        top_door = false;
                    }

                    if(dir == 0) {
                        screen_x += 1;
                        // top_door = false;
                        // bottom_door = false;
                    } else if(dir == 1) {
                        screen_y += 1;
                        // top_door = false;
                        // bottom_door = false;
                    } else if(dir == 2) {
                        // if(top_door) {
                            // bottom_door = true;
                            // top_door = false;
                        // } else if(bottom_door) {
                            // top_door= true;
                            // bottom_door = false;
                        // }
                    }
                }
            }
        }
    }

    World* world = global_game_state->world;
    Tilemap *tilemap = world->tilemap;

    // Pinkish Debug BG
    Draw_Rectangle(back_buffer, 0.0f, (real32)back_buffer->width, 0.0f, (real32)back_buffer->height, 1.0, 0.0, 1.0f);

    Tilemap_Position hero_pos = global_game_state->hero_position;
    Raw_Chunk_Position raw_hero_pos = get_raw_chunk_pos_for(tilemap, &hero_pos);
   
    // @Note Without scrolling.
    // { // @Note Draw world.
    //     Tile_Chunk* tile_chunk = get_tile_chunk(tilemap, raw_hero_pos.chunk_x, raw_hero_pos.chunk_y);
    //     for (u32 row = 0; row < tilemap->num_tilemap_rows; row++) {
    //         for (u32 col = 0; col < tilemap->num_tilemap_cols; col++) {
    //             real32 greyish = (get_tile_value(tilemap, tile_chunk, col, row) == 1) ? 1.0f : 0.5f;
                
    //             if (row == raw_hero_pos.tile_y && col == raw_hero_pos.tile_x) {
    //                 // @Note Debug draw hero's tile
    //                 greyish = 0.0f;
    //             }

    //             real32 l = tilemap->lower_left_x + ((real32)col * tilemap->tile_side_in_pixels);
    //             real32 r = l + tilemap->tile_side_in_pixels;
    //             real32 t = tilemap->lower_left_y - ((real32)row * tilemap->tile_side_in_pixels);
    //             real32 b = t - tilemap->tile_side_in_pixels;

    //             Draw_Rectangle(back_buffer, l, r, b, t, greyish, greyish, greyish);
    //         }
    //     }
    // }

    // { // @Note Draw hero.
    //     real32 R = 1.0f, G = 0.0f, B = 0.0f;
    //     real32 hero_x = tilemap->lower_left_x + (raw_hero_pos.tile_x * tilemap->tile_side_in_pixels + global_game_state->hero_position.offset_x * tilemap->meters_to_pixels);
    //     real32 hero_y = tilemap->lower_left_y - (raw_hero_pos.tile_y * tilemap->tile_side_in_pixels + global_game_state->hero_position.offset_y * tilemap->meters_to_pixels);
        
    //     real32 hero_width = 0.75f * (real32)tilemap->tile_side_in_pixels;
    //     real32 hero_height = (real32)tilemap->tile_side_in_pixels;
        
    //     real32 l = hero_x - 0.5f * hero_width;
    //     real32 r = hero_x + 0.5f * hero_width;
    //     real32 b = hero_y - 0.5f * hero_height;
    //     real32 t = hero_y + 0.5f * hero_height;
    //     Draw_Rectangle(back_buffer, l, r, b, t, R, G, B);
    // }

    { // @Note With scrolling
        u32 tile_side_in_pixels = 60;
        real32 meters_to_pixels = (real32)tile_side_in_pixels / tilemap->tile_side_in_meters;

        real32 screen_height = (real32)back_buffer->height;
        real32 screen_center_x = (real32)back_buffer->width / 2.0f;
        real32 screen_center_y = (real32)back_buffer->height / 2.0f;

        { // @Note Draw the world.
            real32 center_tile_x = screen_center_x - hero_pos.offset_x * meters_to_pixels;
            real32 center_tile_y = screen_center_y - hero_pos.offset_y * meters_to_pixels;
            real32 half_tile_side_in_pixels = 0.5f * tile_side_in_pixels;

            for (s32 dy = -10; dy < 10; dy++) {
                for (s32 dx = -20; dx < 20; dx++) {
                    real32 greyish = 1.0f;

                    real32 x = center_tile_x + (real32)dx * tile_side_in_pixels;
                    real32 y = center_tile_y + (real32)dy * tile_side_in_pixels;

                    real32 l = x - half_tile_side_in_pixels;
                    real32 r = x + half_tile_side_in_pixels;
                    real32 b = screen_height - y - half_tile_side_in_pixels;
                    real32 t = screen_height - y + half_tile_side_in_pixels;

                    u32 tile_x = dx + hero_pos.x;
                    u32 tile_y = dy + hero_pos.y;

                    u32 val = get_tile_value(tilemap, tile_x, tile_y);

                    if (val) {
                        if (val == 1)
                            greyish = 0.5f;
                        else if (val == 2)
                            greyish = 1.0f;

                        if (tile_x == hero_pos.x && tile_y == hero_pos.y) {
                            greyish = 0.0f;
                        }

                        Draw_Rectangle(back_buffer, l, r, b, t, greyish, greyish, greyish);
                    }
                }
            }
        }
        { // @Note Draw hero.
            real32 R = 1.0f, G = 0.0f, B = 0.0f;
            real32 half_hero_width = 0.5f * 0.75f * (real32)tile_side_in_pixels;
            real32 half_hero_height = 0.5f * (real32)tile_side_in_pixels;

            real32 l = screen_center_x - half_hero_width; 
            real32 r = screen_center_x + half_hero_width;
            real32 b = screen_height - screen_center_y - half_hero_height;
            real32 t = screen_height - screen_center_y + half_hero_height;

            Draw_Rectangle(back_buffer, l, r, b, t, R, G, B);
        }
    }
    Handle_Game_Input(input);
}

extern "C" GET_SOUND_SAMPLES(Get_Sound_Samples) { Output_Sound(sound_buffer); }