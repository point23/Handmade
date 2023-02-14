#if !defined(HANDMADE_H)

/*
 @todo
 Services that the platform provices to the game:
 - Timing
 - Controller/Keyboard Input
 - Bitmap Buffer
 - Sound Buffer
 - File I/O
 
 @note
 HANDMADE_INTERNAL:
  0 - Build for public 
  1 - Build for developer only

 HANDMADE_SLOW
  0 - Not slow code allowed
  1 - Slow code welcome
 */

// ==== Type extends ==== 
#define internal static
#define global static
#define local_persist static

#define real32 float
#define real64 float

#define int8 INT8
#define int16 INT16
#define int64 INT64

#define uint8 UINT8
#define uint16 UINT16
#define uint UINT32
#define uint64 UINT64

// ==== Consts ====

// Float version of PI
#define PI32 3.14159265359f

// ==== Macros ==== 
#define get_array_size(arr) (sizeof(arr) / sizeof(arr[0]))
#define kilo_bytes(size) (size * 1024)
#define mega_bytes(size) (kilo_bytes(size) * 1024)
#define giga_bytes(size) (mega_bytes(size) * 1024)
#define tera_bytes(size) (giga_bytes(size) * 1024)

// ===== Assertion ==== 
#if HANDMADE_SLOW
#define assert(expr) if (!(expr)) {*(int*)0 = 0;}
#else
#define assert(expr)
#endif

// ==== Structs ==== 
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

struct Game_Clocks
{
  real32 seconds_elapsed;
};

struct Game_State
{
  int blue_offset = 0;
  int green_offset = 0;
  int tone_hz = 256;
};

struct Game_Memory
{
  // @note Required to be cleared as 0 at startup
  uint64 permanent_storage_size;
  void* permanent_storage;
  uint64 transient_storage_size;
  void* transient_storage;
  bool is_initialized;
};

// ==== Functions ====
inline uint
safe_truncate_uint64(uint64 value)
{
  assert(value <= 0xFFFFFFFF); // @todo max_val_of(pod)
  return (uint)value;
}

internal void
Game_Update(Game_Memory* memory,
            Game_Back_Buffer* back_buffer,
            Game_Sound_Buffer* sound_buffer,
            Game_Input* input);

#if HANDMADE_INTERNAL
/*
  @note 
  They are not for shipping...
 */
struct File_Result
{
  void* content;
  uint content_size;
};

internal void Debug_Platform_Get(char* filename, File_Result* dest);

internal void Debug_Platform_Free(void* memory);

internal bool Debug_Platform_Put(char* filename, uint buffer_size, void* buffer);
#endif

#define HANDMADE_H
#endif
