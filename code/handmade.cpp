#include "handmade.h"

global Game_State* global_game_state;

// @hack Drawing blue-green gradient
static void
Render_Gradient(Game_Back_Buffer* buffer)
{
  uint8* row = (uint8*)(buffer->bitmap);
  for (int y = 0; y < buffer->height; y++) {
    uint* pixel = (uint*)row;

    for (int x = 0; x < buffer->width; x++) {
      uint8 blue_channel = (uint8)(x + global_game_state->blue_offset);
      uint8 green_channel = (uint8)(y + global_game_state->green_offset);

      // @note Little-endian RGB: GB|AR
      *pixel++ = (green_channel << 8) | blue_channel;
    }
    row += buffer->pitch;
  }
}

// @hack Play sin wave sounds
internal void
Output_Sound(Game_Sound_Buffer* buffer)
{
  local_persist real32 t;
  const int16 tone_volume = 3000;
  
  int wave_period =
    buffer->samples_per_second /
    global_game_state->tone_hz; // @note Samples-per-circle = samples-per-second / frequency
  int16* write_cursor = buffer->samples;

  for (int i = 0; i < buffer->sample_count; i++) {
    float sine_val = sinf(t);
    int16 sample_val = (int16)(tone_volume * sine_val);
    *write_cursor++ = sample_val; // Left
    *write_cursor++ = sample_val; // Right

    t += 2.0f * PI32 * (1.0f / (real32)wave_period);
  }
}

// @hack
internal void
Handle_Controller_Input(Game_Controller_Input* input)
{
  if (input->is_analog) {
    global_game_state->tone_hz = 256 + (int)(128.0f * input->end_y);
    global_game_state->blue_offset += (int)4.0f * input->end_x;
  }

  if (input->down.ended_down) {
    global_game_state->green_offset += 1;
  }
}

internal void
Game_Update(Game_Memory* memory,
            Game_Back_Buffer* back_buffer,
            Game_Sound_Buffer* sound_buffer,
            Game_Input* input)
{
  global_game_state = (Game_State*)memory->permanent_storage;
  if (!memory->is_initialized) {
    global_game_state->tone_hz = 256;
    memory->is_initialized = true;
  }

  Handle_Controller_Input(&input->controllers[0]);
  Render_Gradient(back_buffer);
  Output_Sound(sound_buffer);
}
