/*
 * End-to-end smoke test for the portable VL API and SDL3 presentation path.
 *
 * This exercises the compatibility functions that the original game code
 * expects, without depending on the still-unported game loop.
 */

#include "legacy_vl.h"
#include "platform.h"

#include <SDL3/SDL.h>

#include <stdint.h>
#include "native_tick.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WolfNativeTick smoke_timer_ticks;

static void count_timer_tick(void *userdata)
{
    (void)userdata;
    (void)wolf_native_tick_increment(&smoke_timer_ticks);
}

static bool present_to_platform(
    const uint8_t *pixels,
    int pitch,
    const uint8_t palette[256][3],
    void *userdata)
{
    WolfPlatform *platform = userdata;

    return wolf_platform_present_indexed(
        platform, pixels, pitch, palette);
}

static void delay_video(uint32_t milliseconds, void *userdata)
{
    (void)userdata;
    wolf_platform_delay_ms(milliseconds);
}

static void delay_with_platform(uint32_t milliseconds, void *userdata)
{
    (void)userdata;
    wolf_platform_delay_ms(milliseconds);
}

static void draw_test_scene(void)
{
    uint8_t wall_column[64];
    int index;

    for (index = 0; index < 256; ++index) {
        VL_SetColor(
            index,
            index & 63,
            (index * 3) & 63,
            (index * 5) & 63);
    }

    VL_ClearVideo(0);
    VL_Bar(0, 0, 320, 200, 4);
    VL_Bar(16, 16, 288, 168, 12);
    VL_Bar(32, 32, 256, 136, 3);
    VL_Bar(40, 40, 240, 120, 7);

    for (index = 0; index < 64; ++index) {
        wall_column[index] = (uint8_t)(32 + ((index * 3) & 31));
    }

    (void)wolf_legacy_video_scale_post(
        VL_GetVideo(),
        136,
        0,
        200,
        48,
        wall_column,
        sizeof(wall_column),
        64,
        144);

    VL_Bar(24, 176, 272, 8, 63);
}

int main(int argc, char **argv)
{
    WolfPlatformConfig config = {
        "Wolfenstein 3D - native video smoke test",
        960,
        600,
        320,
        200
    };
    WolfPlatform *platform;
    int frame_limit = 0;
    int frame_count = 0;

    if (argc == 3 && strcmp(argv[1], "--frames") == 0) {
        frame_limit = atoi(argv[2]);
        if (frame_limit < 1) {
            fputs("Frame limit must be positive.\n", stderr);
            return 1;
        }
    }

    platform = wolf_platform_create(&config);
    if (platform == NULL) {
        fputs("Unable to initialise SDL3 platform.\n", stderr);
        return 1;
    }

    wolf_native_tick_store(&smoke_timer_ticks, 0u);
    if (!wolf_platform_timer_start(platform, 14u, count_timer_tick, NULL)) {
        fputs("Unable to start SDL3 timer service.\n", stderr);
        wolf_platform_destroy(platform);
        return 1;
    }
    wolf_platform_delay_ms(50u);
    wolf_platform_timer_stop(platform);
    if (wolf_native_tick_load(&smoke_timer_ticks) < 2u) {
        fputs("SDL3 timer service did not deliver its callbacks.\n", stderr);
        wolf_platform_destroy(platform);
        return 1;
    }

    VL_Startup();
    wolf_legacy_video_set_present_callback(
        VL_GetVideo(), present_to_platform, platform);
    wolf_legacy_video_set_delay_callback(
        VL_GetVideo(), delay_video, platform);
    VL_SetLineWidth(40);
    VL_SetScreen((int)WOLF_VGA_LEGACY_PAGE1_OFFSET, 0);
    bufferofs = WOLF_VGA_LEGACY_PAGE1_OFFSET;

    draw_test_scene();

    while (wolf_platform_pump_events(platform)) {
        if (!VL_Present()) {
            fputs("Unable to present software VGA frame.\n", stderr);
            break;
        }

        ++frame_count;
        if (frame_limit > 0 && frame_count >= frame_limit) {
            break;
        }

        wolf_platform_delay_ms(16);
    }

    /*
     * Exercise a window-manager close request directly. SDL may also emit a
     * global quit event depending on SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, but
     * the host must handle the per-window request on its own.
     */
    {
        SDL_Event close_event = { 0 };
        close_event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        if (!SDL_PushEvent(&close_event) ||
            wolf_platform_pump_events(platform)) {
            fputs("SDL window-close request was not handled.\n", stderr);
            VL_Shutdown();
            wolf_platform_destroy(platform);
            return 1;
        }
    }

    VL_Shutdown();
    wolf_platform_destroy(platform);
    return 0;
}
