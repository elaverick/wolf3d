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

/* Release all platform resources. Safe to call with NULL. */
void wolf_platform_destroy(WolfPlatform *platform);

#endif
