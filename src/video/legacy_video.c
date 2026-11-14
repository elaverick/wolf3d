#include "legacy_video.h"

#include <limits.h>
#include <string.h>

static uint8_t clamp_dac_component(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 63) {
        return 63;
    }
    return (uint8_t)value;
}

static void refresh_rgb_palette(WolfLegacyVideo *video)
{
    wolf_palette_expand_vga6(
        (const uint8_t (*)[3])video->palette_dac,
        video->palette_rgb);
}

bool wolf_legacy_video_init(
    WolfLegacyVideo *video,
    uint8_t *vga_storage,
    size_t vga_storage_size,
    uint8_t *framebuffer_storage,
    size_t framebuffer_storage_size,
    size_t framebuffer_pitch)
{
    if (video == NULL || framebuffer_pitch < 320 || framebuffer_pitch > INT_MAX) {
        return false;
    }

    if (!wolf_vga_memory_init_legacy(
            &video->memory, vga_storage, vga_storage_size)) {
        return false;
    }

    if (!wolf_framebuffer_init(
            &video->framebuffer,
            320,
            200,
            framebuffer_pitch,
            framebuffer_storage,
            framebuffer_storage_size)) {
        return false;
    }

    wolf_vga_memory_clear(&video->memory, 0);
    wolf_framebuffer_clear(&video->framebuffer, 0);

    video->draw_page_offset = WOLF_VGA_LEGACY_PAGE1_OFFSET;
    video->display_page_offset = WOLF_VGA_LEGACY_PAGE1_OFFSET;
    video->delay_callback = NULL;
    video->delay_userdata = NULL;
    video->present_callback = NULL;
    video->present_userdata = NULL;
    video->screen_faded = false;
    video->border_colour = 0;
    memset(video->palette_dac, 0, sizeof(video->palette_dac));
    refresh_rgb_palette(video);
    return true;
}

void wolf_legacy_video_set_draw_page(WolfLegacyVideo *video, size_t page_offset)
{
    if (video != NULL && page_offset < video->memory.plane_size) {
        video->draw_page_offset = page_offset;
    }
}

void wolf_legacy_video_set_display_page(WolfLegacyVideo *video, size_t page_offset)
{
    if (video != NULL && page_offset < video->memory.plane_size) {
        video->display_page_offset = page_offset;
    }
}

void wolf_legacy_video_clear(WolfLegacyVideo *video, uint8_t colour)
{
    if (video != NULL) {
        wolf_vga_memory_clear(&video->memory, colour);
    }
}

void wolf_legacy_video_plot(WolfLegacyVideo *video, int x, int y, uint8_t colour)
{
    if (video != NULL) {
        wolf_vga_memory_plot(
            &video->memory, video->draw_page_offset, x, y, colour);
    }
}

void wolf_legacy_video_hline(
    WolfLegacyVideo *video, int x, int y, int width, uint8_t colour)
{
    if (video != NULL) {
        wolf_vga_memory_hline(
            &video->memory, video->draw_page_offset, x, y, width, colour);
    }
}

void wolf_legacy_video_vline(
    WolfLegacyVideo *video, int x, int y, int height, uint8_t colour)
{
    if (video != NULL) {
        wolf_vga_memory_vline(
            &video->memory, video->draw_page_offset, x, y, height, colour);
    }
}

void wolf_legacy_video_fill_rect(
    WolfLegacyVideo *video,
    int x,
    int y,
    int width,
    int height,
    uint8_t colour)
{
    if (video != NULL) {
        wolf_vga_memory_fill_rect(
            &video->memory,
            video->draw_page_offset,
            x,
            y,
            width,
            height,
            colour);
    }
}

bool wolf_legacy_video_munge_picture(
    uint8_t *image, size_t image_size, int width, int height)
{
    return wolf_image_chunky_to_planar4(image, image_size, width, height);
}

bool wolf_legacy_video_mem_to_screen(
    WolfLegacyVideo *video,
    int x,
    int y,
    int width,
    int height,
    const uint8_t *planar_image,
    size_t image_size)
{
    if (video == NULL) {
        return false;
    }

    return wolf_vga_memory_mem_to_screen(
        &video->memory,
        video->draw_page_offset,
        x,
        y,
        width,
        height,
        planar_image,
        image_size);
}

