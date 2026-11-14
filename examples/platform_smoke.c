/*
 * End-to-end smoke test for the software VGA and SDL3 presentation path.
 *
 * This intentionally exercises the same legacy video context that the game
 * will use, without depending on the still-unported game loop.
 */

#include "legacy_video.h"
#include "platform.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t vga_storage[
    WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT];
static uint8_t framebuffer_storage[320 * 200];

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

static void draw_test_scene(WolfLegacyVideo *video)
{
    uint8_t wall_column[64];
    int index;

    for (index = 0; index < 256; ++index) {
        wolf_legacy_video_set_color(
            video,
            index,
            index & 63,
            (index * 3) & 63,
            (index * 5) & 63);
    }

    wolf_legacy_video_clear(video, 0);
    wolf_legacy_video_fill_rect(video, 0, 0, 320, 200, 4);
    wolf_legacy_video_fill_rect(video, 16, 16, 288, 168, 12);
    wolf_legacy_video_fill_rect(video, 32, 32, 256, 136, 3);
    wolf_legacy_video_fill_rect(video, 40, 40, 240, 120, 7);

    for (index = 0; index < 64; ++index) {
        wall_column[index] = (uint8_t)(32 + ((index * 3) & 31));
    }

    wolf_legacy_video_scale_post(
        video,
        136,
        0,
        200,
        48,
        wall_column,
        sizeof(wall_column),
        64,
        144);

    wolf_legacy_video_fill_rect(video, 24, 176, 272, 8, 63);
}

int main(void)
{
    WolfPlatformConfig config = {
        "Wolfenstein 3D - native video smoke test",
        960,
        600,
        320,
        200
    };
    WolfPlatform *platform;
    WolfLegacyVideo video;

    platform = wolf_platform_create(&config);
    if (platform == NULL) {
        fputs("Unable to initialise SDL3 platform.\n", stderr);
        return 1;
    }

    if (!wolf_legacy_video_init(
            &video,
            vga_storage,
            sizeof(vga_storage),
            framebuffer_storage,
            sizeof(framebuffer_storage),
            320)) {
        fputs("Unable to initialise software VGA memory.\n", stderr);
        wolf_platform_destroy(platform);
        return 1;
    }

    wolf_legacy_video_set_present_callback(
        &video, present_to_platform, platform);
    wolf_legacy_video_set_draw_page(
        &video, WOLF_VGA_LEGACY_PAGE1_OFFSET);
    wolf_legacy_video_set_display_page(
        &video, WOLF_VGA_LEGACY_PAGE1_OFFSET);

    draw_test_scene(&video);

    while (wolf_platform_pump_events(platform)) {
        if (!wolf_legacy_video_present(&video)) {
            fputs("Unable to present software VGA frame.\n", stderr);
            break;
        }

        wolf_platform_delay_ms(16);
    }

    wolf_platform_destroy(platform);
    return 0;
}
