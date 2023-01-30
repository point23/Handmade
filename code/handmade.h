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
struct game_back_buffer
{
  void* bitmap;
  int width;
  int height;
  int pitch;
};

static void
Game_Update_And_Render(game_back_buffer* buffer);

#define HANDMADE_H
#endif