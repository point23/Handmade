#include <windows.h>

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
                        win32_offscreen_buffer buffer,
                        int windowWidth,
                        int windowHeight,
                        int x,
                        int y,
                        int paintWidth,
                        int paintHeight)
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
                buffer.width,
                buffer.height,
                // Source
                buffer.bitmap,
                // Destination
                &buffer.bmi,
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
      GetClientRect(window, &clientRect);

      HDC deviceContext = BeginPaint(window, &paint);
      { // Painting Process
        int x = paint.rcPaint.left, y = paint.rcPaint.top;
        int width = paint.rcPaint.right - paint.rcPaint.left;
        int height = paint.rcPaint.bottom - paint.rcPaint.top;

        win32_window_dimension dimension = Win32GetWindowDiemnsion(window);

        Win32CopyBufferToWindow(deviceContext,
                                GlobalBackBuffer,
                                dimension.width,
                                dimension.height,
                                x,
                                y,
                                width,
                                height);
      }
      EndPaint(window, &paint);
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

          { // FIXME: Test drawing
            HDC dc = GetDC(window);
            win32_window_dimension dimension = Win32GetWindowDiemnsion(window);
            RenderGradient(GlobalBackBuffer, xOffset, yOffset);
            Win32CopyBufferToWindow(dc,
                                    GlobalBackBuffer,
                                    dimension.width,
                                    dimension.height,
                                    0,
                                    0,
                                    dimension.width,
                                    dimension.height);
            xOffset += 1;
            ReleaseDC(window, dc);
          }
        }
      }
    } else {
      // TODO: Logging
    }
  } else {
    // TODO: Logging
  }

  return 0;
}