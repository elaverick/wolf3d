/*
 * Platform services used by the game.
 *
 * Keep operating-system and multimedia-library details behind this interface.
 * Game code should depend on these functions, not directly on SDL or Win32.
 */

#ifndef WOLF3D_PLATFORM_H
#define WOLF3D_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct WolfPlatform WolfPlatform;

typedef struct WolfPlatformConfig {
    const char *window_title;
    int window_width;
    int window_height;
    int framebuffer_width;
    int framebuffer_height;
} WolfPlatformConfig;

/* Initialise the platform and create the game window and indexed framebuffer. */
WolfPlatform *wolf_platform_create(const WolfPlatformConfig *config);

/* Process pending operating-system events. Returns false when the app should exit. */
bool wolf_platform_pump_events(WolfPlatform *platform);

/*
 * Present an 8-bit indexed framebuffer.
 *
 * pixels points to framebuffer_height rows, each containing at least pitch
 * bytes. palette contains 256 RGB entries with component values in 0..255.
 * The platform owns neither pointer and copies the frame before returning.
 */
bool wolf_platform_present_indexed(
    WolfPlatform *platform,
    const uint8_t *pixels,
    int pitch,
    const uint8_t palette[256][3]);

/* Monotonic time in milliseconds. */
uint64_t wolf_platform_time_ms(void);

/* Sleep for at least the requested number of milliseconds. */
void wolf_platform_delay_ms(uint32_t milliseconds);

/* Legacy mouse-driver command bridge used by the original input code. */
/* Keyboard state and edge events expressed as original DOS scan codes. */
bool wolf_platform_key_down(int scan_code);
bool wolf_platform_take_key_event(int *scan_code);
int wolf_platform_take_ascii(void);

void wolf_platform_mouse_command(
    int function,
    int *ax,
    int *bx,
    int *cx,
    int *dx);

/* Queue mono signed 16-bit PCM samples. The stream copies the data. */
bool wolf_platform_audio_submit(const int16_t *samples, size_t sample_count);

/* Discard pending playback, used when a higher-priority effect interrupts. */
void wolf_platform_audio_clear(void);

/* Number of bytes queued for playback, or -1 when audio is unavailable. */
int wolf_platform_audio_queued_bytes(void);

/* Release all platform resources. Safe to call with NULL. */
void wolf_platform_destroy(WolfPlatform *platform);

#endif
