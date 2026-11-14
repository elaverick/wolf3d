#ifndef WOLF3D_NATIVE_HUFFMAN_H
#define WOLF3D_NATIVE_HUFFMAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WOLF_NATIVE_HUFFMAN_NODE_COUNT 255u

typedef struct WolfNativeHuffmanNode {
    uint16_t bit0;
    uint16_t bit1;
} WolfNativeHuffmanNode;

/*
 * Expand one Id Software Huffman bitstream.
 *
 * Input bits are consumed least-significant-bit first. Values below 256 are
 * output bytes; values 256..510 select another node in the 255-entry table.
 * Both the compressed input and expanded output lengths are bounded.
 */
bool wolf_native_huffman_expand(
    const uint8_t *source,
    size_t source_size,
    uint8_t *destination,
    size_t destination_size,
    const WolfNativeHuffmanNode *table,
    size_t table_node_count);

#endif
