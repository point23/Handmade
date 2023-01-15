#include <dSound.h.>
#include <windows.h>
#include <xinput.h>

// FIXME: Only in dev mode, we should implement it ourselves in the future,
// we may need to get rid of Runtime Libs
#include <math.h>

struct win32_offscreen_buffer
{
  BITMAPINFO bmi;
  void* bitmap;
  int width;
  int height;
  int bytesPerPixel;
  int pitch;
};

struct win32_window_dimension
{
  int width;
  int height;
};

// TODO: It might not suppose to be a static global.
static bool GlobalRunning;
static win32_offscreen_buffer GlobalBackBuffer;
static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

#define PI32 3.14159265359f

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
Win32LoadXInput(void)
{
  HMODULE xInputLib = LoadLibrary("xinput1_4.dll");
  if (!xInputLib) {
    xInputLib = LoadLibrary("xinput1_3.dll");
  }

  if (xInputLib) {
    XInputGetState =
      (x_input_get_state*)GetProcAddress(xInputLib, "XInputGetState");
    XInputSetState =
      (x_input_set_state*)GetProcAddress(xInputLib, "XInputSetState");
  } else {
    // TODO: Logging
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
  if (dSoundLib) {
    DirectSoundCreate =
      (direct_sound_create*)GetProcAddress(dSoundLib, "DirectSoundCreate");
  } else {
    // TODO: Logging
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
    &bufferDescription, &GlobalSecondaryBuffer, 0);
}

static void
Win32InitDSound(HWND window, INT32 bufferSize, INT32 samplesPerSecond)
{
  Win32LoadDSound();

  LPDIRECTSOUND dSound;
  if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &dSound, 0))) {
    WAVEFORMATEX waveFormat = {};
    Win32InitWaveFormatForPrimaryBuffer(&waveFormat, samplesPerSecond);

    HRESULT cooperativelevelIsSet =
      dSound->SetCooperativeLevel(window, DSSCL_PRIORITY);

    if (SUCCEEDED(cooperativelevelIsSet)) {
      LPDIRECTSOUNDBUFFER primaryBuffer;
      HRESULT primaryBufferCreated =
        Win32CreatePrimaryBuffer(dSound, &primaryBuffer);

      if (SUCCEEDED(primaryBufferCreated)) {
        HRESULT waveFormatIsSet = primaryBuffer->SetFormat(&waveFormat);
        if (!SUCCEEDED(waveFormatIsSet)) {
          // TODO: Loging, failed to set format of primary sound buffer.
        }
      }
    } else {
      // TODO: Logging, filed to create primary sound buffer.
    }

    HRESULT secondaryBufferCreated =
      Win32CreateSecondaryBuffer(dSound, waveFormat, bufferSize);
    if (!SUCCEEDED(secondaryBufferCreated)) {
      // TODO: Logging, filed to create secondary sound buffer.
    }
  } else {
    // TODO: Logging, filed to load sound library.
  }
}

struct win32_sound_output
{
  const int SamplesPerSecond = 48000;
  const int BytesPerSample = sizeof(INT16) * 2;
  // Bytes in sound buffer be like:
  // int16 int16  int16 int16
  // [Left Right] [Left Right]
  const INT32 SecondaryBufferSize = SamplesPerSecond * BytesPerSample;
  int toneHz = 256;
  UINT32 runningSampleIdx = 0;
  // Samples-per-circle = samples-per-second / frequency
  int wavePeriod = SamplesPerSecond / toneHz;
  int halfWavePeriod = wavePeriod / 2;
  int maxAbsVolume = 4000;
};

// FIXME: Test audio stuff, Fill the secondary sound buffer with sine wave.
static void
Win32FillSoundBuffer(win32_sound_output* soundOutput,
                     DWORD offset,
                     DWORD bytesTOWrite)
{
  VOID* /*out*/ region1;
  DWORD /*out*/ region1Size;
  VOID* /*out*/ region2;
  DWORD /*out*/ region2Size;
  // Lock the buffer
  HRESULT bufferIsLock = GlobalSecondaryBuffer->Lock(
    offset, bytesTOWrite, &region1, &region1Size, &region2, &region2Size, 0);

  if (SUCCEEDED(bufferIsLock)) {
    // Write Region1
    DWORD region1SampleCount = region1Size / soundOutput->BytesPerSample;
    INT16* writePointer = (INT16*)region1;
    for (DWORD i = 0; i < region1SampleCount; i++) {
      float t = 2.0f * PI32 * (float)soundOutput->runningSampleIdx /
                (float)soundOutput->wavePeriod;
      float sineVal = sinf(t);
      INT16 sampleVal = (INT16)(soundOutput->maxAbsVolume * sineVal);
      *writePointer++ = sampleVal; // Left
      *writePointer++ = sampleVal; // Right
      soundOutput->runningSampleIdx += 1;
    }

    // Write Region2
    DWORD region2SampleCount = region2Size / soundOutput->BytesPerSample;
    writePointer = (INT16*)region2;
    for (DWORD i = 0; i < region2SampleCount; i++) {
      float t = 2.0f * PI32 * (float)soundOutput->runningSampleIdx /
                (float)soundOutput->wavePeriod;
      float sineVal = sinf(t);
      INT16 sampleVal = (INT16)(soundOutput->maxAbsVolume * sineVal);
      *writePointer++ = sampleVal; // Left
      *writePointer++ = sampleVal; // Right
      soundOutput->runningSampleIdx += 1;
    }

    // Unlock the buffer
    GlobalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
  }
}
#pragma endregion Audio Stuff

