/*
 * Software replacement for the original four-plane VGA memory.
 *
 * The engine addresses VGA memory as four independent planes. This structure
 * preserves that addressing model in ordinary host memory while allowing a
 * selected page to be converted into a linear indexed framebuffer for SDL.
 */

#ifndef WOLF3D_VGA_MEMORY_H
#define WOLF3D_VGA_MEMORY_H

#include "indexed_framebuffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_VGA_PLANE_COUNT 4

/* Original Wolfenstein VGA memory layout from WL_DEF.H and ID_VL.H. */
#define WOLF_VGA_LEGACY_PLANE_SIZE 65536u
#define WOLF_VGA_LEGACY_STRIDE_BYTES 80
#define WOLF_VGA_LEGACY_SCANLINES 208
#define WOLF_VGA_LEGACY_PAGE_SIZE \
    (WOLF_VGA_LEGACY_STRIDE_BYTES * WOLF_VGA_LEGACY_SCANLINES)
#define WOLF_VGA_LEGACY_PAGE1_OFFSET 0u
#define WOLF_VGA_LEGACY_PAGE2_OFFSET WOLF_VGA_LEGACY_PAGE_SIZE
#define WOLF_VGA_LEGACY_PAGE3_OFFSET (WOLF_VGA_LEGACY_PAGE_SIZE * 2u)
#define WOLF_VGA_LEGACY_LATCH_OFFSET (WOLF_VGA_LEGACY_PAGE_SIZE * 3u)

typedef struct WolfVgaMemory {
    uint8_t *planes[WOLF_VGA_PLANE_COUNT];
    size_t plane_size;
    int stride_bytes;
    int height;
} WolfVgaMemory;

/* Bind the original 64 KiB VGA planes and 80-byte, 208-line page layout. */
bool wolf_vga_memory_init_legacy(
    WolfVgaMemory *memory,
    uint8_t *storage,
    size_t storage_size);

/* Bind four equally sized planes to caller-owned storage. */
bool wolf_vga_memory_init(
    WolfVgaMemory *memory,
    uint8_t *storage,
    size_t storage_size,
    size_t plane_size,
    int stride_bytes,
    int height);

void wolf_vga_memory_clear(WolfVgaMemory *memory, uint8_t colour);

void wolf_vga_memory_plot(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    uint8_t colour);

void wolf_vga_memory_hline(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    int width,
    uint8_t colour);

void wolf_vga_memory_vline(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    int height,
    uint8_t colour);

void wolf_vga_memory_fill_rect(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    int width,
    int height,
    uint8_t colour);

/*
 * Copy planar byte rows from one VGA page offset to another. Width is in
 * bytes per plane, matching the original VL_ScreenToScreen contract.
 */
/* Copy a munged four-plane image into the selected page. X must be 4-aligned. */
bool wolf_vga_memory_mem_to_screen(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    int width,
    int height,
    const uint8_t *source,
    size_t source_size);

/* Copy a munged four-plane image into all four latch planes at dest_offset. */
bool wolf_vga_memory_mem_to_latch(
    WolfVgaMemory *memory,
    size_t dest_offset,
    int width,
    int height,
    const uint8_t *source,
    size_t source_size);

/* Draw a rectangle from the four latch planes into a page. X must be 4-aligned. */
/* Copy a rectangle between offsets in the planar VGA address space. */
bool wolf_vga_memory_screen_to_screen(
    WolfVgaMemory *memory,
    size_t source_offset,
    size_t destination_offset,
    int width_bytes,
    int height);

bool wolf_vga_memory_latch_to_screen(
    WolfVgaMemory *memory,
    size_t source_offset,
    size_t page_offset,
    int x,
    int y,
    int width_bytes,
    int height);

/*
 * Scale a source column vertically and repeat it across a run of adjacent
 * destination columns. The source is centred in the view rectangle exactly
 * as the original compiled VGA scaler centred wall posts.
 */
bool wolf_vga_memory_scale_post(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int view_y,
    int view_height,
    int post_width,
    const uint8_t *source,
    size_t source_size,
    int source_height,
    int destination_height);

/* Convert a selected VGA page into a caller-owned linear indexed framebuffer. */
bool wolf_vga_memory_present(
    const WolfVgaMemory *memory,
    size_t display_offset,
    WolfFramebuffer *destination);

#endif
