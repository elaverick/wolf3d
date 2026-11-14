#include "indexed_framebuffer.h"

#include <limits.h>
#include <string.h>

static bool framebuffer_is_valid(const WolfFramebuffer *framebuffer)
{
    return framebuffer != NULL &&
        framebuffer->pixels != NULL &&
        framebuffer->width > 0 &&
        framebuffer->height > 0 &&
        framebuffer->pitch >= (size_t)framebuffer->width;
}

bool wolf_framebuffer_init(
    WolfFramebuffer *framebuffer,
    int width,
    int height,
    size_t pitch,
    uint8_t *pixels,
    size_t storage_size)
{
    size_t required_size;

    if (framebuffer == NULL || pixels == NULL ||
        width <= 0 || height <= 0 || pitch < (size_t)width) {
        return false;
    }

    if ((size_t)(height - 1) > (SIZE_MAX - (size_t)width) / pitch) {
        return false;
    }

    required_size = (size_t)(height - 1) * pitch + (size_t)width;
    if (storage_size < required_size) {
        return false;
    }

    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->pitch = pitch;
    framebuffer->pixels = pixels;
    return true;
}

void wolf_framebuffer_clear(WolfFramebuffer *framebuffer, uint8_t colour)
{
    int y;

    if (!framebuffer_is_valid(framebuffer)) {
        return;
    }

    for (y = 0; y < framebuffer->height; ++y) {
        memset(
            framebuffer->pixels + (size_t)y * framebuffer->pitch,
            colour,
            (size_t)framebuffer->width);
    }
}

void wolf_framebuffer_put_pixel(
    WolfFramebuffer *framebuffer, int x, int y, uint8_t colour)
{
    if (!framebuffer_is_valid(framebuffer) ||
        x < 0 || y < 0 ||
        x >= framebuffer->width || y >= framebuffer->height) {
        return;
    }

    framebuffer->pixels[(size_t)y * framebuffer->pitch + (size_t)x] = colour;
}

void wolf_framebuffer_hline(
    WolfFramebuffer *framebuffer, int x, int y, int width, uint8_t colour)
{
    int64_t start;
    int64_t end;
    size_t offset;

    if (!framebuffer_is_valid(framebuffer) ||
        width <= 0 || y < 0 || y >= framebuffer->height) {
        return;
    }

    start = x;
    end = start + (int64_t)width;

    if (end <= 0 || start >= framebuffer->width) {
        return;
    }

    if (start < 0) {
        start = 0;
    }
    if (end > framebuffer->width) {
        end = framebuffer->width;
    }

    offset = (size_t)y * framebuffer->pitch + (size_t)start;
    memset(framebuffer->pixels + offset, colour, (size_t)(end - start));
}

void wolf_framebuffer_vline(
    WolfFramebuffer *framebuffer, int x, int y, int height, uint8_t colour)
{
    int64_t start;
    int64_t end;
    int64_t row;

    if (!framebuffer_is_valid(framebuffer) ||
        height <= 0 || x < 0 || x >= framebuffer->width) {
        return;
    }

    start = y;
    end = start + (int64_t)height;

    if (end <= 0 || start >= framebuffer->height) {
        return;
    }

    if (start < 0) {
        start = 0;
    }
    if (end > framebuffer->height) {
        end = framebuffer->height;
    }

    for (row = start; row < end; ++row) {
        framebuffer->pixels[(size_t)row * framebuffer->pitch + (size_t)x] = colour;
    }
}

void wolf_framebuffer_fill_rect(
    WolfFramebuffer *framebuffer,
    int x,
    int y,
    int width,
    int height,
    uint8_t colour)
{
    int64_t left;
    int64_t top;
    int64_t right;
    int64_t bottom;
    int64_t row;

    if (!framebuffer_is_valid(framebuffer) || width <= 0 || height <= 0) {
        return;
    }

    left = x;
    top = y;
    right = left + (int64_t)width;
    bottom = top + (int64_t)height;

    if (right <= 0 || bottom <= 0 ||
        left >= framebuffer->width || top >= framebuffer->height) {
        return;
    }

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > framebuffer->width) right = framebuffer->width;
    if (bottom > framebuffer->height) bottom = framebuffer->height;

    for (row = top; row < bottom; ++row) {
        memset(
            framebuffer->pixels + (size_t)row * framebuffer->pitch + (size_t)left,
            colour,
            (size_t)(right - left));
    }
}

void wolf_framebuffer_copy(
    WolfFramebuffer *destination,
    int destination_x,
    int destination_y,
    const WolfFramebuffer *source,
    int source_x,
    int source_y,
    int width,
    int height)
{
    int64_t left_clip;
    int64_t top_clip;
    int64_t right_clip;
    int64_t bottom_clip;
    int64_t row;
    int row_start;
    int row_end;
    int row_step;

    if (!framebuffer_is_valid(destination) || !framebuffer_is_valid(source) ||
        width <= 0 || height <= 0) {
        return;
    }

    left_clip = 0;
    top_clip = 0;
    right_clip = width;
    bottom_clip = height;

    if (source_x < 0) left_clip = -((int64_t)source_x);
    if (source_y < 0) top_clip = -((int64_t)source_y);
    if (destination_x < 0 && -(int64_t)destination_x > left_clip)
        left_clip = -(int64_t)destination_x;
    if (destination_y < 0 && -(int64_t)destination_y > top_clip)
        top_clip = -(int64_t)destination_y;

    if ((int64_t)source_x + right_clip > source->width)
        right_clip = (int64_t)source->width - source_x;
    if ((int64_t)destination_x + right_clip > destination->width)
        right_clip = (int64_t)destination->width - destination_x;
    if ((int64_t)source_y + bottom_clip > source->height)
        bottom_clip = (int64_t)source->height - source_y;
    if ((int64_t)destination_y + bottom_clip > destination->height)
        bottom_clip = (int64_t)destination->height - destination_y;

    if (left_clip >= right_clip || top_clip >= bottom_clip) {
        return;
    }

    source_x += (int)left_clip;
    source_y += (int)top_clip;
    destination_x += (int)left_clip;
    destination_y += (int)top_clip;

    row_start = 0;
    row_end = (int)(bottom_clip - top_clip);
    row_step = 1;

    if (source->pixels == destination->pixels &&
        destination_y > source_y &&
        destination_y < source_y + row_end) {
        row_start = row_end - 1;
        row_end = -1;
        row_step = -1;
    }

    for (row = row_start; row != row_end; row += row_step) {
        uint8_t *destination_row =
            destination->pixels +
            (size_t)(destination_y + (int)row) * destination->pitch +
            (size_t)destination_x;
        const uint8_t *source_row =
            source->pixels +
            (size_t)(source_y + (int)row) * source->pitch +
            (size_t)source_x;

        memmove(destination_row, source_row, (size_t)(right_clip - left_clip));
    }
}
