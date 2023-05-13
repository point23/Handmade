#include "handmade.h"

global Game_State* global_game_state;

//  Play sin wave sounds
internal void
Output_Sound(Game_Sound_Buffer* buffer)
{
}

internal void
Handle_Controller_Input(Game_Controller_Input* input)
{
}

inline s32
round_real32_to_s32(real32 val)
{
    s32 res = (s32)(val + 0.5f);
    return res;
}

internal void
Draw_Rectangle(Game_Back_Buffer* buffer,
               real32 l_,
               real32 r_,
               real32 b_,
               real32 t_)
{
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

    u32 color = 0xFFFFFFFF;
    u8* row =
      (u8*)buffer->bitmap + l * buffer->bytes_per_pixel + b * buffer->pitch;
    for (s32 y = b; y < t; y++) {
        u32* pixel = (u32*)row;
        for (s32 x = l; x < r; x++) {
            *pixel++ |= color;
        }
        row += buffer->pitch;
    }
}

// @Note Function signare is define in handmade.h
extern "C" GAME_UPDATE_AND_RENDER(Game_Update_And_Render)
{
    global_game_state = (Game_State*)memory->permanent_storage;
    if (!memory->is_initialized) {
        memory->is_initialized = true;
    }

    Draw_Rectangle(back_buffer, -10.0f, 20.0f, 30.0f, 40.0f);
    Handle_Controller_Input(&input->controllers[0]);
}

extern "C" GET_SOUND_SAMPLES(Get_Sound_Samples)
{
    Output_Sound(sound_buffer);
}