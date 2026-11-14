#ifdef NDEBUG
#undef NDEBUG
#endif

#include "native_huffman.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_literals_lsb_first(void)
{
    WolfNativeHuffmanNode table[WOLF_NATIVE_HUFFMAN_NODE_COUNT] = {{0}};
    const uint8_t compressed[] = {0x00};
    uint8_t expanded[8] = {0};

    table[254].bit0 = 'A';
    table[254].bit1 = 'B';

    assert(wolf_native_huffman_expand(
        compressed, sizeof(compressed),
        expanded, sizeof(expanded),
        table, WOLF_NATIVE_HUFFMAN_NODE_COUNT));

    assert(memcmp(expanded, "AAAAAAAA", 8) == 0);
}

static void test_node_transitions(void)
{
    WolfNativeHuffmanNode table[WOLF_NATIVE_HUFFMAN_NODE_COUNT] = {{0}};
    const uint8_t compressed[] = {0x01};
    uint8_t expanded[2] = {0};

    /*
     * First bit is one: root -> node 0, then the next bit is zero
     * and node 0 emits 'X'. The following bit is zero and root emits 'A'.
     */
    table[254].bit0 = 'A';
    table[254].bit1 = 256;
    table[0].bit0 = 'X';
    table[0].bit1 = 'Y';

    assert(wolf_native_huffman_expand(
        compressed, sizeof(compressed),
        expanded, sizeof(expanded),
        table, WOLF_NATIVE_HUFFMAN_NODE_COUNT));
    assert(expanded[0] == 'X');
    assert(expanded[1] == 'A');
}

static void test_rejects_short_or_invalid_streams(void)
{
    WolfNativeHuffmanNode table[WOLF_NATIVE_HUFFMAN_NODE_COUNT] = {{0}};
    const uint8_t one_bit[] = {0x01};
    uint8_t output[9] = {0};

    table[254].bit0 = 'A';
    table[254].bit1 = 256;
    table[0].bit0 = 'B';

    /* The second bit selects an out-of-range child node (node 255). */
    table[0].bit0 = 511;
    assert(!wolf_native_huffman_expand(
        one_bit, sizeof(one_bit), output, 2,
        table, WOLF_NATIVE_HUFFMAN_NODE_COUNT));

    /* Eight literal bits are not enough to produce the requested ninth byte. */
    table[254].bit1 = 'C';
    table[254].bit0 = 'A';
    assert(!wolf_native_huffman_expand(
        one_bit, sizeof(one_bit), output, sizeof(output),
        table, WOLF_NATIVE_HUFFMAN_NODE_COUNT));

    assert(!wolf_native_huffman_expand(
        one_bit, sizeof(one_bit), output, 1,
        table, WOLF_NATIVE_HUFFMAN_NODE_COUNT - 1u));

    assert(!wolf_native_huffman_expand(
        NULL, 0, output, 1,
        table, WOLF_NATIVE_HUFFMAN_NODE_COUNT));
}

int main(void)
{
    test_literals_lsb_first();
    test_node_transitions();
    test_rejects_short_or_invalid_streams();

    puts("native huffman tests passed");
    return 0;
}
