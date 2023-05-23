#include "handmade.h"

global Game_State *global_game_state;

//  Play sin wave sounds
internal void Output_Sound(Game_Sound_Buffer *buffer) {}

internal void Handle_Controller_Input(Game_Controller_Input *input) {
    real32 dx = 0.0f, dy = 0.0f;

    if (input->move_down.ended_down) {
        dy = 1.0f;
    }
    if (input->move_up.ended_down) {
        dy = -1.0f;
    }
    if (input->move_left.ended_down) {
        dx = -1.0f;
    }
    if (input->move_right.ended_down) {
        dx = 1.0f;
    }

    global_game_state->player_y += dy;
    global_game_state->player_x += dx;
}

inline s32 round_real32_to_s32(real32 val) {
    s32 res = (s32)(val + 0.5f);
    return res;
}

inline u32 round_real32_to_u32(real32 val) {
    u32 res = (u32)(val + 0.5f);
    return res;
}

internal void Draw_Rectangle(Game_Back_Buffer *buffer, real32 l_, real32 r_,
                             real32 b_, real32 t_, real32 R, real32 G,
                             real32 B) {
    s32 l = round_real32_to_s32(l_);
    s32 r = round_real32_to_s32(r_);
    s32 b = round_real32_to_s32(b_);
    s32 t = round_real32_to_s32(t_);

    if (l > r) {
        // swap(l, r); // @ImplementMe
    }
    if (b > t) {
        // swap(b, t); // @ImplementMe
    }

    if (l < 0)
        l = 0;
    if (l > buffer->width)
        l = buffer->width;

    if (r < 0)
        r = 0;
    if (r > buffer->width)
        r = buffer->width;

    if (b < 0)
        b = 0;
    if (b > buffer->height)
        b = buffer->height;

    if (t < 0)
        t = 0;
    if (t > buffer->height)
        r = buffer->height;

    // @Note ARGB
    u32 color = (round_real32_to_u32(R * 255.0f)) << 16 |
                (round_real32_to_u32(G * 255.0f)) << 8 |
                (round_real32_to_u32(B * 255.0f)) << 0;
    u8 *row =
        (u8 *)buffer->bitmap + l * buffer->bytes_per_pixel + b * buffer->pitch;
    for (s32 y = b; y < t; y++) {
        u32 *pixel = (u32 *)row;
        for (s32 x = l; x < r; x++) {
            *pixel++ =
                color; // @Note For now we choose not to blend the color by '|='
        }
        row += buffer->pitch;
    }
}

// @Note Function signare is define in handmade.h
extern "C" GAME_UPDATE_AND_RENDER(Game_Update_And_Render) {
    global_game_state = (Game_State *)memory->permanent_storage;
    if (!memory->is_initialized) {
        memory->is_initialized = true;
    }

    // Pinkish Debug BG
    Draw_Rectangle(back_buffer, 0.0f, (real32)back_buffer->width, 0.0f,
                   (real32)back_buffer->height, 1.0, 0.0, 1.0f);

    real32 tile_width = 60;
    real32 tile_height = 60;
    { // @Note Draw tilemap
        s32 rows = 9;
        s32 cols = 17;
        real32 upper_left_x = -30;
        real32 upper_left_y = 0;

        u32 tilemap[9][17] = {
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
            {1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1},
            {1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1},
            {1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1},
            {1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1},
            {1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        };

        for (s32 row = 0; row < rows; row++) {
            for (s32 col = 0; col < cols; col++) {
                u32 tile_id = tilemap[row][col];
                real32 greyish = 0.5f;
                if (tile_id == 1) {
                    greyish = 1.0f;
                }
                real32 l = upper_left_x + ((real32)col * tile_width);
                real32 r = l + tile_width;
                real32 b = upper_left_y + ((real32)row * tile_height);
                real32 t = b + tile_height;
                Draw_Rectangle(back_buffer, l, r, b, t, greyish, greyish,
                               greyish);
            }
        }
    }

    { // @Note Draw hero.
        real32 R = 1.0f, G = 0.0f, B = 0.0f;
        real32 hero_x = global_game_state->player_x,
               hero_y = global_game_state->player_y;
        real32 hero_width = 0.75f * tile_width;
        real32 hero_height = tile_height;
        real32 l = hero_x - 0.5f * hero_width;
        real32 r = hero_x + 0.5f * hero_width;
        real32 b = hero_y - 0.5f * hero_height;
        real32 t = hero_y + 0.5f * hero_height;
        Draw_Rectangle(back_buffer, l, r, b, t, R, G, B);
    }

    Handle_Controller_Input(&input->controllers[0]);
}

extern "C" GET_SOUND_SAMPLES(Get_Sound_Samples) { Output_Sound(sound_buffer); }