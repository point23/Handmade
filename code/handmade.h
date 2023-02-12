#if !defined(HANDMADE_H)

/**
 * @todo
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

#define get_array_size(arr) (sizeof(arr) / sizeof(arr[0]))

// Float version of PI
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

struct Game_Button_State
{
  int half_transition_count;
  bool ended_down;
};

struct Game_Controller_Input
{
  bool is_analog;
  // @note Controller hardware is sampling...
  real32 start_x;
  real32 start_y;
  real32 end_x;
  real32 end_y;

  real32 min_x;
  real32 min_y;
  real32 max_x;
  real32 max_y;

  union
  {
    Game_Button_State buttons[6];

    struct
    {
      Game_Button_State up;
      Game_Button_State down;
      Game_Button_State left;
      Game_Button_State right;
      Game_Button_State left_shoulder;
      Game_Button_State right_shoulder;
    };
  };
};

struct Game_Input
{
  Game_Controller_Input controllers[4];
};

static void
Game_Update(Game_Back_Buffer* back_buffer,
            Game_Sound_Buffer* sound_buffer,
            Game_Input* input);

#define HANDMADE_H
#endif