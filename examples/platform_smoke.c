/*
 * Small interactive test for the SDL3 indexed-framebuffer presentation path.
 *
 * This is intentionally separate from the game. It verifies that the native
 * window, indexed-to-RGBA conversion and texture presentation work before the
 * original engine is connected.
 */

#include "platform.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FRAME_WIDTH 320
#define FRAME_HEIGHT 200

int main(void)
{
    WolfPlatformConfig config = {
        "Wolfenstein 3D - platform smoke test",
        960,
        600,
        FRAME_WIDTH,
        FRAME_HEIGHT
    };
    WolfPlatform *platform;
    uint8_t *framebuffer;
    uint8_t palette[256][3];
    int x;
    int y;
    int running = 1;

    framebuffer = malloc(FRAME_WIDTH * FRAME_HEIGHT);
    if (framebuffer == NULL) {
        fputs("Unable to allocate smoke-test framebuffer.\n", stderr);
        return EXIT_FAILURE;
    }

    for (x = 0; x < 256; ++x) {
        palette[x][0] = (uint8_t)x;
        palette[x][1] = (uint8_t)(255 - x);
        palette[x][2] = (uint8_t)((x * 3) & 255);
    }

    for (y = 0; y < FRAME_HEIGHT; ++y) {
        for (x = 0; x < FRAME_WIDTH; ++x) {
            framebuffer[y * FRAME_WIDTH + x] =
                (uint8_t)(((x / 8) + (y / 8) * 3) & 255);
        }
    }

    platform = wolf_platform_create(&config);
    if (platform == NULL) {
        fputs("Unable to initialise SDL3 platform.\n", stderr);
        free(framebuffer);
        return EXIT_FAILURE;
    }

    while (running) {
        running = wolf_platform_pump_events(platform);

        if (running && !wolf_platform_present_indexed(
                platform, framebuffer, FRAME_WIDTH, palette)) {
            fputs("Unable to present indexed framebuffer.\n", stderr);
            running = 0;
        }
    }

    wolf_platform_destroy(platform);
    free(framebuffer);
    return EXIT_SUCCESS;
}
