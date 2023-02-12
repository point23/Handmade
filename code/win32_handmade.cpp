#include <dSound.h.>
#include <windows.h>
#include <xinput.h>

#include <assert.h>

/* @fixme: Only used in dev mode, we should implement it ourselves in the      \
 * future, we may need to get rid of RUNTIME libararies. */                    \
#include <math.h>
#include <stdio.h>

#include "handmade.cpp"

/* @todo WIN PLAYFORM LAYER SHOULD DO:
  - A back-buffer to render.
  - A sound-buffer to fill.
  - Timing.
  - Saved game location.
  - Getting handle to own executable file.
  - Asset loading path.
  - multi threading.
  - Raw input (support for multiple keyboards)
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

#define Direct_Sound_Buffer LPDIRECTSOUNDBUFFER

struct Win32_Back_Buffer
{
  BITMAPINFO bmi;
  void* bitmap;
  int width;
  int height;
  int bytes_per_pixel;
  int pitch;
};

struct Win32_Window_Dimension
{
  int width;
  int height;
};

struct Win32_Sound_Output
{
  const int Samples_Per_Second = 48000;
  const int Bytes_Per_Sample = sizeof(INT16) * 2;
  /**
   * @note Bytes in sound buffer be like:
   * [ B B   B B    B B   B B  ]
   * [[Left Right] [Left Right]]
   */
  const int Secondary_Buffer_Size = Samples_Per_Second * Bytes_Per_Sample;
  const int Latency_Sample_Count = Samples_Per_Second / 20;

  uint running_Sample_idx = 0;
};

/* @todo It's not suppose to have static global variables. */
// Tell us if the game is running.
global bool Global_Running = false;

// A wrapper for properties related to bitmap(a graphic object), which we use to
// render our game world.
global Win32_Back_Buffer Global_Back_Buffer;

// Sound buffer that we actually write into.
global Direct_Sound_Buffer Global_Secondary_Sound_Buffer;

// CPU counts per second
global int64 Global_Perf_Frequency;

internal void
Win32_Debug_Log(LPCSTR info)
{
  OutputDebugStringA("[Win32] ");
  OutputDebugStringA(info);
  OutputDebugStringA("\n");
}

#pragma region Controller Stuff
// XInputGetState
#define X_INPUT_GET_STATE(name)                                                \
  DWORD WINAPI name(DWORD userIdx, XINPUT_STATE* state)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
  return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_
// XInputSetState
#define X_INPUT_SET_STATE(name)                                                \
  DWORD WINAPI name(DWORD userIdx, XINPUT_VIBRATION* vibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
  return ERROR_DEVICE_NOT_CONNECTED;
}
// Load XInputGetState & XInputSetState
static x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_
static void
Win32_Load_XInput(void)
{
  HMODULE xInputLib = LoadLibrary("xinput1_4.dll");
  if (!xInputLib) {
    xInputLib = LoadLibrary("xinput1_3.dll");
  }

  if (!xInputLib) {
    /* @todo Failed to load x-input lib. */
  } else {
    XInputGetState =
      (x_input_get_state*)GetProcAddress(xInputLib, "XInputGetState");
    XInputSetState =
      (x_input_set_state*)GetProcAddress(xInputLib, "XInputSetState");
  }
}
#pragma endregion Controller Stuff

#pragma region Audio Stuff
// Dynamically load function: DirectSoundCreate
#define DIRECT_SOUND_CREATE(name)                                              \
  HRESULT WINAPI name(                                                         \
    LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);
DIRECT_SOUND_CREATE(DirectSoundCreateStub)
{
  return DSERR_INVALIDCALL;
}
static direct_sound_create* DirectSoundCreate_ = DirectSoundCreateStub;
#define DirectSoundCreate DirectSoundCreate_

inline void
Win32_Load_DSound()
{
  HMODULE dsound_lib = LoadLibrary("dsound.dll");
  assert(dsound_lib != NULL);

  DirectSoundCreate =
    (direct_sound_create*)GetProcAddress(dsound_lib, "DirectSoundCreate");
}

