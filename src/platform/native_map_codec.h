#ifndef WOLF3D_NATIVE_MAP_CODEC_H
#define WOLF3D_NATIVE_MAP_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Decoders for the original Wolfenstein 3-D word-oriented map compression.
 * All sizes are explicit so corrupt input cannot read or write beyond a
 * caller-owned buffer.
 */
bool wolf_native_carmack_expand(
    const uint8_t *source,
    size_t source_size,
    uint16_t *destination,
    size_t destination_size);

bool wolf_native_rlew_expand(
    const uint16_t *source,
    size_t source_words,
    unsigned *destination,
    size_t destination_words,
    uint16_t rlew_tag);

/*
 * Decode one complete map plane.
 *
 * Carmack-compressed planes begin with the byte count of the expanded
 * Carmack stream. That stream begins with the RLEW output byte count.
 * Non-Carmack planes begin directly with the RLEW output byte count.
 * The RLEW size must exactly match destination_words 16-bit map cells.
 */
bool wolf_native_map_decode_plane(
    const uint8_t *source,
    size_t source_size,
    unsigned *destination,
    size_t destination_words,
    uint16_t rlew_tag,
    bool carmack_compressed);

#endif
