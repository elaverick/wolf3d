/*
 * Game-facing adapter for the original Wolfenstein video-manager concepts.
 *
 * This context owns no memory. It binds the software VGA planes, selected
 * drawing/display pages, linear presentation buffer and original VGA palette.
 * SDL and operating-system details remain outside this module.
 */

#ifndef WOLF3D_LEGACY_VIDEO_H
#define WOLF3D_LEGACY_VIDEO_H

#include "vga_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct WolfLegacyVideo {
    WolfVgaMemory memory;
    WolfFramebuffer framebuffer;
    size_t draw_page_offset;
    size_t display_page_offset;
    uint8_t palette_dac[256][3];
    uint8_t palette_rgb[256][3];
} WolfLegacyVideo;

bool wolf_legacy_video_init(
    WolfLegacyVideo *video,
    uint8_t *vga_storage,
    size_t vga_storage_size,
    uint8_t *framebuffer_storage,
    size_t framebuffer_storage_size,
    size_t framebuffer_pitch);

void wolf_legacy_video_set_draw_page(WolfLegacyVideo *video, size_t page_offset);
void wolf_legacy_video_set_display_page(WolfLegacyVideo *video, size_t page_offset);

void wolf_legacy_video_clear(WolfLegacyVideo *video, uint8_t colour);
void wolf_legacy_video_plot(WolfLegacyVideo *video, int x, int y, uint8_t colour);
void wolf_legacy_video_hline(
    WolfLegacyVideo *video, int x, int y, int width, uint8_t colour);
void wolf_legacy_video_vline(
    WolfLegacyVideo *video, int x, int y, int height, uint8_t colour);
void wolf_legacy_video_fill_rect(
    WolfLegacyVideo *video,
    int x,
    int y,
    int width,
    int height,
    uint8_t colour);

bool wolf_legacy_video_munge_picture(
    uint8_t *image, size_t image_size, int width, int height);
bool wolf_legacy_video_mem_to_screen(
    WolfLegacyVideo *video,
    int x,
    int y,
    int width,
    int height,
    const uint8_t *planar_image,
    size_t image_size);
bool wolf_legacy_video_mem_to_latch(
    WolfLegacyVideo *video,
    size_t latch_offset,
    int width,
    int height,
    const uint8_t *planar_image,
    size_t image_size);
bool wolf_legacy_video_latch_to_screen(
    WolfLegacyVideo *video,
    size_t latch_offset,
    int x,
    int y,
    int width_bytes,
    int height);
bool wolf_legacy_video_scale_post(
    WolfLegacyVideo *video,
    int x,
    int view_y,
    int view_height,
    int post_width,
    const uint8_t *source,
    size_t source_size,
    int source_height,
    int destination_height);

void wolf_legacy_video_fill_palette(
    WolfLegacyVideo *video, int red, int green, int blue);
void wolf_legacy_video_set_color(
    WolfLegacyVideo *video, int index, int red, int green, int blue);
void wolf_legacy_video_get_color(
    const WolfLegacyVideo *video, int index, int *red, int *green, int *blue);
void wolf_legacy_video_set_palette(
    WolfLegacyVideo *video, const uint8_t palette[256][3]);
void wolf_legacy_video_get_palette(
    const WolfLegacyVideo *video, uint8_t palette[256][3]);

/* Convert the selected display page into the linear framebuffer. */
bool wolf_legacy_video_present(WolfLegacyVideo *video);

#endif
