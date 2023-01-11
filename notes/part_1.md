## Chap#001 Setting up the Windows build

#### Section#1 The entry point -- `WinMain`

```c++
int CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE prevInstance, 
        LPSTR     lpCmdLine,
        int       nShowCmd) {
    return 0;
}
```

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-winmain?source=recommendations
- what is CALLBACK?:

  - https://stackoverflow.com/questions/13871617/winmain-and-main-in-c-extended

#### Section#2 Build tool: batch file

- Wiki: https://en.wikipedia.org/wiki/Batch_file

- `cl` is not recognized

  - https://stackoverflow.com/questions/8800361/cl-is-not-recognized-as-an-internal-or-external-command

- build.bat

  ```
  @echo off
  
  pushd build
  cl -Zi ..\code\win32_handmade.cpp [libs]
  popd
  ```

#### Section#3 Debugger: Visual Studio

- Command: `devenv build\win32_handmade.exe`
- A dummy solution would be created for this project.
- Change the working dir:  `[YOUR WORKING DIR]\handmade\data`

#### [MessageBox: A Dummy Hello World Test]

- syntax:

  - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-messagebox

- requirements

  - | Library    | DLL        |
    | ---------- | ---------- |
    | User32.lib | User32.dll |

- use case:

  - ```c++
    MessageBox(
        0,
        Welcome!",
        "Handmade",
        MB_OK|MB_ICONINFORMATION
    );
    ```

## Chap#002 Opening a Win32 Window

#### Section#1 WNDCLASS structure

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-wndclassa
  
- Use case:

  - ```c++
    WNDCLASS WindowClass = {};
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = MainWindowCallback;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "HandmadeWIndowClass";
    // WindowClass.hIcon
    ```
  
- Windows class style

  - https://learn.microsoft.com/en-us/windows/win32/winmsg/window-class-styles
  
    | CS_HREDRAW                                        | CS_VREDRAW                                      |
    | ------------------------------------------------- | ----------------------------------------------- |
    | Repaint when width changed or horizontally moved. | Repaint when height change or vertically moved. |
  
- *Windows proc callback

  - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc

    ```c++
    LRESULT CALLBACK
    Win32MainWindowCallback(HWND 	window,
                       		UINT 	message,
                       		WPARAM 	wParam,
                       		LPARAM 	lParam)
    {
        LRESULT result = 0;
        switch(message)
        {
            case WM_SIZE:
            {
                OutputDebugStringA("SIZE\n");
            } break;
            
            case WM_DESTROY: {
                OutputDebugStringA("DESTROY\n");
            } break;
    
            case WM_CLOSE: {
                OutputDebugStringA("CLOSE\n");
            } break;
    
            case WM_ACTIVATEAPP: {
             	OutputDebugStringA("ACTIVATE\n");
            } break;
    
            default: {
                result = DefWindowProc(window, message, wParam, lParam);
            } break;
        }
    
        return result;
    }
    ```
  
  - Message categories
  
    - https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues#system-defined-messages
    
  - Mostly used: WM - General window messages

#### Section#2 Register Window Class

- Syntax:

  - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassa

  - Return Value: ATOM

    Class atom that uniquely identifies the class. If the function fails, the return value is zero.

- Use case:

  - ```c++
    if (RegisterClass(&WindowClass)) {
    	// Rendering
    }
    else {
    	// Loging
    }
    ```

#### Section#3 Create Window

- Ex for Extended.

- Syntax: 

  - https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa

- Use case:

  - ```c++
    HWND windowHandle = 
        CreateWindowEx(
            0,
            windowClass.lpszClassName,
            "Handmade",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            instance,
            0
    	);
    ```

#### Section#4 *Handle Messages

Solution#1 Naïve Approach:

```c++
for (;;) {
    MSG message;
    
    BOOL messageResult = GetMessage(&message, 0, 0, 0);
    if (messageResult > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    else {
        break;
    }
}
```

Solution#2 Without blocking

```c++
while (running) {
    MSG message;
    
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            running = false;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}
```

`GetMessageA()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmessage
- Retrieves a message from the calling thread's message queue.
- *Remarks: during this call, the system delivers pending.

`PeekMessageA()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-peekmessagea

