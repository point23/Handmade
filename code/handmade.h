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

#include <math.h>

// ==== Type extends ====
#define internal static
#define global static
#define local_persist static

#define real32 float
#define real64 double

#define s8 __int8
#define s16 __int16
#define s32 __int32
#define s64 __int64

#define u8 unsigned __int8
#define u16 unsigned __int16
#define u32 unsigned __int32
#define ulong unsigned long
#define u64 unsigned __int64

// ==== Consts ====
// real32 version of PI
#define PI32 3.14159265359f

// ==== Macros ====
#define get_array_size(arr) (sizeof(arr) / sizeof(arr[0]))
#define kilo_bytes(size) (size * 1024)
#define mega_bytes(size) (kilo_bytes(size) * 1024)
#define giga_bytes(size) (mega_bytes(size) * 1024)
#define tera_bytes(size) (giga_bytes(size) * 1024)

// ===== Assertion ====
#if HANDMADE_SLOW
#define assert(expr)                                                           \
    if (!(expr)) {                                                             \
        *(int *)0 = 0;                                                         \
    }
#else
#define assert(expr)
#endif

// ==== Structs ====
struct Game_Back_Buffer {
    void *bitmap;
    s32 width;
    s32 height;
    s32 pitch;
    s32 bytes_per_pixel;
};

struct Game_Sound_Buffer {
    s16 *samples;
    int samples_per_second;
    int sample_count;
};

struct Game_Button_State {
    int half_transition_count;
    bool ended_down;
};

struct Game_Controller_Input {
    bool is_analog;
    bool is_connected;
    // @note Controller hardware is sampling...
    real32 stick_average_x;
    real32 stick_average_y;

    union {
        Game_Button_State buttons[12];

        struct {
            Game_Button_State move_up;
            Game_Button_State move_down;
            Game_Button_State move_left;
            Game_Button_State move_right;
            Game_Button_State action_up;
            Game_Button_State action_down;
            Game_Button_State action_left;
            Game_Button_State action_right;
            Game_Button_State left_shoulder;
            Game_Button_State right_shoulder;
            Game_Button_State start;
            Game_Button_State end;
        };
    };
};

struct Game_Input {
    // @note
    // 0    - keyboard
    // 1..4 - controller
    Game_Controller_Input controllers[5];
    real32 dt_per_frame;
};

struct Game_Clocks {
    real32 seconds_elapsed;
};

struct Tilemap {
    u32* tiles;
};

struct World {
    s32 num_tilemap_cols;
    s32 num_tilmap_rows;

    s32 num_world_cols;
    s32 num_world_rows;

    real32 upper_left_x;
    real32 upper_left_y;

    real32 tile_width;
    real32 tile_height;

    Tilemap* tilemaps;
};

struct Game_State {
    Tilemap tilemaps[2];
    // Hero info...?
    s32 tilemap_x;
    s32 tilemap_y;
    real32 hero_x;
    real32  hero_y;
 };

struct Canonical_Position {
    s32 tilemap_x;
    s32 tilemap_y;
    s32 tile_x;
    s32 tile_y;

    // Tile related pos.
    real32 offset_x;
    real32 offset_y; 
};

struct Raw_Position {
    s32 tilemap_x;
    s32 tilemap_y;

    // Screen related pos.
    real32 x;
    real32 y;
};

struct Game_Memory {
    // @note Required to be cleared as 0 at startup
    u64 permanent_storage_size;
    void* permanent_storage;
    u64 transient_storage_size;
    void* transient_storage;
    bool is_initialized;
};

// ==== Functions ====
u32 safe_truncate_u64(u64 value) {
    assert(value <= 0xFFFFFFFF); // @todo max_val_of(pod)
    return (u32)value;
}

// @Note GUAR
#define GAME_UPDATE_AND_RENDER(name)                                           \
    void name(Game_Memory *memory, Game_Back_Buffer *back_buffer,              \
              Game_Input *input)
typedef GAME_UPDATE_AND_RENDER(guar);
GAME_UPDATE_AND_RENDER(Game_Update_And_Render_Stub) { return; }

//@Note GSS
#define GET_SOUND_SAMPLES(name) void name(Game_Sound_Buffer *sound_buffer)
typedef GET_SOUND_SAMPLES(gss);
GET_SOUND_SAMPLES(Get_Sound_Samples_Stub) { return; }

#if HANDMADE_INTERNAL
/*
  @note
  They are not for shipping...
 */
struct File_Result {
    void* content;
    u32 content_size;
};

#define DEBUG_PLATFORM_GET(name) void name(char *filename, File_Result *dest)
#define DEBUG_PLATFORM_PUT(name)                                               \
    bool name(char *filename, u32 buffer_size, void *buffer)
#define DEBUG_PLATFORM_FREE(name) void name(void *memory)

typedef DEBUG_PLATFORM_GET(defug_platform_get);
typedef DEBUG_PLATFORM_PUT(defug_platform_put);
typedef DEBUG_PLATFORM_FREE(defug_platform_free);
#endif

// #if HANDMADE_WIN32
// #include "windows.h"
// BOOL WINAPI
// DllMain(HINSTANCE hinstDLL, // handle to DLL module
//         DWORD fdwReason,    // reason for calling function
//         LPVOID lpReserved)  // reserved
// {
//     return TRUE; // Successful DLL_PROCESS_ATTACH.
// }
// #endif

#define HANDMADE_H
#endif