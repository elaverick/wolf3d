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
} WolfPlatformConfig;

/* Initialise the platform and create the game window. */
WolfPlatform *wolf_platform_create(const WolfPlatformConfig *config);

/* Process pending operating-system events. Returns false when the app should exit. */
bool wolf_platform_pump_events(WolfPlatform *platform);

/* Monotonic time in milliseconds. */
uint64_t wolf_platform_time_ms(void);

/* Release all platform resources. Safe to call with NULL. */
void wolf_platform_destroy(WolfPlatform *platform);

#endif
