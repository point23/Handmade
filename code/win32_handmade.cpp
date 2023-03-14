/*
 @todo WIN32 PLAYFORM LAYER:
  - A back-buffer to render.
  - A sound-buffer to fill.
  - Timing.
  - Asset loading path.
  - Lauch multi-threading.
  - Raw input.
  - Sleep/timeBeginPeriod.
  - ClipCursor (multimonitor support)
  - Fullscreen support.
  - Control cursor visibility (WM_SETCURSOR).
  - When we are not the active app (WM_ACTIVATE).
  - QueryCancelAutoplay.
  - Blit speed improvement.
  - Hardware acceleration (Direct3D/OpenGL).
  - GetKeyboardLayout (french keyboards/international wasd).
  - ...
*/

/*
 @fixme: Only used in dev mode, we should implement it ourselves
   in the future, we may need to get rid of any RUNTIME libararies.
   And it's some platform dependent stuff, we need to be able to calcu
   sinf()... on the Game-Layer instead of depend on something call out.
*/

// @note Dynamic loaded
#include "handmade.h"

#include "win32_handmade.h"

/* @fixme It's not suppose to have static global variables. */
// Tell us if the game is running.
global bool global_running = false;

// A wrapper for properties related to bitmap(a graphic object),
// which we use to render our game world.
global Win32_Back_Buffer global_back_buffer;

// Sound buffer that we actually write into.
global Direct_Sound_Buffer global_sound_buffer;

// CPU counts per second
global s64 global_perf_count_freq;

char SOURCE_DLL_NAME[] = "handmade.dll";
char TEMP_DLL_NAME[] = "handmade_temp.dll";

// @hack Debug log for now...
internal void
Win32_Debug_Log(LPCSTR info)
{
    OutputDebugStringA("[Win32] ");
    OutputDebugStringA(info);
    OutputDebugStringA("\n");
}

DEBUG_PLATFORM_FREE(Win32_Free)
{
    if (memory == NULL)
        return;
    VirtualFree(memory, 0, MEM_RELEASE);
}

// @note File I/O Stuff
// Use case:
//     char* filename = "";
//     File_Result res = {};
//     Win32_Get(filename, res);
//     if (res.content_size != 0) {
//         Win32_Put("test.cpp", res.content_size, res.content);
//     } else {
//         // Logging
//     }
//
DEBUG_PLATFORM_GET(Win32_Get)
{
    void* result = 0;
    HANDLE file_handle = CreateFileA(filename,
                                     GENERIC_READ,    // Access mode
                                     FILE_SHARE_READ, // Share mode
                                     NULL,
                                     OPEN_EXISTING, // Creation&Disposition
                                     0,
                                     NULL);

    if (file_handle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER file_size64;
        if (GetFileSizeEx(file_handle, &file_size64)) {
            result = VirtualAlloc(0,
                                  (u32)file_size64.QuadPart,
                                  MEM_COMMIT | MEM_RESERVE,
                                  PAGE_READWRITE);
            assert(dest);

            DWORD file_size32 = safe_truncate_u64(file_size64.QuadPart);
            DWORD bytes_read;
            if (ReadFile(file_handle, result, file_size32, &bytes_read, 0) &&
                file_size32 == bytes_read) {
                dest->content_size = bytes_read;
                // Logging succeed
            } else {
                Win32_Free(result);
                result = 0;
            }
        }
        CloseHandle(file_handle);
    }

    dest->content = result;
}

DEBUG_PLATFORM_PUT(Win32_Put)
{
    bool result = false;
    HANDLE file_handle = CreateFileA(filename,
                                     GENERIC_WRITE, // Access mode
                                     0,             // Share mode
                                     NULL,
                                     CREATE_ALWAYS, // Creation&Disposition
                                     0,
                                     NULL);

    if (file_handle != INVALID_HANDLE_VALUE) {
        DWORD bytes_written;
        if (WriteFile(file_handle, buffer, buffer_size, &bytes_written, 0)) {
            result = buffer_size == bytes_written;
        } else {
            // Logging
        }
        CloseHandle(file_handle);
    } else {
        // Logging
    }

    return result;
}

// Game Code Stuff
struct Win32_Game_Code
{
    HMODULE game_code_lib;
    FILETIME dll_last_write_time;
    game_update* Update;
    bool is_valid;
};

inline FILETIME
Win32_Get_File_Time(char* filename)
{
    FILETIME last = {};

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFileA(filename, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        FindClose(find_handle);
        last = find_data.ftLastWriteTime;
    }
    return last;
}

