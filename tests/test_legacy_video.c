#ifdef NDEBUG
#undef NDEBUG
#endif

#include "legacy_video.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t vga_storage[
    WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT];
static uint8_t framebuffer_storage[320 * 200];

static void test_video_page_and_palette(void)
{
    WolfLegacyVideo video;
    uint8_t palette[256][3];
    int red;
    int green;
    int blue;

    memset(vga_storage, 0, sizeof(vga_storage));
    memset(framebuffer_storage, 0xee, sizeof(framebuffer_storage));

    assert(wolf_legacy_video_init(
        &video,
        vga_storage,
        sizeof(vga_storage),
        framebuffer_storage,
        sizeof(framebuffer_storage),
        320));

    wolf_legacy_video_set_draw_page(
        &video, WOLF_VGA_LEGACY_PAGE2_OFFSET);
    wolf_legacy_video_set_display_page(
        &video, WOLF_VGA_LEGACY_PAGE2_OFFSET);

    wolf_legacy_video_set_color(&video, 1, 63, 0, 31);
    wolf_legacy_video_get_color(&video, 1, &red, &green, &blue);
    assert(red == 63 && green == 0 && blue == 31);
    assert(video.palette_rgb[1][0] == 255);
    assert(video.palette_rgb[1][1] == 0);
    assert(video.palette_rgb[1][2] == 125);

    wolf_legacy_video_plot(&video, 319, 199, 1);
    assert(wolf_legacy_video_present(&video));
    assert(framebuffer_storage[199 * 320 + 319] == 1);

    wolf_legacy_video_set_display_page(
        &video, WOLF_VGA_LEGACY_PAGE1_OFFSET);
    assert(wolf_legacy_video_present(&video));
    assert(framebuffer_storage[199 * 320 + 319] == 0);

    wolf_legacy_video_fill_palette(&video, 70, -1, 12);
    assert(video.palette_dac[0][0] == 63);
    assert(video.palette_dac[0][1] == 0);
    assert(video.palette_dac[0][2] == 12);

    wolf_legacy_video_get_palette(&video, palette);
    assert(palette[0][0] == 63);
    assert(palette[0][1] == 0);
    assert(palette[0][2] == 12);
}

static void test_image_and_latch_path(void)
{
    WolfLegacyVideo video;
    uint8_t image[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    memset(vga_storage, 0, sizeof(vga_storage));
    memset(framebuffer_storage, 0, sizeof(framebuffer_storage));

    assert(wolf_legacy_video_init(
        &video,
        vga_storage,
        sizeof(vga_storage),
        framebuffer_storage,
        sizeof(framebuffer_storage),
        320));

    wolf_legacy_video_set_draw_page(
        &video, WOLF_VGA_LEGACY_PAGE3_OFFSET);
    wolf_legacy_video_set_display_page(
        &video, WOLF_VGA_LEGACY_PAGE3_OFFSET);

    assert(wolf_legacy_video_munge_picture(image, sizeof(image), 4, 2));
    assert(wolf_legacy_video_mem_to_screen(
        &video, 0, 0, 4, 2, image, sizeof(image)));
    assert(wolf_legacy_video_present(&video));
    for (int x = 0; x < 4; ++x) {
        assert(framebuffer_storage[x] == (uint8_t)x);
        assert(framebuffer_storage[320 + x] == (uint8_t)(x + 4));
    }

    assert(wolf_legacy_video_mem_to_latch(
        &video,
        WOLF_VGA_LEGACY_LATCH_OFFSET,
        4,
        2,
        image,
        sizeof(image)));
    assert(wolf_legacy_video_latch_to_screen(
        &video, WOLF_VGA_LEGACY_LATCH_OFFSET, 4, 0, 1, 2));
    assert(wolf_legacy_video_present(&video));
    for (int x = 0; x < 4; ++x) {
        assert(framebuffer_storage[4 + x] == (uint8_t)x);
        assert(framebuffer_storage[320 + 4 + x] == (uint8_t)(x + 4));
    }
}

int main(void)
{
    test_video_page_and_palette();
    test_image_and_latch_path();

    puts("legacy video adapter tests passed");
    return 0;
}
