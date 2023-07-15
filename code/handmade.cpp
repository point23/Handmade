#include "handmade.h"
#include "handmade_tile.h"
#include "handmade_tile.cpp"
#include "handmade_random.h"

#define NUM_TILEMAP_COLS 17
#define NUM_TILEMAP_ROWS 9

#define NUM_WORLD_COLS 128
#define NUM_WORLD_ROWS 128

PLATFORM_PRINT_POINTER(Print);

// @Note We shouldn't setup the game world by something global since we are unloading and loading this lib quite often...
global Game_State *global_game_state;

#pragma pack(push, 1)
// @Note Bitmap storage: https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-storage
struct Bitmap_Header {
    // @Note Bitmap file header: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader
    u16 file_type;
    u32 file_size;
    u16 reserved1;
    u16 reserved2;
    u32 bitmap_offset;
    // @Note Bitmap info header(V4): https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv4header
    u32 size;
    s32 width;
    s32 height;
    u16 planes;
    u16 bits_per_pixel;
    u32 compression;
    u32 size_image;
    u32 horizontal_resolution; // In pixels-per-meter
    u32 vertical_resolution;   // In pixles-per-meter
    u32 colors_used;
    u32 colors_important;
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
    u32 alpha_mask;
};
#pragma pack(pop)

inline u8 process_pixel_with_mask(u32 pixel, u32 mask) {
    Bit_Scan_Result result = bit_scan_forward(mask);
    return (u8)(pixel >> result.index);
}

internal Loaded_Bitmap Load_BMP(Thread_Context* thread, debug_platform_read_file* Read_File, char* filename) {
    Loaded_Bitmap result = {};

    File_Result read_result = {};
    Read_File(thread, filename, &read_result);

    if (read_result.content) {
        Bitmap_Header* header = (Bitmap_Header*)read_result.content;

        result.width = header->width;
        result.height = header->height;

        assert(header->compression == 3);

        result.pixels = (u32*)((u8*)read_result.content + header->bitmap_offset);

        // @Note Restructure pixels, expected order: 0xAARRGGBB
        u32* pixel = result.pixels;
        for (s32 y = 0; y < header->height; y++) {
            for (s32 x = 0; x < header->width; x++) {
                u8 A = process_pixel_with_mask(*pixel, header->alpha_mask);
                u8 R = process_pixel_with_mask(*pixel, header->red_mask);
                u8 G = process_pixel_with_mask(*pixel, header->green_mask);
                u8 B = process_pixel_with_mask(*pixel, header->blue_mask);

                *pixel = (A << 24 | R << 16 | G << 8 | B);
                *pixel++;
            }
        }
    }

    return result;
}

internal void Draw_Bitmap(Game_Back_Buffer* buffer, Loaded_Bitmap bitmap, real32 upper_left_x, real32 upper_left_y) {
    s32 l = round_real32_to_s32(upper_left_x);
    s32 r = l + bitmap.width;
    s32 b = round_real32_to_s32(upper_left_y);
    s32 t = b + bitmap.height;

    s32 clip_x = 0;
    s32 clip_y = 0;

    if (l < 0) {
        clip_x = -l;
        l = 0;
    }

    if (b < 0) {
        clip_y = -b;
        b = 0;
    }

    if (r > buffer->width) r = buffer->width;
    if (t > buffer->height) t = buffer->height;

    // @Note Begin at Upper-Left.
    u32* source_row = bitmap.pixels + bitmap.width * clip_y + clip_x;
    u32* dest_row = (u32*)buffer->memory + b * buffer->width + l;

    for (s32 y = b; y < t; y++) {
        u32* source = source_row;
        u32* dest = dest_row;

        for (s32 x = l; x < r; x++) {
            // @Note Why can't we also use a mask here?
            real32 SA = (real32)((*source >> 24) & 0xff);
            real32 SR = (real32)((*source >> 16) & 0xff);
            real32 SG = (real32)((*source >>  8) & 0xff);
            real32 SB = (real32)((*source >>  0) & 0xff);

            real32 DR = (real32)((*dest >> 16) & 0xff);
            real32 DG = (real32)((*dest >>  8) & 0xff);
            real32 DB = (real32)((*dest >>  0) & 0xff);

            if (SA > 128) {
                real32 ratio = SA / 255.0f;

                u32 R = (u32)((1 - ratio) * DR + ratio * SR + 0.5f);
                u32 G = (u32)((1 - ratio) * DG + ratio * SG + 0.5f);
                u32 B = (u32)((1 - ratio) * DB + ratio * SB + 0.5f);

                *dest = R << 16 | G << 8 | B;
            }

            dest++;
            source++;
        }

        source_row += bitmap.width;
        dest_row += buffer->width;
    }
}

