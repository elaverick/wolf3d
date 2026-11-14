#include "native_sprite_scaler.h"

#include <stdint.h>

#define SPRITE_SOURCE_SIZE 64u
#define SPRITE_COLUMN_COUNT 64u
#define SPRITE_HEADER_SIZE (4u + SPRITE_COLUMN_COUNT * sizeof(uint16_t))

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static unsigned column_slot(unsigned source_x, unsigned left_pixel)
{
    /*
     * Both halves of the original ScaleShape walk address the same table
     * order: dataofs[source_x - leftpix]. The left half decrements its
     * pointer while source x decrements; the right half starts at
     * dataofs[32 - leftpix] and increments it with source x.
     */
    return source_x - left_pixel;
}

static size_t next_column_offset(
    const uint16_t offsets[SPRITE_COLUMN_COUNT],
    unsigned slot,
    size_t shape_size)
{
    unsigned next_slot;

    for (next_slot = slot + 1u;
         next_slot < SPRITE_COLUMN_COUNT;
         ++next_slot) {
        if (offsets[next_slot] != 0) {
            return offsets[next_slot];
        }
    }

    return shape_size;
}

bool wolf_native_sprite_scale(
    WolfLegacyVideo *video,
    const uint8_t *shape,
    size_t shape_size,
    int x_center,
    int view_width,
    int view_height,
    int destination_height,
    const unsigned *wall_height,
    unsigned projected_height)
{
    unsigned left_pixel;
    unsigned right_pixel;
    uint16_t column_offsets[SPRITE_COLUMN_COUNT];
    size_t pixel_pool_end = shape_size;
    size_t previous_offset = 0;
    unsigned slot;
    int sprite_half_height;
    int top;
    unsigned source_x;

    if (video == NULL || shape == NULL ||
        shape_size < SPRITE_HEADER_SIZE ||
        view_width <= 0 || view_height <= 0 ||
        destination_height <= 0 || (destination_height & 1) != 0 ||
        view_width > video->memory.stride_bytes * 4 ||
        view_height > video->memory.height) {
        return false;
    }

    left_pixel = read_u16_le(shape);
    right_pixel = read_u16_le(shape + 2);
    if (left_pixel > right_pixel || right_pixel >= SPRITE_SOURCE_SIZE) {
        return false;
    }

    for (slot = 0; slot < SPRITE_COLUMN_COUNT; ++slot) {
        size_t offset = read_u16_le(
            shape + 4u + (size_t)slot * sizeof(uint16_t));

        if (offset != 0) {
            if (offset < SPRITE_HEADER_SIZE ||
                offset > shape_size - sizeof(uint16_t) ||
                offset <= previous_offset) {
                return false;
            }
            previous_offset = offset;
            if (offset < pixel_pool_end) {
                pixel_pool_end = offset;
            }
        }
        column_offsets[slot] = (uint16_t)offset;
    }

    if (pixel_pool_end < SPRITE_HEADER_SIZE) {
        return false;
    }

    sprite_half_height = destination_height / 2;
    top = view_height / 2 - sprite_half_height;

    for (source_x = left_pixel; source_x <= right_pixel; ++source_x) {
        unsigned column_index = column_slot(source_x, left_pixel);
        size_t column_offset;
        size_t column_end;
        int64_t left_edge;
        int64_t right_edge;
        int screen_left;
        int screen_right;
        int screen_x;
        size_t post_cursor;

        if (column_index >= SPRITE_COLUMN_COUNT) {
            return false;
        }

        column_offset = column_offsets[column_index];
        if (column_offset == 0) {
            continue;
        }

        column_end = next_column_offset(
            column_offsets, column_index, shape_size);
        if (column_end <= column_offset || column_end > shape_size) {
            return false;
        }

        left_edge = (int64_t)x_center - sprite_half_height +
            ((int64_t)source_x * destination_height) /
                (int64_t)SPRITE_SOURCE_SIZE;
        right_edge = (int64_t)x_center - sprite_half_height +
            ((int64_t)(source_x + 1u) * destination_height) /
                (int64_t)SPRITE_SOURCE_SIZE;

        if (left_edge < 0) left_edge = 0;
        if (right_edge > view_width) right_edge = view_width;
        if (left_edge >= right_edge) {
            continue;
        }
        screen_left = (int)left_edge;
        screen_right = (int)right_edge;

        post_cursor = column_offset;
        for (;;) {
            uint16_t end_word;
            uint16_t virtual_top;
            uint16_t start_word;
            unsigned end_y;
            unsigned start_y;
            size_t pixel_start;
            size_t pixel_end;
            unsigned source_y;

            if (post_cursor > column_end ||
                column_end - post_cursor < sizeof(uint16_t)) {
                return false;
            }

            end_word = read_u16_le(shape + post_cursor);
            post_cursor += sizeof(uint16_t);
            if (end_word == 0) {
                break;
            }

            if (column_end - post_cursor < 2u * sizeof(uint16_t)) {
                return false;
            }

            virtual_top = read_u16_le(shape + post_cursor);
            start_word = read_u16_le(shape + post_cursor + sizeof(uint16_t));
            post_cursor += 2u * sizeof(uint16_t);

            end_y = end_word >> 1;
            start_y = start_word >> 1;
            if (start_y >= end_y || end_y > SPRITE_SOURCE_SIZE) {
                return false;
            }

            /*
             * ScaleLine sets SI to the post's virtual-top word and generated
             * code reads [SI + source_y]. The post's pixel bytes are therefore
             * addressed by virtual_top + source_y, not by a compact run index.
             */
            pixel_start = (size_t)virtual_top + start_y;
            pixel_end = (size_t)virtual_top + end_y;
            if (pixel_start < SPRITE_HEADER_SIZE ||
                pixel_end > pixel_pool_end) {
                return false;
            }

            for (source_y = start_y; source_y < end_y; ++source_y) {
                int64_t scaled_start =
                    ((int64_t)source_y * destination_height) /
                    (int64_t)SPRITE_SOURCE_SIZE;
                int64_t scaled_end =
                    ((int64_t)(source_y + 1u) * destination_height) /
                    (int64_t)SPRITE_SOURCE_SIZE;
                int screen_top = top + (int)scaled_start;
                int screen_bottom = top + (int)scaled_end;
                int screen_y;
                uint8_t colour = shape[(size_t)virtual_top + source_y];

                if (screen_top < 0) screen_top = 0;
                if (screen_bottom > view_height) {
                    screen_bottom = view_height;
                }
                if (screen_top >= screen_bottom) {
                    continue;
                }

                for (screen_x = screen_left;
                     screen_x < screen_right;
                     ++screen_x) {
                    if (wall_height != NULL &&
                        wall_height[screen_x] >= projected_height) {
                        continue;
                    }

                    for (screen_y = screen_top;
                         screen_y < screen_bottom;
                         ++screen_y) {
                        wolf_legacy_video_plot(
                            video, screen_x, screen_y, colour);
                    }
                }
            }
        }
    }

    return true;
}
