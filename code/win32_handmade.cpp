#include <dSound.h.>
#include <windows.h>
#include <xinput.h>

#include <assert.h>

/* FIXME: Only used in dev mode, we should implement it ourselves in the       \
 * future, we may need to get rid of RUNTIME libararies. */                    \
#include <math.h>
#include <stdio.h>

#include "handmade.cpp"

/* TODO WIN PLAYFORM LAYER SHOULD DO:
  - A backbuffer to render.
  - A soundbuffer to player.
  - Time.
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
   * NOTE Bytes in sound buffer be like:
   * [ B B   B B    B B   B B  ]
   * [[Left Right] [Left Right]]
   */
  const int Secondary_Buffer_Size = Samples_Per_Second * Bytes_Per_Sample;
  const int Latency_Sample_Count = Samples_Per_Second / 20;

  uint running_Sample_idx = 0;
};

/* TODO It's not suppose to have static global variables. */
// Tell us if the game is running.
global bool Global_Running = false;
// A wrapper for properties related to bitmap(a graphic object), which we use to
// render our game world.
global Win32_Back_Buffer Global_Back_Buffer;
// Sound buffer that we actually write into.
global Direct_Sound_Buffer Global_Secondary_Sound_Buffer;
// CPU counts per second
global int64 Global_Perf_Frequency;
// Float version of PI

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
    /* TODO Failed to load x-input lib. */
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
Win32LoadDSound()
{
  HMODULE dSoundLib = LoadLibrary("dsound.dll");
  if (!dSoundLib) {
    /* TODO Failed to load direct-sound lib. */
  } else {
    DirectSoundCreate =
      (direct_sound_create*)GetProcAddress(dSoundLib, "DirectSoundCreate");
  }
}

inline void
Win32InitWaveFormatForPrimaryBuffer(WAVEFORMATEX* waveFormat,
                                    INT32 samplesPerSecond)
{
  waveFormat->wFormatTag = WAVE_FORMAT_PCM;
  waveFormat->nChannels = 2;
  waveFormat->nSamplesPerSec = samplesPerSecond;
  waveFormat->wBitsPerSample = 16;
  waveFormat->nBlockAlign =
    waveFormat->nChannels * waveFormat->wBitsPerSample / 8;
  waveFormat->nAvgBytesPerSec = samplesPerSecond * waveFormat->nBlockAlign;
  waveFormat->cbSize = 0;
}

inline HRESULT
Win32CreatePrimaryBuffer(LPDIRECTSOUND dSound,
                         LPDIRECTSOUNDBUFFER* primaryBuffer)
{
  DSBUFFERDESC bufferDescription = {};
  bufferDescription.dwSize = sizeof(bufferDescription);
  bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
  return dSound->CreateSoundBuffer(&bufferDescription, primaryBuffer, 0);
}

inline HRESULT
Win32CreateSecondaryBuffer(LPDIRECTSOUND dSound,
                           WAVEFORMATEX waveFormat,
                           INT32 bufferSize)
{
  DSBUFFERDESC bufferDescription = {};
  bufferDescription.dwSize = sizeof(bufferDescription);
  bufferDescription.dwFlags = 0;
  bufferDescription.dwBufferBytes = bufferSize;
  bufferDescription.lpwfxFormat = &waveFormat;

  return dSound->CreateSoundBuffer(
    &bufferDescription, &Global_Secondary_Sound_Buffer, 0);
}