internal Win32_Game_Code
Win32_Load_Game_Code(char* source_dll_path, char* temp_dll_path)
{
    Win32_Game_Code result = {};

    result.dll_last_write_time = Win32_Get_File_Time(source_dll_path);

    CopyFile(source_dll_path, temp_dll_path, FALSE);
    HMODULE lib = LoadLibraryA(temp_dll_path);

    if (lib) {
        result.game_code_lib = lib;
        result.Update = (game_update*)GetProcAddress(lib, "Game_Update");

        result.is_valid = result.Update != NULL;
    }

    if (!result.is_valid) {
        result.is_valid = false;
        result.Update = Game_Update_Stub;
    }

    return result;
}

internal void
Win32_Unload_Game_Code(Win32_Game_Code* game)
{
    if (game->game_code_lib) {
        FreeLibrary(game->game_code_lib);
        game->game_code_lib = 0;
    }

    game->is_valid = false;
    game->Update = Game_Update_Stub;
}

// @note Controller Stuff
// XInput Get State
#define X_INPUT_GET_STATE(name)                                                \
    DWORD WINAPI name(DWORD userIdx, XINPUT_STATE* state)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
internal x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// XInput Set State
#define X_INPUT_SET_STATE(name)                                                \
    DWORD WINAPI name(DWORD userIdx, XINPUT_VIBRATION* vibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
internal x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// Load XInputGetState & XInputSetState
internal void
Win32_Load_XInput(void)
{
    HMODULE xinput_lib = LoadLibrary("xinput1_4.dll");
    if (!xinput_lib) {
        xinput_lib = LoadLibrary("xinput1_3.dll");
    }

    assert(xinput_lib != NULL);

    XInputGetState =
      (x_input_get_state*)GetProcAddress(xinput_lib, "XInputGetState");
    XInputSetState =
      (x_input_set_state*)GetProcAddress(xinput_lib, "XInputSetState");
}

// @note Audio Stuff
// Dynamically load function: DirectSoundCreate
#define DIRECT_SOUND_CREATE(name)                                              \
    HRESULT WINAPI name(                                                       \
      LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);
DIRECT_SOUND_CREATE(Direct_Sound_Create_Stub)
{
    return DSERR_INVALIDCALL;
}
static direct_sound_create* Direct_Sound_Create_ = Direct_Sound_Create_Stub;
#define Direct_Sound_Create Direct_Sound_Create_

inline void
Win32_Load_DSound()
{
    HMODULE dsound_lib = LoadLibrary("dsound.dll");
    assert(dsound_lib != NULL);

    Direct_Sound_Create =
      (direct_sound_create*)GetProcAddress(dsound_lib, "DirectSoundCreate");
}

inline void
Win32_Init_WaveFormat(WAVEFORMATEX* wave_format, s32 samples_per_second)
{
    wave_format->wFormatTag = WAVE_FORMAT_PCM;
    wave_format->nChannels = 2;
    wave_format->nSamplesPerSec = samples_per_second;
    wave_format->wBitsPerSample = 16;
    wave_format->nBlockAlign =
      wave_format->nChannels * wave_format->wBitsPerSample / 8;
    wave_format->nAvgBytesPerSec =
      samples_per_second * wave_format->nBlockAlign;
    wave_format->cbSize = 0;
}

inline HRESULT
Win32_Create_Primary_Buffer(LPDIRECTSOUND dsound,
                            LPDIRECTSOUNDBUFFER* primary_buffer)
{
    DSBUFFERDESC buffer_description = {};
    buffer_description.dwSize = sizeof(buffer_description);
    buffer_description.dwFlags = DSBCAPS_PRIMARYBUFFER;
    return dsound->CreateSoundBuffer(&buffer_description, primary_buffer, 0);
}

inline HRESULT
Win32_Create_Secondary_Buffer(LPDIRECTSOUND dsound,
                              WAVEFORMATEX wave_format,
                              s32 buffer_size)
{
    DSBUFFERDESC buffer_description = {};
    buffer_description.dwSize = sizeof(buffer_description);
    buffer_description.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
    buffer_description.dwBufferBytes = buffer_size;
    buffer_description.lpwfxFormat = &wave_format;

    return dsound->CreateSoundBuffer(
      &buffer_description, &global_sound_buffer, 0);
}

internal void
Win32_Init_DSound(HWND window, s32 bufferSize, s32 samplesPerSecond)
{
    Win32_Load_DSound();
    assert(Direct_Sound_Create != NULL);

    LPDIRECTSOUND dsound;
    bool dsound_created = SUCCEEDED(Direct_Sound_Create(0, &dsound, 0));
    assert(dsound_created);

    WAVEFORMATEX wave_format = {};
    Win32_Init_WaveFormat(&wave_format, samplesPerSecond);

    bool cooperative_level_settled =
      SUCCEEDED(dsound->SetCooperativeLevel(window, DSSCL_PRIORITY));
    assert(cooperative_level_settled);

    LPDIRECTSOUNDBUFFER primary_buffer;
    bool primary_buffer_created =
      SUCCEEDED(Win32_Create_Primary_Buffer(dsound, &primary_buffer));
    assert(primary_buffer_created);

    bool wave_format_settled =
      SUCCEEDED(primary_buffer->SetFormat(&wave_format));
    assert(wave_format_settled);

    bool secondary_buffer_created =
      SUCCEEDED(Win32_Create_Secondary_Buffer(dsound, wave_format, bufferSize));
    assert(secondary_buffer_created);
}

internal void
Win32_Clear_Sound_Buffer(Win32_Sound_Output* sound_output)
{
    void* /*out*/ region1;
    DWORD /*out*/ region1_size;
    void* /*out*/ region2;
    DWORD /*out*/ region2_size;
    bool buffer_locked =
      SUCCEEDED(global_sound_buffer->Lock(0,
                                          sound_output->buffer_size,
                                          &region1,
                                          &region1_size,
                                          &region2,
                                          &region2_size,
                                          0));

    assert(buffer_locked);

    u8* write_cursor = (u8*)region1;
    for (u32 i = 0; i < region1_size; i++) {
        *write_cursor++ = 0;
    }

    write_cursor = (u8*)region2;
    for (u32 i = 0; i < region2_size; i++) {
        *write_cursor++ = 0;
    }

    global_sound_buffer->Unlock(region1, region1_size, region2, region2_size);

    Win32_Debug_Log("Secondary sound buffer is cleared.");
}

internal void
Win32_Try_Fill_Sound_Buffer(Win32_Sound_Output* sound_output,
                            u32 offset,
                            u32 bytes_to_write,
                            Game_Sound_Buffer* source_buffer)
{
    void* /*out*/ region1;
    DWORD /*out*/ region1_size;
    void* /*out*/ region2;
    DWORD /*out*/ region2_size;
    bool buffer_locked = SUCCEEDED(global_sound_buffer->Lock(offset,
                                                             bytes_to_write,
                                                             &region1,
                                                             &region1_size,
                                                             &region2,
                                                             &region2_size,
                                                             0));

    // assert(buffer_locked);
    if (!buffer_locked) {
        return;
    }

    // Write region1
    u32 region1_sample_count = region1_size / sound_output->bytes_per_sample;
    s16* dest_sample = (s16*)region1;
    s16* source_sample = source_buffer->samples;

    for (u32 i = 0; i < region1_sample_count; i++) {
        *dest_sample++ = *source_sample++; // Left
        *dest_sample++ = *source_sample++; // Right
        sound_output->running_sample_idx += 1;
    }

    // Write region2
    u32 region2_sample_count = region2_size / sound_output->bytes_per_sample;
    dest_sample = (s16*)region2;
    for (u32 i = 0; i < region2_sample_count; i++) {
        *dest_sample++ = *source_sample++; // Left
        *dest_sample++ = *source_sample++; // Right
        sound_output->running_sample_idx += 1;
    }

    global_sound_buffer->Unlock(region1, region1_size, region2, region2_size);
}

// @note Bitmap Stuff
Win32_Window_Dimension
Win32_Get_Window_Dimension(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);

    s32 width = clientRect.right - clientRect.left;
    s32 height = clientRect.bottom - clientRect.top;

    return { width, height };
}

