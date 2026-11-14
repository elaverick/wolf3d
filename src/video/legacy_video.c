#include "legacy_video.h"

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
    if (video == NULL || framebuffer_pitch < 320) {
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

    video->draw_page_offset = WOLF_VGA_LEGACY_PAGE1_OFFSET;
    video->display_page_offset = WOLF_VGA_LEGACY_PAGE1_OFFSET;
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

    return wolf_vga_memory_present(
        &video->memory,
        video->display_page_offset,
        &video->framebuffer);
}
