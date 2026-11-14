#include "native_sprite_scaler.h"

#include <stdint.h>

#define SPRITE_SOURCE_SIZE 64u
#define SPRITE_FIXED_HEADER_SIZE 4u
#define SPRITE_COLUMN_ENTRY_SIZE 2u

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
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
    size_t column_count;
    size_t header_size;
    size_t pixel_pool_end;
    size_t column_index;
    int sprite_half_height;
    int top;

    if (video == NULL || shape == NULL ||
        shape_size < SPRITE_FIXED_HEADER_SIZE ||
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

    column_count = (size_t)(right_pixel - left_pixel) + 1u;
    if (column_count > (SIZE_MAX - SPRITE_FIXED_HEADER_SIZE) /
            SPRITE_COLUMN_ENTRY_SIZE) {
        return false;
    }

    header_size = SPRITE_FIXED_HEADER_SIZE +
        column_count * SPRITE_COLUMN_ENTRY_SIZE;
    if (header_size > shape_size) {
        return false;
    }

    /* The first column's post offset marks the end of the packed pixel pool. */
    pixel_pool_end = shape_size;
    for (column_index = 0; column_index < column_count; ++column_index) {
        size_t offset = read_u16_le(
            shape + SPRITE_FIXED_HEADER_SIZE +
            column_index * SPRITE_COLUMN_ENTRY_SIZE);

        if (offset < header_size || offset > shape_size - sizeof(uint16_t)) {
            return false;
        }
        if (column_index > 0) {
            size_t previous = read_u16_le(
                shape + SPRITE_FIXED_HEADER_SIZE +
                (column_index - 1u) * SPRITE_COLUMN_ENTRY_SIZE);
            if (offset <= previous) {
                return false;
            }
        }
        if (offset < pixel_pool_end) {
            pixel_pool_end = offset;
        }
    }

    if (pixel_pool_end < header_size) {
        return false;
    }

    sprite_half_height = destination_height / 2;
    top = view_height / 2 - sprite_half_height;

    for (column_index = 0; column_index < column_count; ++column_index) {
        size_t column_offset = read_u16_le(
            shape + SPRITE_FIXED_HEADER_SIZE +
            column_index * SPRITE_COLUMN_ENTRY_SIZE);
        size_t column_end = column_index + 1u < column_count
            ? (size_t)read_u16_le(
                shape + SPRITE_FIXED_HEADER_SIZE +
                (column_index + 1u) * SPRITE_COLUMN_ENTRY_SIZE)
            : shape_size;
        unsigned source_x = left_pixel + (unsigned)column_index;
        int64_t left_edge;
        int64_t right_edge;
        int screen_left;
        int screen_right;
        int screen_x;
        size_t post_cursor = column_offset;

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
             * The middle post word is the virtual source-line base, not the
             * first pixel address. WL_SCALE.C loads [SI+source_y], with SI
             * taken directly from this field.
             */
            pixel_start = (size_t)virtual_top + start_y;
            pixel_end = (size_t)virtual_top + end_y;
            if (pixel_start < header_size || pixel_end > pixel_pool_end) {
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
