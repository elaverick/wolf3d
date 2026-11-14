#include "native_map_codec.h"

#include <stdlib.h>

#define CARMACK_NEAR_TAG 0xa7u
#define CARMACK_FAR_TAG 0xa8u

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)(
        (uint16_t)source[0] |
        ((uint16_t)source[1] << 8));
}

bool wolf_native_carmack_expand(
    const uint8_t *source,
    size_t source_size,
    uint16_t *destination,
    size_t destination_size)
{
    size_t source_cursor = 0;
    size_t produced = 0;
    size_t destination_words;

    if (source == NULL || destination == NULL ||
        (destination_size & 1u) != 0) {
        return false;
    }

    destination_words = destination_size / sizeof(uint16_t);

    while (produced < destination_words) {
        uint16_t value;
        unsigned tag;
        unsigned count;

        if (source_cursor > source_size ||
            source_size - source_cursor < sizeof(uint16_t)) {
            return false;
        }

        value = read_u16_le(source + source_cursor);
        source_cursor += sizeof(uint16_t);
        tag = value >> 8;
        count = value & 0xffu;

        if (tag != CARMACK_NEAR_TAG && tag != CARMACK_FAR_TAG) {
            destination[produced++] = value;
            continue;
        }

        /*
         * A zero count escapes the tag: the following byte is the low byte
         * of a literal word whose high byte is the tag itself.
         */
        if (count == 0) {
            if (source_cursor >= source_size) {
                return false;
            }

            destination[produced++] =
                (uint16_t)((tag << 8) | source[source_cursor++]);
            continue;
        }

        if ((size_t)count > destination_words - produced) {
            return false;
        }

        if (tag == CARMACK_NEAR_TAG) {
            size_t offset;
            size_t copy_index;
            unsigned copied;

            if (source_cursor >= source_size) {
                return false;
            }

            offset = source[source_cursor++];
            if (offset == 0 || offset > produced) {
                return false;
            }

            copy_index = produced - offset;
            for (copied = 0; copied < count; ++copied) {
                /*
                 * The source index may enter the newly-produced range.
                 * This is intentional: Carmack near copies may overlap.
                 */
                if (copy_index >= produced) {
                    return false;
                }

                destination[produced++] = destination[copy_index++];
            }
        } else {
            size_t copy_index;
            unsigned copied;

            if (source_size - source_cursor < sizeof(uint16_t)) {
                return false;
            }

            copy_index = read_u16_le(source + source_cursor);
            source_cursor += sizeof(uint16_t);
            if (copy_index >= produced) {
                return false;
            }

            for (copied = 0; copied < count; ++copied) {
                if (copy_index >= produced) {
                    return false;
                }

                destination[produced++] = destination[copy_index++];
            }
        }
    }

    /* Reject unexpected bytes after the declared expanded word count. */
    return source_cursor == source_size;
}

bool wolf_native_rlew_expand(
    const uint16_t *source,
    size_t source_words,
    unsigned *destination,
    size_t destination_words,
    uint16_t rlew_tag)
{
    size_t source_cursor = 0;
    size_t produced = 0;

    if ((source == NULL && source_words != 0) ||
        (destination == NULL && destination_words != 0)) {
        return false;
    }

    while (produced < destination_words) {
        uint16_t value;

        if (source_cursor >= source_words) {
            return false;
        }

        value = source[source_cursor++];
        if (value != rlew_tag) {
            destination[produced++] = value;
            continue;
        }

        if (source_words - source_cursor < 2u) {
            return false;
        }

        {
            size_t run_length = source[source_cursor++];
            uint16_t run_value = source[source_cursor++];

            if (run_length == 0 ||
                run_length > destination_words - produced) {
                return false;
            }

            while (run_length-- > 0) {
                destination[produced++] = run_value;
            }
        }
    }

    /* The map plane length is exact; trailing words indicate bad metadata. */
    return source_cursor == source_words;
}

bool wolf_native_map_decode_plane(
    const uint8_t *source,
    size_t source_size,
    unsigned *destination,
    size_t destination_words,
    uint16_t rlew_tag,
    bool carmack_compressed)
{
    const uint8_t *rlew_data;
    size_t rlew_data_size;
    uint16_t expanded_size;
    uint8_t *expanded = NULL;
    bool decoded;

    if (source == NULL || destination == NULL ||
        destination_words == 0 ||
        destination_words > UINT16_MAX / sizeof(uint16_t) ||
        source_size < sizeof(uint16_t)) {
        return false;
    }

    if (carmack_compressed) {
        expanded_size = read_u16_le(source);

        if ((expanded_size & 1u) != 0 ||
            expanded_size < 2u * sizeof(uint16_t) ||
            source_size == sizeof(uint16_t)) {
            return false;
        }

        expanded = (uint8_t *)malloc(expanded_size);
        if (expanded == NULL) {
            return false;
        }

        decoded = wolf_native_carmack_expand(
            source + sizeof(uint16_t),
            source_size - sizeof(uint16_t),
            (uint16_t *)expanded,
            expanded_size);
        if (!decoded) {
            free(expanded);
            return false;
        }

        if (read_u16_le(expanded) !=
            destination_words * sizeof(uint16_t)) {
            free(expanded);
            return false;
        }

        rlew_data = expanded + sizeof(uint16_t);
        rlew_data_size = (size_t)expanded_size - sizeof(uint16_t);
    } else {
        expanded_size = read_u16_le(source);

        if (expanded_size != destination_words * sizeof(uint16_t)) {
            return false;
        }

        rlew_data = source + sizeof(uint16_t);
        rlew_data_size = source_size - sizeof(uint16_t);
    }

    if ((rlew_data_size & 1u) != 0) {
        free(expanded);
        return false;
    }

    decoded = wolf_native_rlew_expand(
        (const uint16_t *)rlew_data,
        rlew_data_size / sizeof(uint16_t),
        destination,
        destination_words,
        rlew_tag);

    free(expanded);
    return decoded;
}