bool wolf_legacy_video_mem_to_latch(
    WolfLegacyVideo *video,
    size_t latch_offset,
    int width,
    int height,
    const uint8_t *planar_image,
    size_t image_size)
{
    if (video == NULL) {
        return false;
    }

    return wolf_vga_memory_mem_to_latch(
        &video->memory,
        latch_offset,
        width,
        height,
        planar_image,
        image_size);
}

bool wolf_legacy_video_latch_to_screen(
    WolfLegacyVideo *video,
    size_t latch_offset,
    int x,
    int y,
    int width_bytes,
    int height)
{
    if (video == NULL) {
        return false;
    }

    return wolf_vga_memory_latch_to_screen(
        &video->memory,
        latch_offset,
        video->draw_page_offset,
        x,
        y,
        width_bytes,
        height);
}

bool wolf_legacy_video_scale_post(
    WolfLegacyVideo *video,
    int x,
    int view_y,
    int view_height,
    int post_width,
    const uint8_t *source,
    size_t source_size,
    int source_height,
    int destination_height)
{
    if (video == NULL) {
        return false;
    }

    return wolf_vga_memory_scale_post(
        &video->memory,
        video->draw_page_offset,
        x,
        view_y,
        view_height,
        post_width,
        source,
        source_size,
        source_height,
        destination_height);
}

void wolf_legacy_video_set_delay_callback(
    WolfLegacyVideo *video,
    WolfLegacyVideoDelayFn callback,
    void *userdata)
{
    if (video == NULL) {
        return;
    }

    video->delay_callback = callback;
    video->delay_userdata = userdata;
}

void wolf_legacy_video_wait_vbl(WolfLegacyVideo *video, int vbls)
{
    uint64_t milliseconds;

    if (video == NULL || video->delay_callback == NULL || vbls <= 0) {
        return;
    }

    milliseconds = ((uint64_t)(unsigned)vbls * 1000u + 69u) / 70u;
    if (milliseconds > UINT32_MAX) {
        milliseconds = UINT32_MAX;
    }

    video->delay_callback((uint32_t)milliseconds, video->delay_userdata);
}

void wolf_legacy_video_set_present_callback(
    WolfLegacyVideo *video,
    WolfLegacyVideoPresentFn callback,
    void *userdata)
{
    if (video == NULL) {
        return;
    }

    video->present_callback = callback;
    video->present_userdata = userdata;
}

bool wolf_legacy_video_present_current(const WolfLegacyVideo *video)
{
    if (video == NULL) {
        return false;
    }

    if (video->present_callback == NULL) {
        return true;
    }

    return video->present_callback(
        video->framebuffer.pixels,
        (int)video->framebuffer.pitch,
        (const uint8_t (*)[3])video->palette_rgb,
        video->present_userdata);
}

void wolf_legacy_video_fade_out(
    WolfLegacyVideo *video,
    int first_colour,
    int last_colour,
    int red,
    int green,
    int blue,
    int steps)
{
    uint8_t original[256][3];
    uint8_t working[256][3];
    uint8_t target[3];
    int index;
    int component;
    int step;

    if (video == NULL) {
        return;
    }

    if (first_colour < 0) first_colour = 0;
    if (last_colour > 255) last_colour = 255;
    if (first_colour > 255) first_colour = 255;
    if (last_colour < 0) last_colour = 0;

    target[0] = clamp_dac_component(red);
    target[1] = clamp_dac_component(green);
    target[2] = clamp_dac_component(blue);

    wolf_legacy_video_wait_vbl(video, 1);
    wolf_legacy_video_get_palette(video, original);

    if (steps > 0 && first_colour <= last_colour) {
        for (step = 0; step < steps; ++step) {
            memcpy(working, original, sizeof(working));

            for (index = first_colour; index <= last_colour; ++index) {
                for (component = 0; component < 3; ++component) {
                    int start = original[index][component];
                    int64_t delta = (int)target[component] - start;

                    working[index][component] =
                        (uint8_t)(start + delta * step / steps);
                }
            }

            wolf_legacy_video_wait_vbl(video, 1);
            wolf_legacy_video_set_palette(
                video, (const uint8_t (*)[3])working);
            (void)wolf_legacy_video_present_current(video);
        }
    }

    wolf_legacy_video_fill_palette(
        video, target[0], target[1], target[2]);
    video->screen_faded = true;
    (void)wolf_legacy_video_present_current(video);
}