inline void
Win32_Init_WaveFormat(WAVEFORMATEX* wave_format, INT32 samples_per_second)
{
  wave_format->wFormatTag = WAVE_FORMAT_PCM;
  wave_format->nChannels = 2;
  wave_format->nSamplesPerSec = samples_per_second;
  wave_format->wBitsPerSample = 16;
  wave_format->nBlockAlign =
    wave_format->nChannels * wave_format->wBitsPerSample / 8;
  wave_format->nAvgBytesPerSec = samples_per_second * wave_format->nBlockAlign;
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
                              int buffer_size)
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
Win32_Init_DSound(HWND window, INT32 bufferSize, INT32 samplesPerSecond)
{
  Win32_Load_DSound();
  assert(DirectSoundCreate != NULL);

  LPDIRECTSOUND dsound;
  bool dsound_created = SUCCEEDED(DirectSoundCreate(0, &dsound, 0));
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

  bool wave_format_settled = SUCCEEDED(primary_buffer->SetFormat(&wave_format));
  assert(wave_format_settled);

  bool secondary_buffer_created =
    SUCCEEDED(Win32_Create_Secondary_Buffer(dsound, wave_format, bufferSize));
  assert(secondary_buffer_created);
}

internal void
Win32_Clear_Sound_Buffer(Win32_Sound_Output* sound_output)
{
  VOID* /*out*/ region1;
  DWORD /*out*/ region1_size;
  VOID* /*out*/ region2;
  DWORD /*out*/ region2_size;
  bool buffer_locked = SUCCEEDED(
    Global_Secondary_Sound_Buffer->Lock(0,
                                        sound_output->Secondary_Buffer_Size,
                                        &region1,
                                        &region1_size,
                                        &region2,
                                        &region2_size,
                                        0));

  assert(buffer_locked);

  uint8* write_cursor = (uint8*)region1;
  for (int i = 0; i < region1_size; i++) {
    *write_cursor++ = 0;
  }

  write_cursor = (uint8*)region2;
  for (int i = 0; i < region2_size; i++) {
    *write_cursor++ = 0;
  }

  Global_Secondary_Sound_Buffer->Unlock(
    region1, region1_size, region2, region2_size);

  Win32_Debug_Log("Secondary sound buffer is cleared.");
}

internal void
Win32_Try_Fill_Sound_Buffer(Win32_Sound_Output* sound_output,
                            DWORD offset,
                            DWORD bytes_to_write,
                            Game_Sound_Buffer* source_buffer)
{
  VOID* /*out*/ region1;
  DWORD /*out*/ region1_size;
  VOID* /*out*/ region2;
  DWORD /*out*/ region2_size;
  bool buffer_locked =
    SUCCEEDED(Global_Secondary_Sound_Buffer->Lock(offset,
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
  DWORD region1SampleCount = region1_size / sound_output->Bytes_Per_Sample;
  int16* dest_sample = (int16*)region1;
  int16* source_sample = source_buffer->samples;

  for (int i = 0; i < region1SampleCount; i++) {
    *dest_sample++ = *source_sample++; // Left
    *dest_sample++ = *source_sample++; // Right
    sound_output->running_Sample_idx += 1;
  }

  // Write region2
  DWORD region2SampleCount = region2_size / sound_output->Bytes_Per_Sample;
  dest_sample = (INT16*)region2;
  for (int i = 0; i < region2SampleCount; i++) {
    *dest_sample++ = *source_sample++; // Left
    *dest_sample++ = *source_sample++; // Right
    sound_output->running_Sample_idx += 1;
  }

  Global_Secondary_Sound_Buffer->Unlock(
    region1, region1_size, region2, region2_size);
}
#pragma endregion Audio Stuff

#pragma region Bitmap Stuff
Win32_Window_Dimension
Win32_Get_Window_Diemnsion(HWND window)
{
  RECT clientRect;
  GetClientRect(window, &clientRect);

  int width = clientRect.right - clientRect.left;
  int height = clientRect.bottom - clientRect.top;

  return { width, height };
}

internal void
Win32_Resize_DIBSection(Win32_Back_Buffer* buffer, int width, int height)
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
  bmiHeader.biBitCount = buffer->bytes_per_pixel * 8;
  bmiHeader.biCompression = BI_RGB;

  int memorySize = buffer->width * buffer->height * buffer->bytes_per_pixel;
  buffer->bitmap =
    VirtualAlloc(0, memorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  buffer->pitch = buffer->width * buffer->bytes_per_pixel;
}

internal void
Win32_Copy_Buffer_To_Window(HDC deviceContext,
                            Win32_Back_Buffer* buffer,
                            int window_width,
                            int window_height)
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
#pragma endregion Bitmap Stuff

internal void
Win32_Process_XInput_Digital_Button(Game_Button_State* s_old,
                                    Game_Button_State* s_new,
                                    DWORD button_state,
                                    DWORD btn_bit)
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
                           WPARAM wParam,
                           LPARAM lParam)
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
      RECT clientRect;
      HDC deviceContext = BeginPaint(window, &paint);

      // Display buffer to window
      Win32_Window_Dimension dimension = Win32_Get_Window_Diemnsion(window);
      Win32_Copy_Buffer_To_Window(
        deviceContext, &Global_Back_Buffer, dimension.width, dimension.height);

      EndPaint(window, &paint);
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      UINT32 vkcode = wParam;
      bool wasDown = lParam & (1 << 30) != 0;
      bool isDown = lParam & (1 << 31) == 0;

      if (vkcode == 'W') {
        OutputDebugStringA("W\n");
      } else if (vkcode == 'A') {
        OutputDebugStringA("A\n");
      } else if (vkcode == 'S') {
        OutputDebugStringA("S\n");
      } else if (vkcode == 'D') {
        OutputDebugStringA("D\n");
      } else if (vkcode == VK_UP) {
        OutputDebugStringA("VK_UP\n");
      } else if (vkcode == VK_DOWN) {
        OutputDebugStringA("VK_DOWN\n");
      } else if (vkcode == VK_LEFT) {
        OutputDebugStringA("VK_LEFT\n");
      } else if (vkcode == VK_RIGHT) {
        OutputDebugStringA("VK_RIGHT\n");
      } else if (vkcode == VK_ESCAPE) {
        OutputDebugStringA("VK_ESCAPE\n");
      } else if (vkcode == VK_SPACE) {
        OutputDebugStringA("VK_SPACE\n");
      }

      // @fixme There might be a waste of time trans int32 to bool
      bool altDown = lParam & (1 << 29);
      if (vkcode == VK_F4 && altDown) {
        Global_Running = false;
      }
    } break;

    default: {
      result = DefWindowProc(window, message, wParam, lParam);
    } break;
  }

  return result;
}

int CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE prevInstance,
        LPSTR lpCmdLine,
        int nShowCmd)
{
  // Dynamically load functions
  Win32_Load_XInput();

  // Resize windows back buffer
  Win32_Resize_DIBSection(&Global_Back_Buffer, 1280, 720);

  // Retrieves performance frequency.
  LARGE_INTEGER perfFrequencyResult;
  QueryPerformanceFrequency(&perfFrequencyResult);
  Global_Perf_Frequency = perfFrequencyResult.QuadPart;

  WNDCLASS windowClass = {};
  windowClass.style =
    CS_HREDRAW | CS_VREDRAW; // Redraw when adjust or move happend vertically or
                             // horizontally.
  windowClass.lpfnWndProc = Win32_Main_Window_Callback;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = "Handmade_Window_Class";

  /* @todo Setup icon for this app: windowClass.hIcon */
  bool registered = RegisterClass(&windowClass);
  assert(registered);

  HWND window = CreateWindowEx(0,
                               windowClass.lpszClassName,
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
  Win32_Init_DSound(window,
                    sound_output.Secondary_Buffer_Size,
                    sound_output.Samples_Per_Second);
  Win32_Clear_Sound_Buffer(&sound_output);
  Global_Secondary_Sound_Buffer->Play(0, 0, DSBPLAY_LOOPING);
  int16* sound_samples =
    (int16*)VirtualAlloc(0,
                         sound_output.Secondary_Buffer_Size,
                         MEM_COMMIT | MEM_RESERVE,
                         PAGE_READWRITE);

  // @hack Debug stuff
  LARGE_INTEGER lastCounter;
  QueryPerformanceCounter(&lastCounter);
  UINT64 lastCycleCount = __rdtsc();

  Game_Input inputs[2] = {};
  Game_Input* old_input = &inputs[0];
  Game_Input* new_input = &inputs[1];

  // Win32 main loop
  while (Global_Running) {
    MSG message = {};

    // @note Peek the newest message from queue without pending the application
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
      if (message.message == WM_QUIT)
        Global_Running = false;

      TranslateMessage(&message);
      DispatchMessage(&message);
    }

    { // Handle controller input

      int max_controller_count = XUSER_MAX_COUNT;
      if (max_controller_count > get_array_size(new_input->controllers)) {
        max_controller_count = get_array_size(new_input->controllers);
      }

      // @todo Should we ask for user input more frequently?
      for (int it_idx = 0; it_idx < max_controller_count; it_idx++) {
        Game_Controller_Input* old_controller = &old_input->controllers[it_idx];
        Game_Controller_Input* new_controller = &new_input->controllers[it_idx];

        XINPUT_STATE controller_state;
        DWORD result = XInputGetState(it_idx, &controller_state);
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
        Win32_Process_XInput_Digital_Button(&old_controller->left_shoulder,
                                            &new_controller->left_shoulder,
                                            gamepad->wButtons,
                                            XINPUT_GAMEPAD_LEFT_SHOULDER);
        Win32_Process_XInput_Digital_Button(&old_controller->right_shoulder,
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
        new_controller->max_x = new_controller->min_x = new_controller->end_x =
          x;
        real32 y;
        if (gamepad->sThumbLX < 0) {
          y = (real32)gamepad->sThumbLX / 32768.0f;
        } else {
          y = (real32)gamepad->sThumbLX / 32767.0f;
        }
        new_controller->max_y = new_controller->min_y = new_controller->end_y =
          y;
        new_controller->is_analog = true;

        // // Right thumbstick
        // uint16 thumb_right_x = gamepad->sThumbRX;
        // uint16 thumb_right_y = gamepad->sThumbRY;
      }

      // @todo Impl swap
      Game_Input* temp = new_input;
      new_input = old_input;
      old_input = temp;
    }

    DWORD offset;
    DWORD target_cursor;
    DWORD bytes_to_write =
      0; // max  availables that we can write to the buffer.

    { // Calcu bytes need to be write to sound buffer
      DWORD play_cursor;
      DWORD
      write_cursor; // @Ignored: We are going to write the buffer on our own

      bool buffer_cursor_getted =
        SUCCEEDED(Global_Secondary_Sound_Buffer->GetCurrentPosition(
          &play_cursor, &write_cursor));

      assert(buffer_cursor_getted);

      offset =
        (sound_output.running_Sample_idx * sound_output.Bytes_Per_Sample) %
        sound_output.Secondary_Buffer_Size;

      target_cursor = (play_cursor + (sound_output.Latency_Sample_Count *
                                      sound_output.Bytes_Per_Sample)) %
                      sound_output.Secondary_Buffer_Size;

      if (offset > target_cursor) {
        // | _ _ _ play~~~target *** offset _ _ _ |
        bytes_to_write = sound_output.Secondary_Buffer_Size - offset;
        bytes_to_write += target_cursor;
      } else {
        // | *** offset _ _ _ play~~~target *** |
        bytes_to_write = target_cursor - offset;
      }
    }

    Game_Back_Buffer back_buffer = {};
    back_buffer.bitmap = Global_Back_Buffer.bitmap;
    back_buffer.width = Global_Back_Buffer.width;
    back_buffer.height = Global_Back_Buffer.height;
    back_buffer.pitch = Global_Back_Buffer.pitch;

    Game_Sound_Buffer sound_buffer = {};
    sound_buffer.samples = sound_samples;
    sound_buffer.samples_per_second = sound_output.Samples_Per_Second;
    sound_buffer.sample_count = bytes_to_write / sound_output.Bytes_Per_Sample;

    Game_Update(&back_buffer, &sound_buffer, new_input);

    { // Render to screen
      HDC dc = GetDC(window);
      Win32_Window_Dimension dimension = Win32_Get_Window_Diemnsion(window);
      Win32_Copy_Buffer_To_Window(
        dc, &Global_Back_Buffer, dimension.width, dimension.height);
      ReleaseDC(window, dc);
    }

    { // Fill sound output
      Win32_Try_Fill_Sound_Buffer(
        &sound_output, offset, bytes_to_write, &sound_buffer);
    }

    { // Display debug data
      /* @hack query performance data. */
      LARGE_INTEGER endCounter;
      QueryPerformanceCounter(&endCounter);
      INT64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
      UINT64 endCycleCount = __rdtsc();
      UINT64 cyclesElapsed = endCycleCount - lastCycleCount;

      // Milliseconds per frame
      float mspf =
        (1000.0f * (float)counterElapsed) / (float)Global_Perf_Frequency;
      // Frames per second
      float fps = (float)Global_Perf_Frequency / (float)counterElapsed;
      // Mega-Cycles per frame
      float mcpf = (float)cyclesElapsed / (1000.0f * 1000.0f);

      // char perf_log[256];
      // sprintf(perf_log, "%.2fms/f, %.2ff/s, %.2fmc/f\n", mspf, fps, mcpf);
      // Win32_Debug_Log(perf_log);

      // Update performance data of current frame.
      lastCounter = endCounter;
      lastCycleCount = endCycleCount;
    }
  }

  return 0;
}