internal void
Win32_Resize_DIBSection(Win32_Back_Buffer* buffer, s32 width, s32 height)
{
    // @todo Add some Virtual Protect stuff in the future, we may want older
    // buffers for some reason.

    // Clear the old buffer.
    if (buffer->bitmap)
        VirtualFree(buffer->bitmap, 0, MEM_RELEASE);

    buffer->width = width;
    buffer->height = height;
    buffer->bytes_per_pixel = 4;

    // @note Init bitmapInfo, mainly bmiHeader
    BITMAPINFOHEADER& bmiHeader = buffer->bmi.bmiHeader;
    bmiHeader.biSize = sizeof(bmiHeader);
    bmiHeader.biWidth = buffer->width;
    // Neg height means Top-Down DIB with origin at upper-left
    bmiHeader.biHeight = -buffer->height;
    // This value must be set to 1.
    bmiHeader.biPlanes = 1;
    bmiHeader.biBitCount = (u16)(buffer->bytes_per_pixel * 8);
    bmiHeader.biCompression = BI_RGB;

    u32 memory_size = buffer->width * buffer->height * buffer->bytes_per_pixel;
    buffer->bitmap =
      VirtualAlloc(0, memory_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    buffer->pitch = buffer->width * buffer->bytes_per_pixel;
}

internal void
Win32_Copy_Buffer_To_Window(HDC deviceContext,
                            Win32_Back_Buffer* buffer,
                            s32 window_width,
                            s32 window_height)
{
    // @todo Aspect ratio correction
    StretchDIBits(deviceContext,
                  // Destination rectangle
                  0,
                  0,
                  window_width,
                  window_height,
                  // Source rectangle
                  0,
                  0,
                  buffer->width,
                  buffer->height,
                  // Source
                  buffer->bitmap,
                  // Destination
                  &buffer->bmi,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal void
Win32_Process_XInput_Digital_Button(Game_Button_State* s_old,
                                    Game_Button_State* s_new,
                                    bool is_down)
{
    s_new->ended_down = is_down;
    s_new->half_transition_count = (s_old->ended_down != is_down) ? 1 : 0;
}

internal real32
Win32_Process_XInput_Stick_Value(s32 dead_zone, real32 axis_value)
{
    const real32 abs_max_axis_val = 32768.0f;
    const real32 abs_min_axis_val = 32767.0f;

    real32 result = 0;
    if (axis_value < -dead_zone) {
        result = axis_value / abs_max_axis_val;
    } else if (axis_value > dead_zone) {
        result = axis_value / abs_min_axis_val;
    }
    return result;
}

internal void
Win32_Process_Keyboard_Message(Game_Button_State* state, bool is_down)
{
    assert(state->ended_down != is_down);
    state->ended_down = is_down;
    state->half_transition_count++;
}

/*
 @note
 - Functional func: take things in, and return new things back.
 - Func with side effects: take things in, and modify it.
*/
internal void
Win32_Process_Pending_Messages(Game_Controller_Input* keyboard_controller)
{
    // @note Peek the newest message from queue without pending the
    // application
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT)
            global_running = false;

        switch (message.message) {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                // 30 == 1, 31 == 0 | 1,
            case WM_KEYDOWN:
                // 30 == 1, 31 == 1
            case WM_KEYUP: {
                u32 vk_code = (u32)message.wParam;
                u32 l_param = (u32)message.lParam;

                bool was_down = (l_param & (1 << 30)) != 0;
                bool is_down = (l_param & (1 << 31)) == 0;

                if (was_down == is_down)
                    break; // @note Only handle it when btn state changed

                if (vk_code == 'W') {
                    Win32_Process_Keyboard_Message(
                      &keyboard_controller->move_up, is_down);
                } else if (vk_code == 'A') {
                    Win32_Process_Keyboard_Message(
                      &keyboard_controller->move_left, is_down);
                } else if (vk_code == 'S') {
                    Win32_Process_Keyboard_Message(
                      &keyboard_controller->move_down, is_down);
                } else if (vk_code == 'D') {
                    Win32_Process_Keyboard_Message(
                      &keyboard_controller->move_right, is_down);
                } else if (vk_code == VK_UP) {
                    Win32_Process_Keyboard_Message(
                      &keyboard_controller->action_up, is_down);
                } else if (vk_code == VK_DOWN) {
                    Win32_Process_Keyboard_Message(
                      &keyboard_controller->action_down, is_down);
                } else if (vk_code == VK_LEFT) {
                    Win32_Process_Keyboard_Message(
                      &keyboard_controller->action_left, is_down);
                } else if (vk_code == VK_RIGHT) {
                    Win32_Process_Keyboard_Message(
                      &keyboard_controller->action_right, is_down);
                } else if (vk_code == VK_ESCAPE) {
                } else if (vk_code == VK_SPACE) {
                }

                // @fixme There might be a waste of time trans s32 to bool
                bool alt_down = l_param & (1 << 29);
                if (vk_code == VK_F4 && alt_down) {
                    global_running = false;
                }
            } break;
            default: {
                TranslateMessage(&message);
                DispatchMessage(&message);
            } break;
        }
    }
}

// @note A callback function, which you define in your application, that
// processes messages sent to a window.
LRESULT CALLBACK
Win32_Main_Window_Callback(HWND window,
                           UINT message,
                           WPARAM w_param,
                           LPARAM l_param)
{
    LRESULT result = 0;
    switch (message) {
        case WM_ACTIVATEAPP: {
            Win32_Debug_Log("Activated.");
        } break;

        case WM_SIZE: {
            Win32_Debug_Log("Resize window.");
        } break;

        case WM_DESTROY: {
            global_running = false;
            Win32_Debug_Log("Destroy window.");
        } break;

        case WM_CLOSE: {
            global_running = false;
            Win32_Debug_Log("Close window.");
        } break;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);

            // Display buffer to window
            Win32_Window_Dimension dimension =
              Win32_Get_Window_Dimension(window);
            Win32_Copy_Buffer_To_Window(deviceContext,
                                        &global_back_buffer,
                                        dimension.width,
                                        dimension.height);

            EndPaint(window, &paint);
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            // @note We're suppose to handle key input inside the main loop
            assert(false);
        } break;

        default: {
            result = DefWindowProc(window, message, w_param, l_param);
        } break;
    }

    return result;
}

