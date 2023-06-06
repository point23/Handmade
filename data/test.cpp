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
#include <math.h>

// @note Dynamic loaded
#include <dSound.h.>
#include <windows.h>
#include <xinput.h>

#include "handmade.cpp"

#define Direct_Sound_Buffer LPDIRECTSOUNDBUFFER

struct Win32_Back_Buffer
{
    BITMAPINFO bmi;
    void* memory;
    s32 width;
    s32 height;
    s32 bytes_per_pixel;
    s32 pitch;
};

struct Win32_Window_Dimension
{
    s32 width;
    s32 height;
};

struct Win32_Sound_Output
{
    const s32 samples_per_second = 48000;
    const s32 bytes_per_sample = sizeof(s16) * 2;
    /**
     * @note Bytes in sound buffer be like:
     * [ B B   B B    B B   B B  ]
     * [[Left Right] [Left Right]]
     */
    const s32 buffer_size = samples_per_second * bytes_per_sample;
    const s32 latency_sample_count = samples_per_second / 20;

    u32 running_sample_idx = 0;
};

/* @fixme It's not suppose to have static global variables. */
// Tell us if the game is running.
global bool Global_Running = false;

// A wrapper for properties related to bitmap(a graphic object),
// which we use to render our game world.
global Win32_Back_Buffer Global_Back_Buffer;

// Sound buffer that we actually write into.
global Direct_Sound_Buffer Global_Secondary_Sound_Buffer;

// CPU counts per second
global s64 Global_Perf_Frequency;

// @hack Debug log for now...
internal void
Win32_Debug_Log(LPCSTR info)
{
    OutputDebugStringA("[Win32] ");
    OutputDebugStringA(info);
    OutputDebugStringA("\n");
}

// @note File I/O Stuff
internal void
Debug_Platform_Get(char* filename, File_Result* dest)
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
                                  file_size64.QuadPart,
                                  MEM_COMMIT | MEM_RESERVE,
                                  PAGE_READWRITE);
            assert(dest);

            u32 file_size32 = safe_truncate_u64(file_size64.QuadPart);
            u32 bytes_read;
            if (ReadFile(file_handle,
                         result,
                         cast_to_dword(file_size32),
                         &cast_to_dword(bytes_read),
                         0) &&
                file_size32 == bytes_read) {
                dest->content_size = bytes_read;
                // Logging succeed
            } else {
                Debug_Platform_Free(result);
                result = 0;
            }
        }
        CloseHandle(file_handle);
    }

    dest->content = result;
}

internal void
Debug_Platform_Free(void* memory)
{
    if (memory == NULL)
        return;
    VirtualFree(memory, 0, MEM_RELEASE);
}

