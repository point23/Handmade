#include <windows.h>
#include <xinput.h>

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

/* BEGIN REGION: Dynacmically loaded functions */
// XInputGetState
#define X_INPUT_GET_STATE(name)                                                \
  DWORD WINAPI name(DWORD userIdx, XINPUT_STATE* state)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
  return 0;
}
static x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_
// XInputSetState
#define X_INPUT_SET_STATE(name)                                                \
  DWORD WINAPI name(DWORD userIdx, XINPUT_VIBRATION* vibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
  return 0;
}
static x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

static void
Win32LoadXInput(void)
{
  HMODULE xInputLib = LoadLibraryA("xinput1_3.dll");
  if (xInputLib) {
    XInputGetState =
      (x_input_get_state*)GetProcAddress(xInputLib, "XInputGetState");
    XInputSetState =
      (x_input_set_state*)GetProcAddress(xInputLib, "XInputSetState");
  }
}
/* END REGION: Dynacmically loaded functions */

win32_window_dimension
Win32GetWindowDiemnsion(HWND window)
{
  RECT clientRect;
  GetClientRect(window, &clientRect);

  int width = clientRect.right - clientRect.left;
  int height = clientRect.bottom - clientRect.top;

  return { width, height };
}

static void
RenderGradient(win32_offscreen_buffer buffer, int xOffset, int yOffset)
{ // FIXME: Function only for test drawing
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

  // TODO: Clearing
}

static void
Win32CopyBufferToWindow(HDC deviceContext,
                        win32_offscreen_buffer* buffer,
                        int windowWidth,
                        int windowHeight)
{
  // TODO: Acspect ratio correction
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

    /* BEGIN REGION: User KeyBoard Input */
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
    } break;
    /* END REGION: User KeyBoard Input */
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
      // Init backbuffer
      Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

      // FIXME: Test variable
      int xOffset = 0, yOffset = 0;

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

            // FIXME: Test Offset
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

        // FIXME: Test drawing
        HDC dc = GetDC(window);
        win32_window_dimension dimension = Win32GetWindowDiemnsion(window);
        RenderGradient(GlobalBackBuffer, xOffset, yOffset);
        Win32CopyBufferToWindow(
          dc, &GlobalBackBuffer, dimension.width, dimension.height);
        ReleaseDC(window, dc);
      }

    } else {
      // TODO: Logging
    }
  } else {
    // TODO: Logging
  }

  return 0;
}