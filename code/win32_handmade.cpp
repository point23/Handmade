#include <windows.h>

// TODO: It might not suppose to be a static global.
static bool running;
static BITMAPINFO bitmapInfo;
static void* bitmapMemory;
static int bitmapHeight, bitmapWidth;
static int bytesPerPixel = 4;

static void
RenderGradient(int xOffset, int yOffset)
{ // FIXME: Function only for test drawing
  int pitch = bitmapWidth * bytesPerPixel;
  UINT8* row = (UINT8*)bitmapMemory;
  for (int y = 0; y < bitmapHeight; y++) {
    UINT32* pixel = (UINT32*)row;

    for (int x = 0; x < bitmapWidth; x++) {
      UINT8 blueCannel = (UINT8)(x + xOffset);
      UINT8 greenCannel = (UINT8)(y + yOffset);

      *pixel = (greenCannel << 8) | blueCannel;
      pixel += 1;
    }
    row += pitch;
  }
}

static void
Win32ResizeDIBSection(int width, int height)
{
  // TODO: Add some VirtualProtect stuff in the future.
  if (bitmapMemory) {
    VirtualFree(bitmapMemory, 0, MEM_RELEASE);
  }

  bitmapHeight = height, bitmapWidth = width;

  { // Init struct BITMAPINFO
    BITMAPINFOHEADER& bmiHeader = bitmapInfo.bmiHeader;
    bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bmiHeader.biWidth = bitmapWidth;
    bmiHeader.biHeight = -bitmapHeight;
    bmiHeader.biPlanes = 1; // This value must be set to 1.
    bmiHeader.biBitCount = 32;
    bmiHeader.biCompression = BI_RGB;
  }

  int memorySize = (bitmapWidth * bitmapHeight) * bytesPerPixel;
  bitmapMemory = VirtualAlloc(0, memorySize, MEM_COMMIT, PAGE_READWRITE);
  // TODO: Clearing
}

static void
Win32UpdateWindow(HDC deviceContext,
                  RECT* clientRect,
                  int x,
                  int y,
                  int width,
                  int height)
{
  int windowWidth = clientRect->right - clientRect->left;
  int windowHeight = clientRect->bottom - clientRect->top;
  StretchDIBits(deviceContext,
                // Destination rectangle
                0,
                0,
                bitmapWidth,
                bitmapHeight,
                // Source rectangle
                0,
                0,
                windowWidth,
                windowHeight,
                bitmapMemory, // Source
                &bitmapInfo,  // Destination
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
      RECT clientRect;
      GetClientRect(window, &clientRect);

      int width = clientRect.right - clientRect.left;
      int height = clientRect.bottom - clientRect.top;
      Win32ResizeDIBSection(width, height);

      OutputDebugStringA("WM_SIZE\n");
    } break;

    case WM_DESTROY: {
      // TODO: Handle this as an error.
      running = false;
      OutputDebugStringA("WM_DESTROY\n");
    } break;

    case WM_CLOSE: {
      // TODO: Handle this with a message to user
      running = false;
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
        Win32UpdateWindow(deviceContext, &clientRect, x, y, width, height);
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
  // TODO: Check if these flags still matter
  windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
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
      running = true;
      // FIXME: Test variable
      int xOffset = 0, yOffset = 0;

      MSG message;
      while (running) {
        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
          if (message.message == WM_QUIT) {
            running = false;
          }

          TranslateMessage(&message);
          DispatchMessage(&message);

          { // FIXME: Test drawing
            RECT clientRect;
            GetClientRect(window, &clientRect);
            HDC dc = GetDC(window);
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            RenderGradient(xOffset, yOffset);
            Win32UpdateWindow(dc, &clientRect, 0, 0, width, height);
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