internal bool
Debug_Platform_Put(char* filename, u32 buffer_size, void* buffer)
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
        u32 bytes_written;
        if (WriteFile(file_handle,
                      buffer,
                      cast_to_dword(buffer_size),
                      &cast_to_dword(bytes_written),
                      0)) {
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
    buffer_description.dwFlags = 0;
    buffer_description.dwBufferBytes = buffer_size;
    buffer_description.lpwfxFormat = &wave_format;

    return dsound->CreateSoundBuffer(
      &buffer_description, &Global_Secondary_Sound_Buffer, 0);
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
    u32 /*out*/ region1_size;
    void* /*out*/ region2;
    u32 /*out*/ region2_size;
    bool buffer_locked = SUCCEEDED(
      Global_Secondary_Sound_Buffer->Lock(0,
                                          sound_output->buffer_size,
                                          &region1,
                                          &cast_to_dword(region1_size),
                                          &region2,
                                          &cast_to_dword(region2_size),
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

    Global_Secondary_Sound_Buffer->Unlock(
      region1, region1_size, region2, region2_size);

    Win32_Debug_Log("Secondary sound buffer is cleared.");
}

internal void
Win32_Try_Fill_Sound_Buffer(Win32_Sound_Output* sound_output,
                            u32 offset,
                            u32 bytes_to_write,
                            Game_Sound_Buffer* source_buffer)
{
    void* /*out*/ region1;
    u32 /*out*/ region1_size;
    void* /*out*/ region2;
    u32 /*out*/ region2_size;
    bool buffer_locked = SUCCEEDED(
      Global_Secondary_Sound_Buffer->Lock(offset,
                                          bytes_to_write,
                                          &region1,
                                          &cast_to_dword(region1_size),
                                          &region2,
                                          &cast_to_dword(region2_size),
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

    Global_Secondary_Sound_Buffer->Unlock(
      region1, region1_size, region2, region2_size);
}

// @note Bitmap Stuff
Win32_Window_Dimension
Win32_Get_Window_Diemnsion(HWND window)
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
    if (buffer->memory)
        VirtualFree(buffer->memory, 0, MEM_RELEASE);

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

    u64 memorySize = buffer->width * buffer->height * buffer->bytes_per_pixel;
    buffer->memory =
      VirtualAlloc(0, memorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
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
                  buffer->memory,
                  // Destination
                  &buffer->bmi,
                  DIB_RGB_COLORS,
                  SRCCOPY);
}

internal void
Win32_Process_XInput_Digital_Button(Game_Button_State* s_old,
                                    Game_Button_State* s_new,
                                    u32 button_state,
                                    u32 btn_bit)
{
    s_new->ended_down = button_state & btn_bit;
    s_new->half_transition_count =
      (s_old->ended_down != s_new->ended_down) ? 1 : 0;
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
            Global_Running = false;
            Win32_Debug_Log("Destroy window.");
        } break;

        case WM_CLOSE: {
            Global_Running = false;
            Win32_Debug_Log("Close window.");
        } break;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);

            // Display buffer to window
            Win32_Window_Dimension dimension =
              Win32_Get_Window_Diemnsion(window);
            Win32_Copy_Buffer_To_Window(deviceContext,
                                        &Global_Back_Buffer,
                                        dimension.width,
                                        dimension.height);

            EndPaint(window, &paint);
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            u32 vkcode = (u32)w_param;
            // bool was_down = (l_param & (1 << 30)) != 0;
            // bool is_down = (l_param & (1 << 31)) == 0;

            if (vkcode == 'W') {

            } else if (vkcode == 'A') {

            } else if (vkcode == 'S') {

            } else if (vkcode == 'D') {

            } else if (vkcode == VK_UP) {

            } else if (vkcode == VK_DOWN) {

            } else if (vkcode == VK_LEFT) {

            } else if (vkcode == VK_RIGHT) {

            } else if (vkcode == VK_ESCAPE) {

            } else if (vkcode == VK_SPACE) {
            }

            // @fixme There might be a waste of time trans s32 to bool
            bool altDown = l_param & (1 << 29);
            if (vkcode == VK_F4 && altDown) {
                Global_Running = false;
            }
        } break;

        default: {
            result = DefWindowProc(window, message, w_param, l_param);
        } break;
    }

    return result;
}

