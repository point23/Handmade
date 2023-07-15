#if !defined(HANDMADE_PLATFORM_H)

// @Note Comilers
#if !defined(COMPILER_MSVC)
#if _MSC_VER
#define COMPILER_MSVC 1
#else
#define COMPILER_MSVC 0
#endif 
#endif

 #if COMPILER_MSVC
 #include <intrin.h>
 #endif


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

struct Thread_Context {
    s32 placeholder;
};

#ifdef HANDMADE_INTERNAL
// @Note These are not for shipping...
struct File_Result {
    void* content;
    u32 content_size;
};

#define DEBUG_PLATFORM_READ_FILE(name) void name(Thread_Context* thread, char *filename, File_Result *dest)
#define DEBUG_PLATFORM_WRITE_FILE(name) bool name(Thread_Context* thread, char *filename, u32 buffer_size, void *buffer)
#define DEBUG_PLATFORM_FREE_FILE(name) void name(Thread_Context* thread, void *memory)

typedef DEBUG_PLATFORM_READ_FILE(debug_platform_read_file);
typedef DEBUG_PLATFORM_WRITE_FILE(debug_platform_write_file);
typedef DEBUG_PLATFORM_FREE_FILE(debug_platform_free_file);
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

// Platform debug methods.
#define PLATFORM_PRINT(name) void name(const char* format, ...)
typedef PLATFORM_PRINT(platform_print);
#define PLATFORM_PRINT_POINTER(name) void (*name)(const char* format, ...)

// ==== Structs ====
struct Game_Back_Buffer {
    void *memory;
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

struct Game_Memory {
    // @note Required to be cleared as 0 at startup
    u64 permanent_storage_size;
    void* permanent_storage;
    u64 transient_storage_size;
    void* transient_storage;
    bool is_initialized;

    u32* bmp_data;

    // Platform File W/R Methods
    debug_platform_read_file* Debug_Platform_Read_File;
    debug_platform_write_file* Debug_Platform_Write_File;
    debug_platform_free_file* Debug_Platform_Free_File;

    // Debug methods
    platform_print* Platform_Print;
};

// ===== Assertions ====
#if HANDMADE_SLOW
#define assert(expr)                                                           \
    if (!(expr)) {                                                             \
        *(int *)0 = 0;                                                         \
    }
#else
#define assert(expr)
#endif

// ==== Functions ==== 
u32 safe_truncate_u64(u64 value) {
    assert(value <= 0xFFFFFFFF); // @todo max_val_of(pod)
    return (u32)value;
}

// @Note GUAR
#define GAME_UPDATE_AND_RENDER(name)                                           \
    void name(Thread_Context* thread, Game_Memory *memory, Game_Back_Buffer *back_buffer, Game_Input *input)
typedef GAME_UPDATE_AND_RENDER(guar);
GAME_UPDATE_AND_RENDER(Game_Update_And_Render_Stub) { return; }

//@Note GSS
#define GET_SOUND_SAMPLES(name) void name(Thread_Context* thread, Game_Sound_Buffer *sound_buffer)
typedef GET_SOUND_SAMPLES(gss);
GET_SOUND_SAMPLES(Get_Sound_Samples_Stub) { return; }

#define HANDMADE_PLATFORM_H
#endif