- Remarks:

  `wRemoveMsg`: Specifies how messages are to be handled.

`TranslateMessage()`:

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-translatemessage?source=recommendations
- Translates virtual-key messages into character messages.

`DispatchMessage()`:

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dispatchmessage
- Dispatches a message to a window procedure.

#### Section#5 Simple Painting

Use case: inside the `Win32MainWindowCallback`

```c++
case WM_PAINT: {
    PAINTSTRUCT paint;
    HDC deviceContext = BeginPaint(window, &paint);
    int x = paint.rcPaint.left, y = paint.rcPaint.top;
    int width = paint.rcPaint.right - paint.rcPaint.left;
    int height = paint.rcPaint.bottom - paint.rcPaint.top;
    static DWORD backdrop = WHITENESS;
    PatBlt(deviceContext, x, y, width, height, backdrop);
    if (backdrop == WHITENESS) {
        backdrop = BLACKNESS;
    }
    else {
        backdrop = WHITENESS;
    }
    EndPaint(window, &paint);
} break;
```

PAINTSTRUCT

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-paintstruct
- Used to paint the client area of a window owned by that application.

`BeginPaint()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-beginpaint
- Prepares the specified window for painting and fills a PAINTSTRUCT structure with information about the painting.

`EndPaint()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-endpaint
- Marks the end of painting in the specified window.

`PatBlt()`:

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-patblt
- Paints the specified rectangle using the brush that is currently selected into the specified device context.

#### Section#6 Quit

> Let Windows clean up everything for us.

Solution#1 `PostQuitMessage(nExitCode)`

- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postquitmessage

- exit codes is no concern

*Solution#2 with a global variable - `static bool running;`

>  use these to separate 3 kinds of "static":
>
>  #define local_persist       static
>  #define global_variable  static
>  #define internal       	   static

- inside the `MainWindowCallback` function

  ```c++
  case WM_DESTROY: {
      running = false;
      OutputDebugStringA("WM_DESTROY\n");
  } break;
  
  case WM_CLOSE: {
      // TODO: Handle this with a message to user
      running = false;
      OutputDebugStringA("WM_CLOSE\n");
  } break;
  ```

- inside the entry point, the `WinMain`

  - from: `for(;;)`
  - to: `while(running)`

## Chap#003 Backbuffer: Allocating and Animating

#### Section#1 the Global backbuffer

 ```c++
 struct win32_offscreen_buffer
 {
   BITMAPINFO bmi;
   void* bitmap;
   int width;
   int height;
   int bytesPerPixel;
   int pitch;
 };
 
 static win32_offscreen_buffer GlobalBackBuffer;
 ```

#### Section#2 Windows GDI

Windows Graphics Device Interface

- Info: https://learn.microsoft.com/en-us/windows/win32/gdi/windows-gdi
- Enables applications to use graphics and formatted text on video display.
- Do not access the graphics hardware directly, interacts with device drivers.
- Message-Driven Architecture.

Device Context

- Main Purpose: Device Independence

  An application must inform GDI to load a particular device driver, once the driver is loaded, to prepare the device for drawing operations, the creating and maintaining of a DC is needed.

- Defines <u>a set of graphic objects</u> and their associated attributes, and the graphic modes that affect output.

  - **<u>Graphic Objects</u>**

    | gRAPHIC oBJECTS | aSSOCIATED attriBUTES                   |
    | --------------- | --------------------------------------- |
    | Bitmap          | size, bytes, dimensions, pixels...      |
    | Brush           | style, color...                         |
    | Palette         | colors and size (or number of colors).  |
    | Font            | width, height, weight, character set... |
    | Path            | shape                                   |
    | Pen             | style, width, color                     |
    | Region          | location, demensions                    |

#### Section#3 BITMAP structures

​	https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-structures

BITMAP

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmap
- A *bitmap* is a graphical object used to create, manipulate and store images **<u>as files on a disk.</u>**

BITMAPINFO

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfo

  - ```c++
    typedef struct tagBITMAPINFO {
      BITMAPINFOHEADER bmiHeader;
      RGBQUAD          bmiColors[1];
    } BITMAPINFO, *LPBITMAPINFO, *PBITMAPINFO;
    ```

