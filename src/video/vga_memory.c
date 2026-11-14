#include "vga_memory.h"

#include <limits.h>
#include <string.h>

static bool vga_memory_is_valid(const WolfVgaMemory *memory)
{
    int plane;

    if (memory == NULL || memory->stride_bytes <= 0 || memory->height <= 0 ||
        memory->plane_size == 0) {
        return false;
    }

    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        if (memory->planes[plane] == NULL) {
            return false;
        }
    }

    return (size_t)memory->height <=
        memory->plane_size / (size_t)memory->stride_bytes;
}

bool wolf_vga_memory_init(
    WolfVgaMemory *memory,
    uint8_t *storage,
    size_t storage_size,
    size_t plane_size,
    int stride_bytes,
    int height)
{
    size_t required_plane_size;
    size_t required_storage;
    int plane;

    if (memory == NULL || storage == NULL ||
        stride_bytes <= 0 || stride_bytes > INT_MAX / 4 || height <= 0 ||
        plane_size == 0 ||
        (size_t)height > SIZE_MAX / (size_t)stride_bytes) {
        return false;
    }

    required_plane_size = (size_t)stride_bytes * (size_t)height;
    if (plane_size < required_plane_size ||
        plane_size > SIZE_MAX / WOLF_VGA_PLANE_COUNT) {
        return false;
    }

    required_storage = plane_size * WOLF_VGA_PLANE_COUNT;
    if (storage_size < required_storage) {
        return false;
    }

    memory->plane_size = plane_size;
    memory->stride_bytes = stride_bytes;
    memory->height = height;

    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        memory->planes[plane] = storage + (size_t)plane * plane_size;
    }

    return true;
}

void wolf_vga_memory_clear(WolfVgaMemory *memory, uint8_t colour)
{
    int plane;

    if (!vga_memory_is_valid(memory)) {
        return;
    }

    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        memset(memory->planes[plane], colour, memory->plane_size);
    }
}

void wolf_vga_memory_plot(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    uint8_t colour)
{
    size_t address;
    size_t row_offset;
    int plane;

    if (!vga_memory_is_valid(memory) ||
        x < 0 || y < 0 ||
        x >= memory->stride_bytes * 4 || y >= memory->height) {
        return;
    }

    row_offset = (size_t)y * (size_t)memory->stride_bytes;
    if (page_offset > memory->plane_size ||
        row_offset > memory->plane_size - page_offset) {
        return;
    }

    address = page_offset + row_offset + (size_t)x / 4;
    if (address >= memory->plane_size) {
        return;
    }

    plane = x & 3;
    memory->planes[plane][address] = colour;
}

void wolf_vga_memory_hline(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    int width,
    uint8_t colour)
{
    int64_t start;
    int64_t end;
    int64_t column;

    if (!vga_memory_is_valid(memory) || width <= 0 ||
        y < 0 || y >= memory->height) {
        return;
    }

    start = x;
    end = start + (int64_t)width;

    if (end <= 0 || start >= (int64_t)memory->stride_bytes * 4) {
        return;
    }

    if (start < 0) start = 0;
    if (end > (int64_t)memory->stride_bytes * 4) {
        end = (int64_t)memory->stride_bytes * 4;
    }

    for (column = start; column < end; ++column) {
        wolf_vga_memory_plot(
            memory, page_offset, (int)column, y, colour);
    }
}

void wolf_vga_memory_vline(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    int height,
    uint8_t colour)
{
    int64_t start;
    int64_t end;
    int64_t row;

    if (!vga_memory_is_valid(memory) || height <= 0 ||
        x < 0 || x >= memory->stride_bytes * 4) {
        return;
    }

    start = y;
    end = start + (int64_t)height;

    if (end <= 0 || start >= memory->height) {
        return;
    }

    if (start < 0) start = 0;
    if (end > memory->height) end = memory->height;

    for (row = start; row < end; ++row) {
        wolf_vga_memory_plot(memory, page_offset, x, (int)row, colour);
    }
}

void wolf_vga_memory_fill_rect(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    int width,
    int height,
    uint8_t colour)
{
    int64_t top;
    int64_t bottom;
    int64_t row;

    if (!vga_memory_is_valid(memory) || height <= 0) {
        return;
    }

    top = y;
    bottom = top + (int64_t)height;

    if (bottom <= 0 || top >= memory->height) {
        return;
    }

    if (top < 0) top = 0;
    if (bottom > memory->height) bottom = memory->height;

    for (row = top; row < bottom; ++row) {
        wolf_vga_memory_hline(
            memory, page_offset, x, (int)row, width, colour);
    }
}