internal void Output_Sound(Game_Sound_Buffer *buffer) {}

internal void Initialize_Entity(Entity* entity) {
    entity->exist = true;

    entity->orientation = DIR_DOWN;

    entity->position.x = 1;
    entity->position.y = 3;
    entity->position.z = 0;
    entity->position.tile_offset.x = 0.5f;
    entity->position.tile_offset.y = 0.5f;

    entity->height = 1.4f;
    entity->width = entity->height * 0.75f;
}

internal Entity* Get_Entity(Game_State* game_state, u32 index) {
    Entity* result = 0;
    if (index < get_array_size(game_state->entities)) {
        result = &game_state->entities[index];
    }
    return result;
}

internal u32 Add_Entity(Game_State* game_state) {
    u32 index = game_state->entity_count++;
    assert(index < get_array_size(game_state->entities));
    return index;
}

internal void Move_Player(Game_State* game_state, Entity * entity, Vector2 acc, real32 dt) {
    Tilemap* tilemap = game_state->world->tilemap;
    Vector2& velocity = entity->velocity;

    real32 speed = 80.0f;
    if (0) { // @Todo Speed up when player pressed action button.
        speed = 100.0f; 
    }

    acc *= speed;
    acc -= 8.0f * velocity; // @Note Add Some Friction.

    Tilemap_Position old_position = entity->position;
    Tilemap_Position new_center = old_position;

    // p' = (1/2 * a * t^2) + v * t + p
    Vector2 position_delta = (0.5f * acc * square(dt)) + (velocity * dt);
    new_center.tile_offset = position_delta + new_center.tile_offset;
    // v' = a * t + v
    velocity += (acc * dt);

    Tilemap_Position new_left = new_center;
    new_left.tile_offset.x -= 0.5f * entity->width;
    
    Tilemap_Position new_right = new_center;
    new_right.tile_offset.x += 0.5f * entity->width;

    canonicalize_position(tilemap, &new_center);
    canonicalize_position(tilemap, &new_left);
    canonicalize_position(tilemap, &new_right);

    Tilemap_Position collide_position = new_center;
    bool collided = false;
    if (!is_point_empty(tilemap, &new_center)) {
        collided = true;
        collide_position = new_center;
    }        
    if (!is_point_empty(tilemap, &new_left)) {
        collided = true;
        collide_position = new_left;
    }        
    if (!is_point_empty(tilemap, &new_right)) {
        collided = true;
        collide_position = new_right;
    }

    if (!collided) {
        if (!same_position(old_position, new_center)) {
            u32 value = get_tile_value(tilemap, new_center.x, new_center.y, new_center.z);
            
            if (value == 3) {
                new_center.z = 1 - new_center.z;  // @Temporary
            }
        }

        entity->position = new_center;
    } else { // Bounce back.
        Vector2 n = {0.0f, 0.0f};
        if (collide_position.x < old_position.x) {
            n.x = -1.0f;
        }
        if (collide_position.x > old_position.x) {
            n.x = 1.0f;
        }
        if (collide_position.y < old_position.y) {
            n.y = 1.0f;
        }
        if (collide_position.y > old_position.y) {
            n.y = -1.0f;
        }
        velocity = velocity - (1 * dot_product(velocity, n) * n);
    }

    // @Todo Working on new collision detection strategy.
    // const u32 min_tile_y = 0;
    // const u32 min_tile_x = 0;
    // const u32 one_past_max_tile_y = ;
    // const u32 one_past_max_tile_x = ;
    // const tile_z = global_game_state->hero_position.z;
    
    // Tilemap_Position best_point = global_game_state->hero_position;
    // s32 best_distance_square = length_square(position_delta);

    // for (u32 y = min_tile_y; y != one_past_max_tile_y; y++) {
    //     for (u32 x = min_tile_x; x != one_past_max_tile_x; x++) {
    //         u32 v = get_tile_value(tilemap, x, y, tile_z);

    //         Tilemap_Position test_position = centered_tilemap_position(x, y, tile_z);
    //     }
    // }
}