s32 CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE prev_instance,
        LPSTR cmd_line,
        s32 show_cmd)
{
    // Dynamically load functions
    Win32_Load_XInput();

    // Resize windows back buffer
    Win32_Resize_DIBSection(&Global_Back_Buffer, 1280, 720);

    // Retrieves performance frequency.
    LARGE_INTEGER perf_frequency_res;
    QueryPerformanceFrequency(&perf_frequency_res);
    Global_Perf_Frequency = perf_frequency_res.QuadPart;

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

    Global_Running = true;

    // Initialize secondary sound buffer
    Win32_Sound_Output sound_output;
    Win32_Init_DSound(
      window, sound_output.buffer_size, sound_output.samples_per_second);
    Win32_Clear_Sound_Buffer(&sound_output);
    Global_Secondary_Sound_Buffer->Play(0, 0, DSBPLAY_LOOPING);
    s16* sound_samples = (s16*)VirtualAlloc(
      0, sound_output.buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(sound_samples);

    // Debug stuff, query last clock-cycle count;
    LARGE_INTEGER last_counter;
    QueryPerformanceCounter(&last_counter);
    u64 last_cycle_count = __rdtsc();

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
    game_memory.is_initialized = false;
    game_memory.permanent_storage_size = mega_bytes(64);     // @hack
    game_memory.transient_storage_size = giga_bytes((u64)1); // @hack
    u64 total_size =
      game_memory.permanent_storage_size + game_memory.transient_storage_size;
    game_memory.permanent_storage = (u64*)VirtualAlloc(
      0, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    game_memory.transient_storage =
      (u8*)game_memory.permanent_storage + game_memory.permanent_storage_size;
    assert(game_memory.permanent_storage != NULL);
    assert(game_memory.transient_storage != NULL);

    char* filename = __FILE__; // @hack
    File_Result hack_result = {};
    Debug_Platform_Get(filename, &hack_result);
    if (hack_result.content_size == 0) {
        // @todo Logging
    }
    // @hack
    Debug_Platform_Put(
      "test.cpp", hack_result.content_size, hack_result.content);

    // Win32 main loop
    while (Global_Running) {
        MSG message = {};

        // @note Peek the newest message from queue without pending the
        // application
        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                Global_Running = false;

            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        { // Handle controller input

            s32 max_controller_count = XUSER_MAX_COUNT;
            if (max_controller_count > get_array_size(new_input->controllers)) {
                max_controller_count = get_array_size(new_input->controllers);
            }

            // @todo Should we ask for user input more frequently?
            for (s32 it_idx = 0; it_idx < max_controller_count; it_idx++) {
                Game_Controller_Input* old_controller =
                  &old_input->controllers[it_idx];
                Game_Controller_Input* new_controller =
                  &new_input->controllers[it_idx];

                XINPUT_STATE controller_state;
                u32 result = XInputGetState(it_idx, &controller_state);
                if (result != ERROR_SUCCESS)
                    continue;

                // Controller is connected
                XINPUT_GAMEPAD* gamepad = &controller_state.Gamepad;

                // D-pad
                bool dpad_up = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                bool dpad_down = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                bool dpad_left = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                bool dpad_right = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                // Buttons
                Win32_Process_XInput_Digital_Button(&old_controller->down,
                                                    &new_controller->down,
                                                    gamepad->wButtons,
                                                    XINPUT_GAMEPAD_A);
                Win32_Process_XInput_Digital_Button(&old_controller->up,
                                                    &new_controller->up,
                                                    gamepad->wButtons,
                                                    XINPUT_GAMEPAD_B);
                Win32_Process_XInput_Digital_Button(&old_controller->left,
                                                    &new_controller->left,
                                                    gamepad->wButtons,
                                                    XINPUT_GAMEPAD_X);
                Win32_Process_XInput_Digital_Button(&old_controller->right,
                                                    &new_controller->right,
                                                    gamepad->wButtons,
                                                    XINPUT_GAMEPAD_Y);
                // bool btn_start = gamepad->wButtons & XINPUT_GAMEPAD_START;
                // bool btn_back = gamepad->wButtons & XINPUT_GAMEPAD_BACK;

                // Shoulders
                Win32_Process_XInput_Digital_Button(
                  &old_controller->left_shoulder,
                  &new_controller->left_shoulder,
                  gamepad->wButtons,
                  XINPUT_GAMEPAD_LEFT_SHOULDER);
                Win32_Process_XInput_Digital_Button(
                  &old_controller->right_shoulder,
                  &new_controller->right_shoulder,
                  gamepad->wButtons,
                  XINPUT_GAMEPAD_RIGHT_SHOULDER);

                // Left thumbstick
                // @hack Only for debuging.
                // @todo Deadzone detection.
                real32 x;
                if (gamepad->sThumbLX < 0) {
                    x = (real32)gamepad->sThumbLX / 32768.0f;
                } else {
                    x = (real32)gamepad->sThumbLX / 32767.0f;
                }
                new_controller->max_x = new_controller->min_x =
                  new_controller->end_x = x;
                real32 y;
                if (gamepad->sThumbLX < 0) {
                    y = (real32)gamepad->sThumbLX / 32768.0f;
                } else {
                    y = (real32)gamepad->sThumbLX / 32767.0f;
                }
                new_controller->max_y = new_controller->min_y =
                  new_controller->end_y = y;
                new_controller->is_analog = true;

                // // Right thumbstick
                // u16 thumb_right_x = gamepad->sThumbRX;
                // u16 thumb_right_y = gamepad->sThumbRY;
            }

            // @todo Impl swap
            Game_Input* temp = new_input;
            new_input = old_input;
            old_input = temp;
        }

        u32 offset;
        u32 target_cursor;
        u32 bytes_to_write =
          0; // max  availables that we can write to the buffer.

        { // Calcu bytes need to be write to sound buffer
            u32 play_cursor;
            u32 write_cursor; // @Ignored: We are going to write the buffer on
                              // our own

            bool buffer_cursor_getted =
              SUCCEEDED(Global_Secondary_Sound_Buffer->GetCurrentPosition(
                &cast_to_dword(play_cursor), &cast_to_dword(write_cursor)));

            assert(buffer_cursor_getted);

            offset = (sound_output.running_sample_idx *
                      sound_output.bytes_per_sample) %
                     sound_output.buffer_size;

            target_cursor = (play_cursor + (sound_output.latency_sample_count *
                                            sound_output.bytes_per_sample)) %
                            sound_output.buffer_size;

            if (offset > target_cursor) {
                // | _ _ _ play~~~target *** offset _ _ _ |
                bytes_to_write = sound_output.buffer_size - offset;
                bytes_to_write += target_cursor;
            } else {
                // | *** offset _ _ _ play~~~target *** |
                bytes_to_write = target_cursor - offset;
            }
        }

        Game_Back_Buffer back_buffer = {};
        back_buffer.memory = Global_Back_Buffer.memory;
        back_buffer.width = Global_Back_Buffer.width;
        back_buffer.height = Global_Back_Buffer.height;
        back_buffer.pitch = Global_Back_Buffer.pitch;

        Game_Sound_Buffer sound_buffer = {};
        sound_buffer.samples = sound_samples;
        sound_buffer.samples_per_second = sound_output.samples_per_second;
        sound_buffer.sample_count =
          bytes_to_write / sound_output.bytes_per_sample;

        Game_Update(&game_memory, &back_buffer, &sound_buffer, new_input);

        { // Render to screen
            HDC dc = GetDC(window);
            Win32_Window_Dimension dimension =
              Win32_Get_Window_Diemnsion(window);
            Win32_Copy_Buffer_To_Window(
              dc, &Global_Back_Buffer, dimension.width, dimension.height);
            ReleaseDC(window, dc);
        }

        { // Fill sound output
            Win32_Try_Fill_Sound_Buffer(
              &sound_output, offset, bytes_to_write, &sound_buffer);
        }

        { // Display debug data
            /* @note query performance data. */
            LARGE_INTEGER end_counter;
            QueryPerformanceCounter(&end_counter);
            s64 counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
            u64 end_cycle_count = __rdtsc();
            u64 cycles_elapsed = end_cycle_count - last_cycle_count;

            // Milliseconds per frame
            float mspf =
              (1000.0f * (float)counter_elapsed) / (float)Global_Perf_Frequency;
            // Frames per second
            float fps = (float)Global_Perf_Frequency / (float)counter_elapsed;
            // Mega-Cycles per frame
            float mcpf = (float)cycles_elapsed / (1000.0f * 1000.0f);

            // char perf_log[256];
            // sprintf(perf_log, "%.2fms/f, %.2ff/s, %.2fmc/f\n", mspf, fps,
            // mcpf); Win32_Debug_Log(perf_log);

            // Update performance data of current frame.
            last_counter = end_counter;
            last_cycle_count = end_cycle_count;
        }
    }

    return 0;
}