#pragma region Bitmap Stuff
win32_window_dimension
Win32GetWindowDiemnsion(HWND window)
{
  RECT clientRect;
  GetClientRect(window, &clientRect);

  int width = clientRect.right - clientRect.left;
  int height = clientRect.bottom - clientRect.top;

  return { width, height };
}

// FIXME: Function only for test drawing
static void
RenderGradient(win32_offscreen_buffer buffer, int xOffset, int yOffset)
{
  UINT8* row = (UINT8*)buffer.bitmap;
  for (int y = 0; y < buffer.height; y++) {
    UINT32* pixel = (UINT32*)row;

    for (int x = 0; x < buffer.width; x++) {
      UINT8 blueCannel = (UINT8)(x + xOffset);
      UINT8 greenCannel = (UINT8)(y + yOffset);

      *pixel = (greenCannel << 8) | blueCannel;
      pixel += 1;
    }
    row += buffer.pitch;
  }
}

static void
Win32ResizeDIBSection(win32_offscreen_buffer* buffer, int width, int height)
{
  // TODO: Add some VirtualProtect stuff in the future.

  // Clear the old buffer.
  if (buffer->bitmap) {
    VirtualFree(buffer->bitmap, 0, MEM_RELEASE);
  }

  buffer->height = height, buffer->width = width;
  buffer->bytesPerPixel = 4; // TODO: Move it elsewhere

  { /* Init struct BITMAPINFO.
       Mainly the bmiHeader part, cause we don't use the palette */
    BITMAPINFOHEADER& bmiHeader = buffer->bmi.bmiHeader;
    bmiHeader.biSize = sizeof(bmiHeader);
    bmiHeader.biWidth = buffer->width;
    bmiHeader.biHeight =
      -buffer->height;      // Top-Down DIB with origin at upper-left
    bmiHeader.biPlanes = 1; // This value must be set to 1.
    bmiHeader.biBitCount = 32;
    bmiHeader.biCompression = BI_RGB;
  }

  int memorySize = (buffer->width * buffer->height) * buffer->bytesPerPixel;
  buffer->bitmap = VirtualAlloc(0, memorySize, MEM_COMMIT, PAGE_READWRITE);

  buffer->pitch = buffer->width * buffer->bytesPerPixel;
}