internal void Handle_Game_Input(Game_State* game_state, Game_Input *input) {
    for (u32 i = 0; i < 5; i++) {
        Game_Controller_Input* controller = &input->controllers[i];
        u32 entity_index = game_state->player_index_for_controller[i];
        Entity* entity = Get_Entity(game_state, entity_index);
        real32 dt = input->dt_per_frame;

        // Print("Controller: %d, Entity: %d", i, entity_index);

        if (!entity->exist) { // @Note Add entity if new player join in.
            if (controller->start.ended_down) {
                entity_index = Add_Entity(game_state);
                Initialize_Entity(Get_Entity(game_state, entity_index));
                game_state->player_index_for_controller[i] = entity_index;

                if (!game_state->camera_following_entity_index) {
                    game_state->camera_following_entity_index = entity_index;
                }
            }

            continue;
        }

        // Settle new aca value depend on controller status.
        Vector2 acc = {};
        if (controller->is_analog) {
            acc = Vector2{controller->stick_average_x, controller->stick_average_y};
        } else {
            if (controller->move_up.ended_down) {
                entity->orientation = DIR_UP;
                acc.y = 1.0f;
            }

            if (controller->move_down.ended_down) {
                entity->orientation = DIR_DOWN;
                acc.y = -1.0f;
            }

            if (controller->move_right.ended_down) {
                entity->orientation = DIR_RIGHT;
                acc.x = 1.0f;
            }

            if (controller->move_left.ended_down) {
                entity->orientation = DIR_LEFT;
                acc.x = -1.0f;
            }
        }

        Move_Player(game_state, entity, acc, dt);
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
    u8 *row = (u8 *)buffer->memory + l * buffer->bytes_per_pixel + b * buffer->pitch;
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
    Print = memory->Platform_Print;

    if (!memory->is_initialized) {
        memory->is_initialized = true;

        // @Note Reserve entity slot 0 for null entity.
        u32 null_entity_idx = Add_Entity(global_game_state);
        
        global_game_state->background = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_background.bmp");

        Hero_Bitmap* hero_bitmap = &global_game_state->hero_bitmaps[DIR_DOWN];
        hero_bitmap->align_x = 72;
        hero_bitmap->align_y = 35;
        hero_bitmap->head = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_front_head.bmp");
        hero_bitmap->cape = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_front_cape.bmp");
        hero_bitmap->torso = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_front_torso.bmp");

        hero_bitmap = &global_game_state->hero_bitmaps[DIR_LEFT];
        hero_bitmap->align_x = 72;
        hero_bitmap->align_y = 35;
        hero_bitmap->head = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_left_head.bmp");
        hero_bitmap->cape = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_left_cape.bmp");
        hero_bitmap->torso = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_left_torso.bmp");

        hero_bitmap = &global_game_state->hero_bitmaps[DIR_UP];
        hero_bitmap->align_x = 72;
        hero_bitmap->align_y = 35;
        hero_bitmap->head = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_back_head.bmp");
        hero_bitmap->cape = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_back_cape.bmp");
        hero_bitmap->torso = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_back_torso.bmp");

        hero_bitmap = &global_game_state->hero_bitmaps[DIR_RIGHT];
        hero_bitmap->align_x = 72;
        hero_bitmap->align_y = 35;
        hero_bitmap->head = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_right_head.bmp");
        hero_bitmap->cape = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_right_cape.bmp");
        hero_bitmap->torso = Load_BMP(thread, memory->Debug_Platform_Read_File, "test/test_hero_right_torso.bmp");
        
        Memory_Arena* memory_arena = &global_game_state->memory_arena;
        u64 memory_used =  sizeof(Game_State);
        Init_Memory_Arena(memory_arena, memory->permanent_storage_size - memory_used, (u8*)memory->permanent_storage + memory_used);
        global_game_state->world = PUSH_STRUCT(memory_arena, World);
        World* world = global_game_state->world;
        world->tilemap = PUSH_STRUCT(memory_arena, Tilemap);
        Tilemap* tilemap = world->tilemap;

        { // @Note Setup camera pos
            global_game_state->camera_position = {};
            global_game_state->camera_position.x = 8;
            global_game_state->camera_position.y = 4;
            global_game_state->camera_position.z = 0;
        }

        { // @Note Setup tilemap data.
            tilemap->count_x = NUM_WORLD_COLS;
            tilemap->count_y = NUM_WORLD_ROWS;
            tilemap->count_z = 2;

            tilemap->chunk_shift = 8;
            tilemap->chunk_dim = 1 << tilemap->chunk_shift;
            tilemap->chunk_mask = 0xff;

            tilemap->tile_side_in_meters = 1.4f; // @Note Avg 10yo child height.
        
            // @Note lower left x/y are measured in pixels.
            real32 lower_left_x = -30.0f;
            real32 lower_left_y = (real32)back_buffer->height;

            u32 tile_chunk_count = tilemap->count_x * tilemap->count_y * tilemap->count_z;
            tilemap->tile_chunks = PUSH_ARRAY(memory_arena, tile_chunk_count, Tile_Chunk);

            { // @Note Setup tile chunks.
                bool door_left = false;
                bool door_right = false;
                bool door_top = false;
                bool door_bottom = false;
                bool door_up = false;
                bool door_down = false;

                u32 screen_x = 0;
                u32 screen_y = 0;
                u32 screen_z = 0;

                u32 num_tilemap_rows = NUM_TILEMAP_ROWS;
                u32 num_tilemap_cols = NUM_TILEMAP_COLS;
                
                u32 random_number_idx = 0;
  
                for (u32 screen = 0; screen < 100; screen++) {
                    u32 random_number = random_numbers[random_number_idx++];
                    u32 dir = 0;
                    if(door_up || door_down) {
                        dir = random_number % 2;
                    } else {
                        dir = random_number % 3;
                    }

                    if(dir == 0) {
                        door_right = true;
                    } else if(dir == 1) {
                        door_top = true;
                    } else if(dir == 2) { // @Temporary
                        if(screen_z == 0) {
                            door_up = true;
                        } else if(screen_z == 1) {
                            door_down = true;
                        }
                    }
                    
                    for(u32 tile_y = 0; tile_y < num_tilemap_rows; tile_y++) {
                        for(u32 tile_x = 0; tile_x < num_tilemap_cols; tile_x++) {
                            u32 x = screen_x * num_tilemap_cols + tile_x;
                            u32 y = screen_y * num_tilemap_rows + tile_y;
                            u32 z = screen_z;
                            u32 value = 1;
                            
                            if(tile_x == 0) {
                                value = 2;
                                if(door_left && (tile_y == num_tilemap_rows / 2)) {
                                    value = 1;
                                }
                            }

                            if(tile_x == (num_tilemap_cols - 1)) {
                                value = 2;
                                if(door_right && (tile_y == num_tilemap_rows / 2)) {
                                    value = 1;
                                }
                            }

                            if(tile_y == 0) {
                                value = 2;
                                if(door_bottom && (tile_x == num_tilemap_cols / 2)) {
                                    value = 1;
                                }
                            }
                            if(tile_y == (num_tilemap_rows - 1)) {
                                value = 2;
                                if(door_top && (tile_x == num_tilemap_cols / 2)) {
                                    value = 1;
                                }
                            }

                            if (tile_x == 10 && tile_y == 4 && (door_up || door_down)) {
                                value = 3;
                            }

                            set_tile_value(memory_arena, tilemap, x, y, z, value);
                        }
                    }

                    door_left = door_right;
                    door_bottom = door_top;

                    door_right = false;
                    door_top = false;

                    if (dir == 2) {
                        screen_z = 1 - screen_z; // @Temporary

                        if (door_up) {
                            door_down = true;
                            door_up = false;
                        } else if (door_down) {
                            door_down = false;
                            door_up = true;
                        }
                    } else if (dir == 1) {
                        screen_y += 1;
                        door_up = false;
                        door_down = false;
                    } else if (dir == 0) {
                        screen_x += 1;
                        door_up = false;
                        door_down = false;
                    }
                }
            }
        }
    }

    World* world = global_game_state->world;
    Tilemap *tilemap = world->tilemap;

    Handle_Game_Input(global_game_state, input);

    // @Note Move camera to follow target player.
    Entity* camera_follwing_entity = Get_Entity(global_game_state, global_game_state->camera_following_entity_index);
    
    if (camera_follwing_entity) {
        Tilemap_Position* camera_pos = &global_game_state->camera_position;
        camera_pos->z = camera_follwing_entity->position.z;

        real32 delta_x = tilemap->tile_side_in_meters * ((real32)camera_follwing_entity->position.x - (real32)camera_pos->x) + (camera_follwing_entity->position.tile_offset.x - camera_pos->tile_offset.x);
        real32 delta_y = tilemap->tile_side_in_meters * ((real32)camera_follwing_entity->position.y - (real32)camera_pos->y) + (camera_follwing_entity->position.tile_offset.y - camera_pos->tile_offset.y);

        if(delta_x > ((real32)NUM_TILEMAP_COLS / 2) * tilemap->tile_side_in_meters) {
            camera_pos->x += NUM_TILEMAP_COLS;
        }

        if(delta_x < -((real32)NUM_TILEMAP_COLS / 2) * tilemap->tile_side_in_meters) {
            camera_pos->x -= NUM_TILEMAP_COLS;
        }

        if(delta_y > ((real32)NUM_TILEMAP_ROWS / 2) * tilemap->tile_side_in_meters) {
            camera_pos->y += NUM_TILEMAP_ROWS;
        }

        if(delta_y < -((real32)NUM_TILEMAP_ROWS / 2) * tilemap->tile_side_in_meters) {
            camera_pos->y -= NUM_TILEMAP_ROWS;
        }
    }

    // Pinkish Debug BG
    // Draw_Rectangle(back_buffer, 0.0f, (real32)back_buffer->width, 0.0f, (real32)back_buffer->height, 1.0, 0.0, 1.0f);
    Draw_Bitmap(back_buffer, global_game_state->background, 0, 0);

    { // @Note With scrolling
        Tilemap_Position* camera_pos = &global_game_state->camera_position;

        u32 tile_side_in_pixels = 60;
        real32 meters_to_pixels = (real32)tile_side_in_pixels / tilemap->tile_side_in_meters;

        real32 screen_height = (real32)back_buffer->height;

        Vector2 screen_center = {
            (real32)back_buffer->width / 2.0f, 
            (real32)back_buffer->height / 2.0f 
        };

        { // @Note Draw the world.
            Vector2 center_tile = {
                screen_center.x - camera_pos->tile_offset.x * meters_to_pixels, 
                screen_center.y - camera_pos->tile_offset.y * meters_to_pixels
            };

            real32 half_tile_side_in_pixels = 0.5f * tile_side_in_pixels;

            for (s32 dy = -10; dy < 10; dy++) {
                for (s32 dx = -20; dx < 20; dx++) {
                    real32 greyish = 1.0f;

                    real32 x = center_tile.x + (real32)dx * tile_side_in_pixels;
                    real32 y = center_tile.y + (real32)dy * tile_side_in_pixels;

                    real32 l = x - half_tile_side_in_pixels;
                    real32 r = x + half_tile_side_in_pixels;
                    real32 b = y - half_tile_side_in_pixels;
                    real32 t = y + half_tile_side_in_pixels;

                    u32 tile_x = dx + camera_pos->x;
                    u32 tile_y = dy + camera_pos->y;

                    u32 val = get_tile_value(tilemap, tile_x, tile_y, camera_pos->z);

                    if (val > 1) {
                        if (val == 2)
                            greyish = 1.0f;
                        else if (val == 3)
                            greyish = 0.25f;

                        if (tile_x == camera_pos->x && tile_y == camera_pos->y) {
                            greyish = 0.0f;
                        }

                        Draw_Rectangle(back_buffer, l, r, b, t, greyish, greyish, greyish);
                    }
                }
            }
        }

        for (u32 i = 1; i <= global_game_state->entity_count; i++) {
            Entity* entity = &global_game_state->entities[i];

            if (!entity->exist) {
                continue;
            }

            { // @Note Draw hero.
                real32 R = 1.0f, G = 0.0f, B = 0.0f;
                real32 half_hero_width  = 0.5f * 0.75f * (real32)tile_side_in_pixels;
                real32 half_hero_height = 0.5f * (real32)tile_side_in_pixels;

                real32 tile_delta_x = (real32)entity->position.x - (real32)camera_pos->x;
                real32 tile_delta_y = (real32)entity->position.y - (real32)camera_pos->y;
                Vector2 offset_delta = entity->position.tile_offset - camera_pos->tile_offset;
                Vector2 delta = {
                    tilemap->tile_side_in_meters * tile_delta_x + offset_delta.x,
                    tilemap->tile_side_in_meters * tile_delta_y + offset_delta.y
                };

                delta *= meters_to_pixels;

                Vector2 hero_center = screen_center + delta;

                real32 l = hero_center.x - half_hero_width; 
                real32 r = hero_center.x + half_hero_width;
                real32 b = hero_center.y - half_hero_height;
                real32 t = hero_center.y + half_hero_height;

                Draw_Rectangle(back_buffer, l, r, b, t, R, G, B);
                
                u32 orientation = entity->orientation;
                Hero_Bitmap hero_bmp = global_game_state->hero_bitmaps[orientation];
                
                real32 bmp_x = hero_center.x - hero_bmp.align_x;
                real32 bmp_y = hero_center.y - hero_bmp.align_y;

                Draw_Bitmap(back_buffer, hero_bmp.head, bmp_x, bmp_y);
                Draw_Bitmap(back_buffer, hero_bmp.cape, bmp_x, bmp_y);
                Draw_Bitmap(back_buffer, hero_bmp.torso, bmp_x, bmp_y);
            }
        }
    }
}

extern "C" GET_SOUND_SAMPLES(Get_Sound_Samples) { Output_Sound(sound_buffer); }