static void
Win32_Init_DSound(HWND window, INT32 bufferSize, INT32 samplesPerSecond)
{
  Win32LoadDSound();
  if (!DirectSoundCreate) {
    /* TODO Failed to load func:DirectSoundCreate */
  }
  LPDIRECTSOUND dSound;
  if (!SUCCEEDED(DirectSoundCreate(0, &dSound, 0))) {
    /* TODO Failed to create object:DIRECTSOUND */

  } else {
    WAVEFORMATEX waveFormat = {};
    Win32InitWaveFormatForPrimaryBuffer(&waveFormat, samplesPerSecond);

    HRESULT cooperativelevelIsSet =
      dSound->SetCooperativeLevel(window, DSSCL_PRIORITY);

    if (SUCCEEDED(cooperativelevelIsSet)) {
      /* TODO Failed to set cooperative level of dSound */

    } else {
      LPDIRECTSOUNDBUFFER primaryBuffer;
      HRESULT primaryBufferCreated =
        Win32CreatePrimaryBuffer(dSound, &primaryBuffer);

      if (!SUCCEEDED(primaryBufferCreated)) {
        /* TODO Failed to create primary buffer. */

      } else {
        HRESULT waveFormatIsSet = primaryBuffer->SetFormat(&waveFormat);
        if (!SUCCEEDED(waveFormatIsSet)) {
          /* TODO Failed to set format of primary sound buffer. */
          return;
        }
      }
    }

    HRESULT secondaryBufferCreated =
      Win32CreateSecondaryBuffer(dSound, waveFormat, bufferSize);
    if (!SUCCEEDED(secondaryBufferCreated)) {
      /* TODO Filed to create secondary sound buffer. */
    }
  }
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

static void
Win32_Resize_DIBSection(Win32_Back_Buffer* buffer, int width, int height)
{
  // TODO Add some Virtual Protect stuff in the future, we may want older
  // buffers for some reason.

  // Clear the old buffer.
  if (buffer->bitmap)
    VirtualFree(buffer->bitmap, 0, MEM_RELEASE);

  buffer->width = width;
  buffer->height = height;
  buffer->bytes_per_pixel = 4;

  // NOTE Init bitmapInfo, mainly bmiHeader
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

static void
Win32_Copy_Buffer_To_Window(HDC deviceContext,
                            Win32_Back_Buffer* buffer,
                            int window_width,
                            int window_height)
{
  // TODO: Aspect ratio correction
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

// NOTE A callback function, which you define in your application, that
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

#pragma region Keyboard Input Stuff
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

      // FIXME There might be a waste of time trans int32 to bool
      bool altDown = lParam & (1 << 29);
      if (vkcode == VK_F4 && altDown) {
        Global_Running = false;
      }
    } break;
#pragma endregion KeyBoard Input Stuff

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

  /* TODO Setup icon for this app: windowClass.hIcon */
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

  // FIXME Debug stuff
  LARGE_INTEGER lastCounter;
  QueryPerformanceCounter(&lastCounter);
  UINT64 lastCycleCount = __rdtsc();

  // Win32 main loop
  while (Global_Running) {
    MSG message = {};

    // NOTE Peek the newest message from queue without pending the application
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
      if (message.message == WM_QUIT)
        Global_Running = false;

      TranslateMessage(&message);
      DispatchMessage(&message);
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

    Game_Update(&back_buffer, &sound_buffer);

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

    // { // Handle Controller Input
    //   /* TODO Should we ask for user input more frequently? */
    //   for (DWORD controllerIdx = 0; controllerIdx < XUSER_MAX_COUNT;
    //        controllerIdx++) {
    //     DWORD result;
    //     XINPUT_STATE controllerState;

    //     result = XInputGetState(controllerIdx, &controllerState);
    //     if (result != ERROR_SUCCESS) {
    //       /* TODO Controller is not connected. */
    //     } else {
    //       // Controller is connected
    //       XINPUT_GAMEPAD* gamepad = &controllerState.Gamepad;
    //       // D-pad
    //       bool dPadUp = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
    //       bool dPadDown = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
    //       bool dPadLeft = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
    //       bool dPadRight = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

    //       // Button
    //       bool btnStart = gamepad->wButtons & XINPUT_GAMEPAD_START;
    //       bool btnback = gamepad->wButtons & XINPUT_GAMEPAD_BACK;
    //       bool btnA = gamepad->wButtons & XINPUT_GAMEPAD_A;
    //       bool btnB = gamepad->wButtons & XINPUT_GAMEPAD_B;
    //       bool btnX = gamepad->wButtons & XINPUT_GAMEPAD_X;
    //       bool btnY = gamepad->wButtons & XINPUT_GAMEPAD_Y;

    //       // Shoulder
    //       bool leftShoulder = gamepad->wButtons &
    //       XINPUT_GAMEPAD_LEFT_SHOULDER; bool rightShoulder =
    //         gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

    //       // Left thumbstick
    //       UINT16 thumbLeftX = gamepad->sThumbLX;
    //       UINT16 thumbLeftY = gamepad->sThumbLY;

    //       // Right thumbstick
    //       UINT16 thumbRightX = gamepad->sThumbRX;
    //       UINT16 thumbRightY = gamepad->sThumbRY;

    //       if (btnX) {
    //         XINPUT_VIBRATION vibration;
    //         vibration.wLeftMotorSpeed = 60000;
    //         vibration.wRightMotorSpeed = 60000;
    //         XInputSetState(controllerIdx, &vibration);
    //       }
    //     }
    //   }
    // }

    { // Display debug data
      /* NOTE Debug stuff: query performance data. */
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