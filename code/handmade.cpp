#include "handmade.h"

/* TEST drawing blud-green gradient */
static void
Render_Gradient(Game_Back_Buffer* buffer)
{
  uint8* row = (UINT8*)(buffer->bitmap);
  for (int y = 0; y < buffer->height; y++) {
    uint* pixel = (uint*)row;

    for (int x = 0; x < buffer->width; x++) {
      uint8 blueCannel = (uint8)x;
      uint8 greenCannel = (uint8)y;

      *pixel++ = (greenCannel << 8) | blueCannel;
    }
    row += buffer->pitch;
  }
}

/* TEST Play sin wave sounds */
static void
Output_Sound(Game_Sound_Buffer* buffer)
{
  local_persist real32 t;

  int16 tone_volume = 3000;
  int tone_hz = 256;
  int wave_period =
    buffer->samples_per_second /
    tone_hz; // Samples-per-circle = samples-per-second / frequency

  int16* write_cursor = buffer->samples;
  for (int i = 0; i < buffer->sample_count; i++) {
    float sine_val = sinf(t);
    int16 sample_val = (int16)(tone_volume * sine_val);
    *write_cursor++ = sample_val; // Left
    *write_cursor++ = sample_val; // Right

    t += 2.0f * PI32 * (1.0f / (real32)wave_period);
  }
}

static void
Game_Update(Game_Back_Buffer* back_buffer, Game_Sound_Buffer* sound_buffer)
{
  Render_Gradient(back_buffer);
  Output_Sound(sound_buffer);
}