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

static unsigned delay_call_count;
static uint32_t last_delay_ms;

static void record_delay(uint32_t milliseconds, void *userdata)
{
    unsigned *counter = (unsigned *)userdata;

    ++(*counter);
    delay_call_count = *counter;
    last_delay_ms = milliseconds;
}

static unsigned present_call_count;
static uint8_t last_presented_red;

static bool record_present(
    const uint8_t *pixels,
    int pitch,
    const uint8_t palette[256][3],
    void *userdata)
{
    unsigned *counter = (unsigned *)userdata;

    assert(pixels != NULL);
    assert(pitch == 320);
    ++(*counter);
    present_call_count = *counter;
    last_presented_red = palette[5][0];
    return true;
}

static void test_palette_fades_and_timing(void)
{
    WolfLegacyVideo video;
    uint8_t target_palette[256][3] = {{0}};
    unsigned callback_count = 0;
    unsigned present_count = 0;

    memset(vga_storage, 0, sizeof(vga_storage));
    assert(wolf_legacy_video_init(
        &video,
        vga_storage,
        sizeof(vga_storage),
        framebuffer_storage,
        sizeof(framebuffer_storage),
        320));

    wolf_legacy_video_set_delay_callback(&video, record_delay, &callback_count);
    wolf_legacy_video_set_present_callback(
        &video, record_present, &present_count);
    wolf_legacy_video_set_color(&video, 5, 60, 30, 0);
    wolf_legacy_video_fade_out(&video, 5, 5, 0, 0, 0, 2);

    assert(video.screen_faded);
    assert(video.palette_dac[5][0] == 0);
    assert(video.palette_dac[5][1] == 0);
    assert(video.palette_dac[5][2] == 0);
    assert(video.palette_dac[0][0] == 0);
    assert(callback_count == 3);
    assert(delay_call_count == 3);
    assert(last_delay_ms == 15);
    assert(present_count == 3);
    assert(present_call_count == 3);
    assert(last_presented_red == 0);

    target_palette[5][0] = 63;
    target_palette[5][1] = 32;
    target_palette[5][2] = 16;
    target_palette[6][0] = 20;
    target_palette[6][1] = 10;

    wolf_legacy_video_fade_in(
        &video, 5, 5, (const uint8_t (*)[3])target_palette, 2);

    assert(!video.screen_faded);
    assert(video.palette_dac[5][0] == 63);
    assert(video.palette_dac[5][1] == 32);
    assert(video.palette_dac[5][2] == 16);
    assert(video.palette_dac[6][0] == 20);
    assert(video.palette_dac[6][1] == 10);
    assert(callback_count == 6);
    assert(last_delay_ms == 15);
    assert(present_count == 6);
    assert(present_call_count == 6);
    assert(last_presented_red == 255);
}

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
    test_palette_fades_and_timing();
    test_image_and_latch_path();

    puts("legacy video adapter tests passed");
    return 0;
}
