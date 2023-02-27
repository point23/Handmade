#if !defined(WIN32_HANDMADE_H)

#include <dSound.h.>
#include <stdio.h>
#include <windows.h>
#include <xinput.h>

#define Direct_Sound_Buffer LPDIRECTSOUNDBUFFER
#define monitor_refresh_rate 60
#define game_update_rate (monitor_refresh_rate >> 1)

struct Win32_Back_Buffer
{
    BITMAPINFO bmi;
    void* bitmap;
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

    // @note About 1/3 frame...
    // &: Here we ask for current sound buffer cursor
    // w: Write cursor position
    // |**********|: Bytes of exactly one frame of audio.
    // On a low latency machine, we only need to write the bytes of one frame.
    // Low latency:  |....&...w...|.....&.....|
    //                            |***********|
    // On a high latency machine, we need to write a few extra
    //  bytes in case we don't hear any audio *gaps*
    // High latency: |....&.......|..w..&.....|.....&
    //                               |***********|__|
    //                                        safty bytes
    const s32 safety_bytes =
      (samples_per_second * bytes_per_sample) / game_update_rate / 3;
    u32 running_sample_idx = 0;
};

#define WIN32_HANDMADE_H
#endif