inline u64
Win32_Get_Perf_Counter()
{
    LARGE_INTEGER res;
    QueryPerformanceCounter(&res);
    return res.QuadPart;
}

inline real32
Win32_Get_Seconds_Elapsed(u64 start, u64 end)
{
    return ((real32)(end - start) / (real32)global_perf_count_freq);
}

s32 CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE prev_instance,
        LPSTR cmd_line,
        s32 show_cmd)
{
    u32 desired_scheduler_ms = 1;
    bool sleep_is_granular =
      timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR;

    char exe_path[MAX_PATH];
    DWORD fliename_size = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    char* one_past_last_slash = exe_path;
    for (char* i = exe_path + fliename_size;; --i) {
        if (*i == '\\') {
            one_past_last_slash = i + 1;
            break;
        }
    }

    u64 exe_dir_length = one_past_last_slash - exe_path;

    char source_dll_path[MAX_PATH];
    strncpy_s(
      source_dll_path, sizeof(source_dll_path), exe_path, exe_dir_length);
    strncpy_s(source_dll_path + exe_dir_length,
              sizeof(source_dll_path),
              SOURCE_DLL_NAME,
              sizeof(SOURCE_DLL_NAME));

    char temp_dll_path[MAX_PATH];
    strncpy_s(temp_dll_path, sizeof(temp_dll_path), exe_path, exe_dir_length);
    strncpy_s(temp_dll_path + exe_dir_length,
              sizeof(temp_dll_path),
              TEMP_DLL_NAME,
              sizeof(TEMP_DLL_NAME));

    // Dynamically loaded stuff
    Win32_Load_XInput();
    Win32_Game_Code game = Win32_Load_Game_Code(source_dll_path, temp_dll_path);

    // Resize windows back buffer
    Win32_Resize_DIBSection(&global_back_buffer, 1280, 720);

    // Retrieves performance frequency.
    LARGE_INTEGER perf_freq_res;
    QueryPerformanceFrequency(&perf_freq_res);
    global_perf_count_freq = perf_freq_res.QuadPart;

    WNDCLASS window_class = {};
    window_class.style =
      CS_HREDRAW | CS_VREDRAW; // Redraw when adjust or move happend vertically
                               // or horizontally.
    window_class.lpfnWndProc = Win32_Main_Window_Callback;
    window_class.hInstance = instance;
    window_class.lpszClassName = "Handmade_Window_Class";
    /* @todo Setup icon for this app: window_class.hIcon */
    bool registered = RegisterClass(&window_class);
    assert(registered);

    HWND window = CreateWindowEx(0,
                                 window_class.lpszClassName,
                                 "Handmade",
                                 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 0,
                                 0,
                                 instance,
                                 0);
    assert(window);

    // Initialize secondary sound buffer
    Win32_Sound_Output sound_output;
    Win32_Init_DSound(
      window, sound_output.buffer_size, sound_output.samples_per_second);
    Win32_Clear_Sound_Buffer(&sound_output);
    global_sound_buffer->Play(0, 0, DSBPLAY_LOOPING);

    s16* sound_samples = (s16*)VirtualAlloc(
      0, sound_output.buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(sound_samples);

    // Init game input
    Game_Input inputs[2] = {};
    Game_Input* old_input = &inputs[0];
    Game_Input* new_input = &inputs[1];

    // Allocate game memory
#if HANDMADE_INTERNAL
    LPVOID base_addr = (LPVOID)tera_bytes((u64)2);
#else
    LPVOID base_addr = 0;
#endif

    Game_Memory game_memory = {};
    { // Init game memory
        game_memory.is_initialized = false;
        game_memory.permanent_storage_size =
          mega_bytes(64); // @todo Config this value
        game_memory.transient_storage_size =
          giga_bytes((u64)1); // @todo Config this value
        u64 total_size = game_memory.permanent_storage_size +
                         game_memory.transient_storage_size;
        game_memory.permanent_storage = (u64*)VirtualAlloc(
          0, (u32)total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        game_memory.transient_storage = (u8*)game_memory.permanent_storage +
                                        game_memory.permanent_storage_size;
        assert(game_memory.permanent_storage != NULL);
        assert(game_memory.transient_storage != NULL);
    }

    // Frame rate stuff
    u64 last_counter = Win32_Get_Perf_Counter();
    // u64 last_cycle_count = __rdtsc(); // @todo Get rid of __rdtsc()
    const real32 target_seconds_per_frame = (1.0f / (real32)game_update_rate);

    // Audio stuff
    u64 flip_counter = Win32_Get_Perf_Counter();
    bool sound_is_valid = false;

    // Let's rock...
    global_running = true;

    // Win32 main loop
    while (global_running) {
        FILETIME new_write_time = Win32_Get_File_Time(SOURCE_DLL_NAME);
        if (CompareFileTime(&new_write_time, &game.dll_last_write_time)) {
            Win32_Unload_Game_Code(&game);
            game = Win32_Load_Game_Code(source_dll_path, temp_dll_path);
        }

        Game_Controller_Input* keyboard_old = &old_input->controllers[0];
        Game_Controller_Input* keyboard_new = &new_input->controllers[0];
        Game_Controller_Input c_empty = {};
        *keyboard_new = c_empty;
        for (u32 idx = 0; idx < get_array_size(keyboard_old->buttons); idx++) {
            keyboard_new->buttons[idx].ended_down =
              keyboard_old->buttons[idx].ended_down;
        }

        Win32_Process_Pending_Messages(keyboard_new);

        { // Handle controller input
            u32 max_controller_count = XUSER_MAX_COUNT + 1;
            assert(max_controller_count <=
                   get_array_size(new_input->controllers));

            // @todo Should we ask for user input more frequently?
            for (u32 it_idx = 1; it_idx <= max_controller_count; it_idx++) {
                Game_Controller_Input* old_controller =
                  &old_input->controllers[it_idx];
                Game_Controller_Input* new_controller =
                  &new_input->controllers[it_idx];

                XINPUT_STATE controller_state;
                u32 result = XInputGetState(it_idx - 1, &controller_state);
                if (result != ERROR_SUCCESS) {
                    new_controller->is_connected = false;
                    continue;
                }

                new_controller->is_connected = true;
                const real32 stick_move_threshold = 0.5;

                // Controller is connected
                XINPUT_GAMEPAD* gamepad = &controller_state.Gamepad;

                { // @note Detect dead zone for analog: stick/triger
                    // Left thumbstick
                    real32 x = Win32_Process_XInput_Stick_Value(
                      XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, gamepad->sThumbLX);
                    real32 y = Win32_Process_XInput_Stick_Value(
                      XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, gamepad->sThumbLY);

                    new_controller->stick_average_x = x;
                    new_controller->stick_average_y = y;
                    new_controller->is_analog = true;

                    // // Right thumbstick
                    // gamepad->sThumbRX;
                    // gamepad->sThumbRY;
                }

                { // D-pad
                    bool dpad_up = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                    bool dpad_down =
                      gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                    bool dpad_left =
                      gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                    bool dpad_right =
                      gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                    if (dpad_up)
                        new_controller->stick_average_y = 1.0f;
                    if (dpad_down)
                        new_controller->stick_average_y = -1.0f;
                    if (dpad_left)
                        new_controller->stick_average_x = -1.0f;
                    if (dpad_right)
                        new_controller->stick_average_x = 1.0f;
                }

                { // Fake buttons
                    real32& x = new_controller->stick_average_x;
                    real32& y = new_controller->stick_average_y;

                    Win32_Process_XInput_Digital_Button(
                      &old_controller->move_down,
                      &new_controller->move_down,
                      (y < -stick_move_threshold) ? 1 : 0);
                    Win32_Process_XInput_Digital_Button(
                      &old_controller->move_up,
                      &new_controller->move_up,
                      (y > stick_move_threshold) ? 1 : 0);
                    Win32_Process_XInput_Digital_Button(
                      &old_controller->move_left,
                      &new_controller->move_left,
                      (x > stick_move_threshold) ? 1 : 0);
                    Win32_Process_XInput_Digital_Button(
                      &old_controller->move_right,
                      &new_controller->move_right,
                      (x < -stick_move_threshold) ? 1 : 0);
                }

                { // Buttons
                    Win32_Process_XInput_Digital_Button(
                      &old_controller->action_down,
                      &new_controller->action_down,
                      gamepad->wButtons & XINPUT_GAMEPAD_A);
                    Win32_Process_XInput_Digital_Button(
                      &old_controller->action_up,
                      &new_controller->action_up,
                      gamepad->wButtons & XINPUT_GAMEPAD_B);
                    Win32_Process_XInput_Digital_Button(
                      &old_controller->action_left,
                      &new_controller->action_left,
                      gamepad->wButtons & XINPUT_GAMEPAD_X);
                    Win32_Process_XInput_Digital_Button(
                      &old_controller->action_right,
                      &new_controller->action_right,
                      gamepad->wButtons & XINPUT_GAMEPAD_Y);
                    Win32_Process_XInput_Digital_Button(&old_controller->end,
                                                        &new_controller->end,
                                                        gamepad->wButtons &
                                                          XINPUT_GAMEPAD_BACK);
                    Win32_Process_XInput_Digital_Button(&old_controller->start,
                                                        &new_controller->start,
                                                        gamepad->wButtons &
                                                          XINPUT_GAMEPAD_START);
                }

                { // Shoulders
                    Win32_Process_XInput_Digital_Button(
                      &old_controller->left_shoulder,
                      &new_controller->left_shoulder,
                      gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);

                    Win32_Process_XInput_Digital_Button(
                      &old_controller->right_shoulder,
                      &new_controller->right_shoulder,
                      gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                }
            }

            // @todo Impl swap
            Game_Input* temp = new_input;
            new_input = old_input;
            old_input = temp;
        }

        u32 bytes_to_write = 0, audio_lock_offset = 0;
        { // Calcu bytes need to be write to sound buffer
            const u32 sound_bytes_per_frame = (sound_output.samples_per_second *
                                               sound_output.bytes_per_sample) /
                                              game_update_rate; // 30hz

            DWORD play_cursor;
            DWORD write_cursor;
            if (global_sound_buffer->GetCurrentPosition(
                  &play_cursor, &write_cursor) == DS_OK) {
                if (!sound_is_valid) {
                    // In the first frame, we have set the running sample idx
                    // to make sure we're writing from the write cursor.

                    sound_output.running_sample_idx =
                      write_cursor / sound_output.bytes_per_sample;
                    sound_is_valid = true;
                }

                audio_lock_offset = (sound_output.running_sample_idx *
                                     sound_output.bytes_per_sample) %
                                    sound_output.buffer_size;

                // f: Flip counter
                // a: Audio counter
                //                    last frame   current frame
                // Logic Frames: ...|...........F|--A.........|...
                real32 from_last_flip_to_audio = Win32_Get_Seconds_Elapsed(
                  flip_counter, Win32_Get_Perf_Counter());

                // *: We still expect to hear the audio of that frame
                //      the same time we see we see that frame
                //      so theres has to be some latency.
                // Logic Frames: ...|...........F|--A**********|...
                real32 pending_seconds =
                  target_seconds_per_frame - from_last_flip_to_audio;
                DWORD pending_bytes =
                  (DWORD)(pending_seconds / target_seconds_per_frame *
                          (real32)sound_bytes_per_frame);

                // e: Expected cursor is where we expect to start
                //      to fill the audio.
                // Sound-buffer: [..p..w.E......]
                DWORD expected_cursor = play_cursor + pending_bytes;

                DWORD safe_write_cursor = write_cursor;
                if (safe_write_cursor < play_cursor) {
                    // Case: Write-Cursor = (Play-Cursor + 5760/4) % Buffer-Size

                    safe_write_cursor += sound_output.buffer_size;
                }

                safe_write_cursor += sound_output.safety_bytes;
                bool low_latency = safe_write_cursor <= expected_cursor;

                // Target cursor indicates where we should write to.
                u32 target_cursor = 0;
                if (low_latency) {
                    target_cursor = expected_cursor + sound_bytes_per_frame;
                } else {
                    target_cursor = write_cursor + sound_output.safety_bytes +
                                    sound_bytes_per_frame;
                }

                target_cursor %= sound_output.buffer_size;

                if (audio_lock_offset > target_cursor) {
                    // [*******t...o*****]
                    bytes_to_write =
                      sound_output.buffer_size - audio_lock_offset;
                    bytes_to_write += target_cursor;
                } else {
                    // [.....o****t....]
                    bytes_to_write = target_cursor - audio_lock_offset;
                }

#if HANDMADE_INTERNAL
                DWORD unwrapped_write_cursor = write_cursor;
                if (unwrapped_write_cursor < play_cursor) {
                    unwrapped_write_cursor += sound_output.buffer_size;
                }

                char audio_log[256];
                sprintf_s(
                  audio_log,
                  "lock offset: %d, target_cursor: %d, bytes_to_write: %d, "
                  "play_cursor: %d, write_cursor: %d",
                  audio_lock_offset,
                  target_cursor,
                  bytes_to_write,
                  play_cursor,
                  write_cursor);
                // Win32_Debug_Log(audio_log);
#endif
            } else {
                sound_is_valid = false;
            }
        }

        Game_Back_Buffer back_buffer = {};
        back_buffer.bitmap = global_back_buffer.bitmap;
        back_buffer.width = global_back_buffer.width;
        back_buffer.height = global_back_buffer.height;
        back_buffer.pitch = global_back_buffer.pitch;

        Game_Sound_Buffer sound_buffer = {};
        sound_buffer.samples = sound_samples;
        sound_buffer.samples_per_second = sound_output.samples_per_second;
        sound_buffer.sample_count =
          bytes_to_write / sound_output.bytes_per_sample;

        game.Update(&game_memory, &back_buffer, &sound_buffer, new_input);

        { // Render to screen
            HDC dc = GetDC(window);
            Win32_Window_Dimension dimension =
              Win32_Get_Window_Dimension(window);
            Win32_Copy_Buffer_To_Window(
              dc, &global_back_buffer, dimension.width, dimension.height);
            ReleaseDC(window, dc);
        }

        //   logic#1        logic#2
        // |............|............|..........
        //              F   render#1     render#2
        // Flip is when we render the current frame
        flip_counter = Win32_Get_Perf_Counter();

        { // Fill sound output
            Win32_Try_Fill_Sound_Buffer(
              &sound_output, audio_lock_offset, bytes_to_write, &sound_buffer);
        }

        { // Consume the rest frame time and display debug data
            /* @note query performance data. */
            real32 seconds_elapsed =
              Win32_Get_Seconds_Elapsed(last_counter, Win32_Get_Perf_Counter());

            // Slow things down to make sure that we hit the target frame rate
            if (seconds_elapsed < target_seconds_per_frame) {
                // if (sleep_is_granular) {
                //     DWORD sleep_duration_ms =
                //       (DWORD)(1000.0f *
                //               (target_seconds_per_frame - seconds_elapsed));
                //     if (sleep_duration_ms > 0)
                //         Sleep(sleep_duration_ms);
                // } else {
                while (Win32_Get_Seconds_Elapsed(last_counter,
                                                 Win32_Get_Perf_Counter()) <
                       target_seconds_per_frame) {
                    // Do nothing...
                }
                // }
            } else {
                Win32_Debug_Log("==============miss a frame==============");
            }

            // Update performance data of current frame.
            u64 end_counter = Win32_Get_Perf_Counter();
            u64 counter_elapsed = end_counter - last_counter;
            u64 end_cycle_count = __rdtsc();
            // u64 cycles_elapsed = end_cycle_count - last_cycle_count;

#if HANDMADE_INTERNAL // Debug log perf info
            // Milliseconds per frame
            real32 mspf =
              (1000.0f * Win32_Get_Seconds_Elapsed(last_counter, end_counter));
            // Frames per second
            s32 fps =
              (s32)((real32)global_perf_count_freq / (real32)counter_elapsed);
            // // Mega-Cycles per frame
            // real32 mcpf = (real32)cycles_elapsed / (1000.0f * 1000.0f);

            char perf_log[256];
            sprintf_s(perf_log, "%.2fms/f, %df/s\n", mspf, fps);

            // Win32_Debug_Log(perf_log);
#endif
            last_counter = end_counter;
            // last_cycle_count = end_cycle_count;
        }
    }

    return 0;
}
