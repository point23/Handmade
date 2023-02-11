#if !defined(HANDMADE_H)

/**
 * @brief
 * TODO
 * Services that the platform provices to the game:
 * - Timing
 * - Controller/Keyboard Input
 * - Bitmap Buffer
 * - Sound Buffer
 */

#define internal static
#define global static
#define local_persist static

#define real32 float
#define real64 float

#define int16 INT16
#define int64 INT64

#define uint8 UINT8
#define uint16 UINT16
#define uint UINT32

#define PI32 3.14159265359f

struct Game_Back_Buffer
{
  void* bitmap;
  int width;
  int height;
  int pitch;
};

struct Game_Sound_Buffer
{
  int16* samples;
  int samples_per_second;
  int sample_count;
};

static void
Game_Update(Game_Back_Buffer* back_buffer, Game_Sound_Buffer* sound_buffer);

#define HANDMADE_H
#endif