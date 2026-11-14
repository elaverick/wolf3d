/*
 * Portable 8-bit indexed framebuffer primitives.
 *
 * Pixels are stored row-major. The caller owns the storage; pitch may exceed
 * width to support aligned rows or externally managed buffers.
 */

#ifndef WOLF3D_INDEXED_FRAMEBUFFER_H
#define WOLF3D_INDEXED_FRAMEBUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct WolfFramebuffer {
    int width;
    int height;
    size_t pitch;
    uint8_t *pixels;
} WolfFramebuffer;

/*
 * Bind a framebuffer to caller-owned memory.
 *
 * storage_size is the number of writable bytes available at pixels. The
 * required storage is (height - 1) * pitch + width. Padding bytes are never
 * modified by drawing operations.
 */
bool wolf_framebuffer_init(
    WolfFramebuffer *framebuffer,
    int width,
    int height,
    size_t pitch,
    uint8_t *pixels,
    size_t storage_size);

void wolf_framebuffer_clear(WolfFramebuffer *framebuffer, uint8_t colour);
void wolf_framebuffer_put_pixel(
    WolfFramebuffer *framebuffer, int x, int y, uint8_t colour);
void wolf_framebuffer_hline(
    WolfFramebuffer *framebuffer, int x, int y, int width, uint8_t colour);
void wolf_framebuffer_vline(
    WolfFramebuffer *framebuffer, int x, int y, int height, uint8_t colour);
void wolf_framebuffer_fill_rect(
    WolfFramebuffer *framebuffer,
    int x,
    int y,
    int width,
    int height,
    uint8_t colour);

/*
 * Convert a legacy VGA 6-bit DAC palette to 8-bit RGB components. Values above
 * the VGA range are clamped to 63.
 */
void wolf_palette_expand_vga6(
    const uint8_t source[256][3],
    uint8_t destination[256][3]);

/*
 * Draw an image stored in the original Wolfenstein planar format.
 *
 * The source contains four consecutive planes. Each plane contains
 * (width / 4) bytes per row, and width must be divisible by four.
 */
bool wolf_framebuffer_blit_planar4(
    WolfFramebuffer *framebuffer,
    int x,
    int y,
    int width,
    int height,
    const uint8_t *source,
    size_t source_size);

/* Copy a rectangle, clipping both source and destination to their bounds. */
void wolf_framebuffer_copy(
    WolfFramebuffer *destination,
    int destination_x,
    int destination_y,
    const WolfFramebuffer *source,
    int source_x,
    int source_y,
    int width,
    int height);

#endif