static bool planar_image_size(
    int width,
    int height,
    size_t source_size,
    size_t *row_bytes,
    size_t *plane_size)
{
    size_t required_size;

    if (width <= 0 || height <= 0 || (width & 3) != 0) {
        return false;
    }

    *row_bytes = (size_t)width / 4;
    if ((size_t)height > SIZE_MAX / *row_bytes) {
        return false;
    }

    *plane_size = *row_bytes * (size_t)height;
    if (*plane_size > SIZE_MAX / WOLF_VGA_PLANE_COUNT) {
        return false;
    }

    required_size = *plane_size * WOLF_VGA_PLANE_COUNT;
    return source_size >= required_size;
}

bool wolf_vga_memory_mem_to_screen(
    WolfVgaMemory *memory,
    size_t page_offset,
    int x,
    int y,
    int width,
    int height,
    const uint8_t *source,
    size_t source_size)
{
    size_t row_bytes;
    size_t source_plane_size;
    size_t destination_start;
    size_t destination_last_row;
    int plane;
    int row;

    if (!vga_memory_is_valid(memory) || source == NULL ||
        x < 0 || y < 0 || (x & 3) != 0 ||
        x > memory->stride_bytes * 4 - width ||
        height > memory->height - y ||
        !planar_image_size(width, height, source_size,
            &row_bytes, &source_plane_size)) {
        return false;
    }

    if (page_offset > memory->plane_size) {
        return false;
    }

    {
        size_t row_offset =
            (size_t)y * (size_t)memory->stride_bytes + (size_t)x / 4;

        if (row_offset > memory->plane_size - page_offset) {
            return false;
        }

        destination_start = page_offset + row_offset;
    }

    if ((size_t)(height - 1) >
        (SIZE_MAX - destination_start) / (size_t)memory->stride_bytes) {
        return false;
    }

    destination_last_row =
        destination_start + (size_t)(height - 1) * (size_t)memory->stride_bytes;

    if (destination_last_row > memory->plane_size ||
        row_bytes > memory->plane_size - destination_last_row) {
        return false;
    }

    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        const uint8_t *source_plane =
            source + (size_t)plane * source_plane_size;

        for (row = 0; row < height; ++row) {
            uint8_t *destination_row =
                memory->planes[plane] + destination_start +
                (size_t)row * (size_t)memory->stride_bytes;

            memcpy(
                destination_row,
                source_plane + (size_t)row * row_bytes,
                row_bytes);
        }
    }

    return true;
}

bool wolf_vga_memory_mem_to_latch(
    WolfVgaMemory *memory,
    size_t dest_offset,
    int width,
    int height,
    const uint8_t *source,
    size_t source_size)
{
    size_t row_bytes;
    size_t source_plane_size;
    int plane;

    if (!vga_memory_is_valid(memory) || source == NULL ||
        !planar_image_size(width, height, source_size,
            &row_bytes, &source_plane_size) ||
        dest_offset > memory->plane_size ||
        source_plane_size > memory->plane_size - dest_offset) {
        return false;
    }

    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        memcpy(
            memory->planes[plane] + dest_offset,
            source + (size_t)plane * source_plane_size,
            source_plane_size);
    }

    return true;
}