static void
Win32CopyBufferToWindow(HDC deviceContext,
                        win32_offscreen_buffer* buffer,
                        int windowWidth,
                        int windowHeight)
{
  // TODO: Aspect ratio correction
  StretchDIBits(deviceContext,
                // Destination rectangle
                0,
                0,
                windowWidth,
                windowHeight,
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

LRESULT CALLBACK
Win32MainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  LRESULT result = 0;
  switch (message) {
    case WM_ACTIVATEAPP: {
      OutputDebugStringA("WM_ACTIVATEAPP\n");
    } break;

    case WM_SIZE: {
      // We take the initiative to update the backbuffer.
      OutputDebugStringA("WM_SIZE\n");
    } break;

    case WM_DESTROY: {
      // TODO: Handle this as an error.
      GlobalRunning = false;
      OutputDebugStringA("WM_DESTROY\n");
    } break;

    case WM_CLOSE: {
      // TODO: Handle this with a message to user
      GlobalRunning = false;
      OutputDebugStringA("WM_CLOSE\n");
    } break;

    case WM_PAINT: {
      PAINTSTRUCT paint;
      RECT clientRect;
      HDC deviceContext = BeginPaint(window, &paint);
      // GetClientRect(window, &clientRect);
      // int x = paint.rcPaint.left, y = paint.rcPaint.top;
      // int width = paint.rcPaint.right - paint.rcPaint.left;
      // int height = paint.rcPaint.bottom - paint.rcPaint.top;
      // EndPaint(window, &paint);

      // Display buffer to window

      win32_window_dimension dimension = Win32GetWindowDiemnsion(window);
      Win32CopyBufferToWindow(
        deviceContext, &GlobalBackBuffer, dimension.width, dimension.height);

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

      // FIXME: There might be a waste of time trans int32 to bool
      bool altDown = lParam & (1 << 29);
      if (vkcode == VK_F4 && altDown) {
        GlobalRunning = false;
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
  // Dynamically load several functions
  Win32LoadXInput();
  // Init backbuffer
  Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

  WNDCLASS windowClass = {};
  // Redraw when adjust or move happend vertically or horizontally.
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = Win32MainWindowCallback;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = "HandmadeWIndowClass";
  // windowClass.hIcon

  if (RegisterClass(&windowClass)) {
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

    if (window) {
      GlobalRunning = true;
      // FIXME: Test variable
      // Graphic test
      int xOffset = 0, yOffset = 0;
      // Audio test
      win32_sound_output soundOutput;

      Win32InitDSound(
        window, soundOutput.SecondaryBufferSize, soundOutput.SamplesPerSecond);
      Win32FillSoundBuffer(&soundOutput, 0, soundOutput.SecondaryBufferSize);
      GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      while (GlobalRunning) {
        MSG message;

        // Peek the newest message from queue without pending the application
        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
          if (message.message == WM_QUIT) {
            GlobalRunning = false;
          }

          TranslateMessage(&message);
          DispatchMessage(&message);
        }

#pragma region Controller Input Stuff
        // TODO: Should we ask for user input more frequently?
        for (DWORD controllerIdx = 0; controllerIdx < XUSER_MAX_COUNT;
             controllerIdx++) {
          DWORD result;
          XINPUT_STATE controllerState;

          result = XInputGetState(controllerIdx, &controllerState);
          if (result == ERROR_SUCCESS) {
            // Controller is connected

            XINPUT_GAMEPAD* gamepad = &controllerState.Gamepad;
            // D-pad
            bool dPadUp = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            bool dPadDown = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            bool dPadLeft = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            bool dPadRight = gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

            // Button
            bool btnStart = gamepad->wButtons & XINPUT_GAMEPAD_START;
            bool btnback = gamepad->wButtons & XINPUT_GAMEPAD_BACK;
            bool btnA = gamepad->wButtons & XINPUT_GAMEPAD_A;
            bool btnB = gamepad->wButtons & XINPUT_GAMEPAD_B;
            bool btnX = gamepad->wButtons & XINPUT_GAMEPAD_X;
            bool btnY = gamepad->wButtons & XINPUT_GAMEPAD_Y;

            // Shoulder
            bool leftShoulder =
              gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
            bool rightShoulder =
              gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;

            // Left thumbstick
            UINT16 thumbLeftX = gamepad->sThumbLX;
            UINT16 thumbLeftY = gamepad->sThumbLY;

            // Right thumbstick
            UINT16 thumbRightX = gamepad->sThumbRX;
            UINT16 thumbRightY = gamepad->sThumbRY;

            // FIXME: Test handle user input
            if (dPadUp)
              yOffset += 1;
            if (dPadDown)
              yOffset -= 1;

            if (dPadLeft)
              xOffset += 1;
            if (dPadRight)
              xOffset -= 1;

            if (btnX) {
              XINPUT_VIBRATION vibration;
              vibration.wLeftMotorSpeed = 60000;
              vibration.wRightMotorSpeed = 60000;
              XInputSetState(controllerIdx, &vibration);
            }
          } else {
            // Controller is not connected
          }
        }
#pragma endregion Controller Input Stuff

        // FIXME: Test drawing
#pragma region Test : Drawing
        HDC dc = GetDC(window);
        win32_window_dimension dimension = Win32GetWindowDiemnsion(window);
        RenderGradient(GlobalBackBuffer, xOffset, yOffset);
        Win32CopyBufferToWindow(
          dc, &GlobalBackBuffer, dimension.width, dimension.height);
        ReleaseDC(window, dc);
#pragma endregion Drawing

        // FIXME: Tast sound playing
#pragma region Test : Sound playing
        DWORD playCursor;
        DWORD writeCursor; // Ignored this since we are going to write the
                           // buffer on our own.s
        HRESULT bufferCurrentPosIsGet =
          GlobalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor);
        if (SUCCEEDED(bufferCurrentPosIsGet)) {
          // TODO: Use a lower latency offset.
          // Offet in the buffer, end of written part.
          DWORD offset =
            (soundOutput.runningSampleIdx * soundOutput.BytesPerSample) %
            soundOutput.SecondaryBufferSize;
          DWORD bytesTOWrite = 0; // Maximized
          if (offset == playCursor) {
            bytesTOWrite = 0;
          } else if (offset > playCursor) {
            // | *** playCursor ->_ _ _ offset *** |
            bytesTOWrite =
              (soundOutput.SecondaryBufferSize - offset) + (playCursor - 0);
          } else {
            // | _ _ _ offset *** playCursor ->_ _ _|
            bytesTOWrite = playCursor - offset;
          }

          Win32FillSoundBuffer(&soundOutput, offset, bytesTOWrite);
        }
      }
#pragma endregion Sound playing

    } else {
      // TODO: Logging
    }
  } else {
    // TODO: Logging
  }

  return 0;
}