- Contains information about the dimensions of color format.

- PS: we don't care about `bmiColors` since it's used as palette.

BITMAPINFO<u>HEADER</u>

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader

- Contains information about the dimensions and color format of a DIB.

- **<u>*Remarks</u>**: For uncompressed RGB bitmaps, if **biHeight** is positive, the bitmap is a bottom-up DIB with the origin at the lower left corner. If **biHeight** is negative, the bitmap is a top-down DIB with the origin at the upper left corner.

  | Positive  | Negative |
  | --------- | -------- |
  | bottom-up | top-down |

DIBSECTION

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-dibsection

  - ```c++
    typedef struct tagDIBSECTION {
      BITMAP           dsBm;
      BITMAPINFOHEADER dsBmih;
      DWORD            dsBitfields[3];
      HANDLE           dshSection;
      DWORD            dsOffset;
    } DIBSECTION, *LPDIBSECTION, *PDIBSECTION;
    ```

- Contains information about a DIB (Device-Independent-Bitmap)


#### Section#4 Resize DIBSection

Solution#1 Naïve Approach:

1. Delete the Old DIBSection
2. Create if DC not exist
3. Init bitmapInfo structure (mainly on bitmapHeader)
4. Create new DIBSection

`GetClientRect()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclientrect
- Retrieves the coordinates of a window's client area.

`CreateDIBSection()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection
- Create a DIB that your application can write to. 
- Gives you a pointer to the location of the <u>bitmap</u> bit values.

*Solution#2 Simplify: directly allocate and release the memory

```c++
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

  { /* Init struct BITMAPINFO
       mainly the bmiHeader part, cause we don't use the palette */
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
```

1. Release the old bitmap memory 
2. Init bitmapInfo structure (mainly on bitmapHeader)
3. Commit a compatible size of memory for new bitmap

`VirtualAlloc()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
- Reserves, commits, or changes the state of a region of <u>**pages**</u> in the virtual address space of the calling process.

`VirtualFree()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree

- Releases, decommits, or releases and decommits a region of <u>**pages**</u> within the virtual address space of the calling process.

  > If we need these memory in the future, set the `dwFreeType` with `MEM_DECOMMIT`, and the pages will be in reserved state, and we might need to `VirtualProect()` them.

#### Section#5 Copy Buffer To Window

1D -> 2D

- Origin of different DIB

  | Bottom-up  | Top-Down   |
  | ---------- | ---------- |
  | Lower-Left | Upper-Left |
- *RGB in Little Endian 32-bits Architecture

  - order: ARGB

  | 0x1000 | 0x1001 |
  | ------ | ------ |
  | GB     | AR     |
  | 34     | 12     |
- Test Drawing

  ![blue-green gradient rendering](part_1.assets/blue-green%20gradient%20rendering.png)

  ```c++
  { // FIXME: Test drawing
      int pitch = bitmapWidth * bytesPerPixel;
      UINT8* row = (UINT8*)bitmapMemory;
      for (int y = 0; y < bitmapHeight; y++) {
          UINT8* pixel = (UINT8*)row;
  
          for (int x = 0; x < bitmapWidth; x++) {
              *pixel = (UINT8)x;
              pixel += 1;
  
              *pixel = (UINT8)y;
              pixel += 1;
  
              *pixel = 0;
              pixel += 1;
  
              *pixel = 0;
              pixel += 1;
          }
          row += pitch;
      }
  }
  ```

Process:

1. Copy the source DIB Section to destination.

```c++
static void
Win32UpdateWindow(HDC deviceContext,
                  RECT clientRect,
                  int x,
                  int y,
                  int width,
                  int height)
{
  int windowWidth = clientRect.right - clientRect.left;
  int windowHeight = clientRect.bottom - clientRect.top;
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

```

`StretchDIBits()`

- Syntax: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-stretchdibits

- Copies the color data for a rectangle of pixels in a DIB, JPEG, or PNG image to the specified destination rectangle. If sizes don't match, this function <u>stretches/compresses</u> it to fit in.

- rop: raster-operation code

  1. Define how the color data for the source rectangle is to be combined with the color data for the destination rectangle to achieve the final color.

  
  > kinda like a Blend Mode?