bool wolf_vga_memory_latch_to_screen(
    WolfVgaMemory *memory,
    size_t source_offset,
    size_t page_offset,
    int x,
    int y,
    int width_bytes,
    int height)
{
    size_t source_size;
    size_t destination_start;
    size_t destination_last_row;
    int row_start = 0;
    int row_end;
    int row_step = 1;
    int row;
    int plane;

    if (!vga_memory_is_valid(memory) ||
        width_bytes <= 0 || height <= 0 ||
        x < 0 || y < 0 || (x & 3) != 0 ||
        width_bytes > memory->stride_bytes - x / 4 ||
        height > memory->height - y ||
        (size_t)height > SIZE_MAX / (size_t)width_bytes) {
        return false;
    }

    source_size = (size_t)width_bytes * (size_t)height;
    if (source_offset > memory->plane_size ||
        source_size > memory->plane_size - source_offset ||
        page_offset > memory->plane_size) {
        return false;
    }

    {
        size_t row_offset =
            (size_t)y * (size_t)memory->stride_bytes + (size_t)x / 4;

        if (row_offset > memory->plane_size - page_offset) {
            return false;
        }

        destination_start = page_offset + row_offset;
    }

    if ((size_t)(height - 1) >
        (SIZE_MAX - destination_start) / (size_t)memory->stride_bytes) {
        return false;
    }

    destination_last_row =
        destination_start + (size_t)(height - 1) * (size_t)memory->stride_bytes;

    if (destination_last_row > memory->plane_size ||
        (size_t)width_bytes > memory->plane_size - destination_last_row) {
        return false;
    }

    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        uint8_t *plane_memory = memory->planes[plane];

        row_start = 0;
        row_end = height;
        row_step = 1;

        if (destination_start > source_offset &&
            destination_start < source_offset + source_size) {
            row_start = height - 1;
            row_end = -1;
            row_step = -1;
        }

        for (row = row_start; row != row_end; row += row_step) {
            memmove(
                plane_memory + destination_start +
                    (size_t)row * (size_t)memory->stride_bytes,
                plane_memory + source_offset +
                    (size_t)row * (size_t)width_bytes,
                (size_t)width_bytes);
        }
    }

    return true;
}

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
    int destination_height)
{
    int64_t top;
    int64_t bottom;
    int64_t left;
    int64_t right;
    int64_t clipped_top;
    int64_t clipped_bottom;
    int64_t destination_y;

    if (!vga_memory_is_valid(memory) || source == NULL ||
        view_height <= 0 || post_width <= 0 ||
        source_height <= 0 || destination_height <= 0 ||
        source_size < (size_t)source_height) {
        return false;
    }

    top = (int64_t)view_y +
        ((int64_t)view_height - destination_height) / 2;
    bottom = top + destination_height;
    left = x;
    right = left + post_width;

    if (left < 0) left = 0;
    if (right > (int64_t)memory->stride_bytes * 4) {
        right = (int64_t)memory->stride_bytes * 4;
    }

    clipped_top = top;
    clipped_bottom = bottom;

    if (clipped_top < view_y) clipped_top = view_y;
    if (clipped_bottom > (int64_t)view_y + view_height) {
        clipped_bottom = (int64_t)view_y + view_height;
    }
    if (clipped_top < 0) clipped_top = 0;
    if (clipped_bottom > memory->height) {
        clipped_bottom = memory->height;
    }

    if (left >= right || clipped_top >= clipped_bottom) {
        return true;
    }

    for (destination_y = clipped_top;
         destination_y < clipped_bottom;
         ++destination_y) {
        uint64_t relative_y = (uint64_t)(destination_y - top);
        size_t source_y = (size_t)(
            ((relative_y + 1) * (uint64_t)source_height - 1) /
            (uint64_t)destination_height);
        int64_t destination_x;

        for (destination_x = left; destination_x < right; ++destination_x) {
            wolf_vga_memory_plot(
                memory,
                page_offset,
                (int)destination_x,
                (int)destination_y,
                source[source_y]);
        }
    }

    return true;
}

bool wolf_vga_memory_present(
    const WolfVgaMemory *memory,
    size_t display_offset,
    WolfFramebuffer *destination)
{
    int x;
    int y;

    if (!vga_memory_is_valid(memory) ||
        destination == NULL || destination->pixels == NULL ||
        destination->width <= 0 || destination->height <= 0 ||
        destination->pitch < (size_t)destination->width ||
        destination->width > memory->stride_bytes * 4 ||
        destination->height > memory->height ||
        display_offset > memory->plane_size) {
        return false;
    }

    for (y = 0; y < destination->height; ++y) {
        size_t row_offset;
        size_t row_delta = (size_t)y * (size_t)memory->stride_bytes;

        if (row_delta > memory->plane_size - display_offset) {
            return false;
        }
        row_offset = display_offset + row_delta;

        if ((size_t)(destination->width - 1) / 4 >=
            memory->plane_size - row_offset) {
            return false;
        }

        for (x = 0; x < destination->width; ++x) {
            int plane = x & 3;
            size_t address = row_offset + (size_t)x / 4;

            destination->pixels[(size_t)y * destination->pitch + (size_t)x] =
                memory->planes[plane][address];
        }
    }

    return true;
}
