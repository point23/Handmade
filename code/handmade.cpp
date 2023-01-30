#include "handmade.h"

/* TEST drawing blud-green gradient */
static void
Render_Gradient(game_back_buffer* buffer, int x_offset, int y_offset)
{
  UINT8* row = (UINT8*)(buffer->bitmap);
  for (int y = 0; y < buffer->height; y++) {
    UINT32* pixel = (UINT32*)row;

    for (int x = 0; x < buffer->width; x++) {
      UINT8 blueCannel = (UINT8)(x + x_offset);
      UINT8 greenCannel = (UINT8)(y + y_offset);

      *pixel = (greenCannel << 8) | blueCannel;
      pixel += 1;
    }
    row += buffer->pitch;
  }
}

static void
Game_Update_And_Render(game_back_buffer* buffer)
{
  int x_offset = 0, y_offset = 0;
  Render_Gradient(buffer, x_offset, y_offset);
}