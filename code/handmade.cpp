#include "handmade.h"

global int Global_Blue_Offset = 0;
global int Global_Green_Offset = 0;
global int Global_Tone_Hz = 256;

// @hack Drawing blue-green gradient
static void
Render_Gradient(Game_Back_Buffer* buffer)
{
  uint8* row = (UINT8*)(buffer->bitmap);
  for (int y = 0; y < buffer->height; y++) {
    uint* pixel = (uint*)row;

    for (int x = 0; x < buffer->width; x++) {
      uint8 blueCannel = (uint8)(x + Global_Blue_Offset);
      uint8 greenCannel = (uint8)(y + Global_Green_Offset);

      // @note Little-endian RGB: GB|AR
      *pixel++ = (greenCannel << 8) | blueCannel;
    }
    row += buffer->pitch;
  }
}

// @hack Play sin wave sounds
static void
Output_Sound(Game_Sound_Buffer* buffer)
{
  local_persist real32 t;
  const int16 tone_volume = 3000;

  int wave_period =
    buffer->samples_per_second /
    Global_Tone_Hz; // Samples-per-circle = samples-per-second / frequency

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
static void
Handle_Controller_Input(Game_Controller_Input* input)
{
  if (input->is_analog) {
    Global_Tone_Hz = 256 + (int)(128.0f * input->end_y);
    Global_Blue_Offset += (int)4.0f * input->end_x;
  }

  if (input->down.ended_down) {
    Global_Green_Offset += 1;
  }
}

static void
Game_Update(Game_Back_Buffer* back_buffer,
            Game_Sound_Buffer* sound_buffer,
            Game_Input* input)
{
  Handle_Controller_Input(&input->controllers[0]);
  Render_Gradient(back_buffer);
  Output_Sound(sound_buffer);
}