void wolf_legacy_video_fade_in(
    WolfLegacyVideo *video,
    int first_colour,
    int last_colour,
    const uint8_t target_palette[256][3],
    int steps)
{
    uint8_t original[256][3];
    uint8_t target[256][3];
    uint8_t working[256][3];
    int index;
    int component;
    int step;

    if (video == NULL || target_palette == NULL) {
        return;
    }

    if (first_colour < 0) first_colour = 0;
    if (last_colour > 255) last_colour = 255;
    if (first_colour > 255) first_colour = 255;
    if (last_colour < 0) last_colour = 0;

    wolf_legacy_video_wait_vbl(video, 1);
    wolf_legacy_video_get_palette(video, original);
    memcpy(target, target_palette, sizeof(target));

    if (steps > 0 && first_colour <= last_colour) {
        for (step = 0; step < steps; ++step) {
            memcpy(working, original, sizeof(working));

            for (index = first_colour; index <= last_colour; ++index) {
                for (component = 0; component < 3; ++component) {
                    int start = original[index][component];
                    int64_t delta = (int)target[index][component] - start;

                    working[index][component] =
                        (uint8_t)(start + delta * step / steps);
                }
            }

            wolf_legacy_video_wait_vbl(video, 1);
            wolf_legacy_video_set_palette(
                video, (const uint8_t (*)[3])working);
            (void)wolf_legacy_video_present_current(video);
        }
    }

    wolf_legacy_video_set_palette(
        video, (const uint8_t (*)[3])target);
    video->screen_faded = false;
    (void)wolf_legacy_video_present_current(video);
}

void wolf_legacy_video_fill_palette(
    WolfLegacyVideo *video, int red, int green, int blue)
{
    int index;

    if (video == NULL) {
        return;
    }

    for (index = 0; index < 256; ++index) {
        video->palette_dac[index][0] = clamp_dac_component(red);
        video->palette_dac[index][1] = clamp_dac_component(green);
        video->palette_dac[index][2] = clamp_dac_component(blue);
    }

    refresh_rgb_palette(video);
}

void wolf_legacy_video_set_color(
    WolfLegacyVideo *video, int index, int red, int green, int blue)
{
    if (video == NULL || index < 0 || index > 255) {
        return;
    }

    video->palette_dac[index][0] = clamp_dac_component(red);
    video->palette_dac[index][1] = clamp_dac_component(green);
    video->palette_dac[index][2] = clamp_dac_component(blue);
    refresh_rgb_palette(video);
}

void wolf_legacy_video_get_color(
    const WolfLegacyVideo *video, int index, int *red, int *green, int *blue)
{
    if (video == NULL || index < 0 || index > 255) {
        return;
    }

    if (red != NULL) *red = video->palette_dac[index][0];
    if (green != NULL) *green = video->palette_dac[index][1];
    if (blue != NULL) *blue = video->palette_dac[index][2];
}

void wolf_legacy_video_set_palette(
    WolfLegacyVideo *video, const uint8_t palette[256][3])
{
    int index;
    int component;

    if (video == NULL || palette == NULL) {
        return;
    }

    for (index = 0; index < 256; ++index) {
        for (component = 0; component < 3; ++component) {
            video->palette_dac[index][component] =
                palette[index][component] > 63 ? 63 : palette[index][component];
        }
    }

    refresh_rgb_palette(video);
}

void wolf_legacy_video_get_palette(
    const WolfLegacyVideo *video, uint8_t palette[256][3])
{
    if (video == NULL || palette == NULL) {
        return;
    }

    memcpy(palette, video->palette_dac, sizeof(video->palette_dac));
}

bool wolf_legacy_video_present(WolfLegacyVideo *video)
{
    if (video == NULL) {
        return false;
    }

    if (!wolf_vga_memory_present(
            &video->memory,
            video->display_page_offset,
            &video->framebuffer)) {
        return false;
    }

    return wolf_legacy_video_present_current(video);
}
