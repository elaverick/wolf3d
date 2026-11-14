#include "native_huffman.h"

#define HUFFMAN_ROOT_NODE 254u
#define HUFFMAN_FIRST_NODE_VALUE 256u

bool wolf_native_huffman_expand(
    const uint8_t *source,
    size_t source_size,
    uint8_t *destination,
    size_t destination_size,
    const WolfNativeHuffmanNode *table,
    size_t table_node_count)
{
    size_t source_index = 0;
    size_t destination_index = 0;
    unsigned bit_index = 0;
    unsigned node_index = HUFFMAN_ROOT_NODE;

    if ((source == NULL && source_size != 0) ||
        (destination == NULL && destination_size != 0) ||
        table == NULL ||
        table_node_count < WOLF_NATIVE_HUFFMAN_NODE_COUNT) {
        return false;
    }

    while (destination_index < destination_size) {
        uint16_t encoded;
        uint8_t mask;

        if (bit_index == 8u) {
            ++source_index;
            bit_index = 0;
        }

        if (source_index >= source_size ||
            node_index >= WOLF_NATIVE_HUFFMAN_NODE_COUNT) {
            return false;
        }

        mask = (uint8_t)(1u << bit_index);
        encoded = (source[source_index] & mask) != 0
            ? table[node_index].bit1
            : table[node_index].bit0;
        ++bit_index;

        if (encoded < HUFFMAN_FIRST_NODE_VALUE) {
            destination[destination_index++] = (uint8_t)encoded;
            node_index = HUFFMAN_ROOT_NODE;
        } else {
            node_index = (unsigned)encoded - HUFFMAN_FIRST_NODE_VALUE;
            if (node_index >= WOLF_NATIVE_HUFFMAN_NODE_COUNT) {
                return false;
            }
        }
    }

    return true;
}
