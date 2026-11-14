#ifdef NDEBUG
#undef NDEBUG
#endif

#include "native_map_codec.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)(value >> 8);
}

static void test_carmack_literals_near_far_and_escape(void)
{
    const uint8_t near_data[] = {
        0x34, 0x12,       /* literal 0x1234 */
        0x02, 0xa7, 0x01  /* copy the preceding word twice */
    };
    const uint8_t far_data[] = {
        0x78, 0x56,       /* literal 0x5678 */
        0x02, 0xa8, 0x00, 0x00 /* copy from absolute word offset zero */
    };
    const uint8_t escaped[] = {
        0x00, 0xa7, 0x5a /* literal 0xa75a */
    };
    uint16_t output[3] = {0};
    uint16_t one_word[1] = {0};

    assert(wolf_native_carmack_expand(
        near_data, sizeof(near_data), output, sizeof(output)));
    assert(output[0] == 0x1234);
    assert(output[1] == 0x1234);
    assert(output[2] == 0x1234);

    assert(wolf_native_carmack_expand(
        far_data, sizeof(far_data), output, sizeof(output)));
    assert(output[0] == 0x5678);
    assert(output[1] == 0x5678);
    assert(output[2] == 0x5678);

    assert(wolf_native_carmack_expand(
        escaped, sizeof(escaped), one_word, sizeof(one_word)));
    assert(one_word[0] == 0xa75a);
}

static void test_carmack_rejects_malformed_streams(void)
{
    const uint8_t truncated_literal[] = {0x34};
    const uint8_t truncated_near[] = {0x01, 0xa7};
    const uint8_t zero_near_offset[] = {
        0x34, 0x12, 0x01, 0xa7, 0x00
    };
    const uint8_t invalid_near_offset[] = {
        0x34, 0x12, 0x01, 0xa7, 0x02
    };
    const uint8_t oversized_run[] = {
        0x34, 0x12, 0x02, 0xa7, 0x01
    };
    const uint8_t trailing_data[] = {
        0x34, 0x12, 0x99, 0x99
    };
    uint16_t output[3] = {0};

    assert(!wolf_native_carmack_expand(
        truncated_literal, sizeof(truncated_literal), output, 2));
    assert(!wolf_native_carmack_expand(
        truncated_near, sizeof(truncated_near), output, 2));
    assert(!wolf_native_carmack_expand(
        zero_near_offset, sizeof(zero_near_offset), output, 4));
    assert(!wolf_native_carmack_expand(
        invalid_near_offset, sizeof(invalid_near_offset), output, 4));
    assert(!wolf_native_carmack_expand(
        oversized_run, sizeof(oversized_run), output, 4));
    assert(!wolf_native_carmack_expand(
        trailing_data, sizeof(trailing_data), output, 2));
}

static void test_rlew_literals_and_runs(void)
{
    const uint16_t source[] = {
        1, 2, 0xabcd, 3, 7
    };
    unsigned output[5] = {0};

    assert(wolf_native_rlew_expand(
        source,
        sizeof(source) / sizeof(source[0]),
        output,
        sizeof(output) / sizeof(output[0]),
        0xabcd));

    assert(output[0] == 1);
    assert(output[1] == 2);
    assert(output[2] == 7);
    assert(output[3] == 7);
    assert(output[4] == 7);
}

static void test_rlew_rejects_malformed_streams(void)
{
    const uint16_t truncated_run[] = {0xabcd, 2};
    const uint16_t zero_run[] = {0xabcd, 0, 7};
    const uint16_t oversized_run[] = {0xabcd, 4, 7};
    const uint16_t insufficient_literals[] = {1, 2};
    const uint16_t trailing_word[] = {1, 2, 3};
    unsigned output[3] = {0};

    assert(!wolf_native_rlew_expand(
        truncated_run, 2, output, 2, 0xabcd));
    assert(!wolf_native_rlew_expand(
        zero_run, 3, output, 2, 0xabcd));
    assert(!wolf_native_rlew_expand(
        oversized_run, 3, output, 3, 0xabcd));
    assert(!wolf_native_rlew_expand(
        insufficient_literals, 2, output, 3, 0xabcd));
    assert(!wolf_native_rlew_expand(
        trailing_word, 3, output, 2, 0xabcd));
}

static void test_complete_carmack_and_rlew_plane_formats(void)
{
    /*
     * The decoded map plane is 1, 2, 7, 7.
     * Its RLEW representation is [size=8, 1, 2, tag, run=2, value=7].
     */
    const uint16_t encoded_words[] = {
        8, 1, 2, 0xabcd, 2, 7
    };
    uint8_t carmack_plane[sizeof(uint16_t) + sizeof(encoded_words)];
    uint8_t rlew_plane[sizeof(encoded_words)];
    unsigned output[4] = {0};
    size_t index;

    write_u16_le(carmack_plane, (uint16_t)sizeof(encoded_words));
    for (index = 0;
         index < sizeof(encoded_words) / sizeof(encoded_words[0]);
         ++index) {
        write_u16_le(
            carmack_plane + sizeof(uint16_t) + index * sizeof(uint16_t),
            encoded_words[index]);
        write_u16_le(
            rlew_plane + index * sizeof(uint16_t),
            encoded_words[index]);
    }

    assert(wolf_native_map_decode_plane(
        carmack_plane, sizeof(carmack_plane),
        output, 4, 0xabcd, true));
    assert(output[0] == 1 && output[1] == 2 &&
        output[2] == 7 && output[3] == 7);

    memset(output, 0, sizeof(output));
    assert(wolf_native_map_decode_plane(
        rlew_plane, sizeof(rlew_plane),
        output, 4, 0xabcd, false));
    assert(output[0] == 1 && output[1] == 2 &&
        output[2] == 7 && output[3] == 7);
}

static void test_complete_plane_rejects_bad_size_and_truncation(void)
{
    const uint8_t wrong_rlew_size[] = {6, 0, 1, 0, 2, 0};
    const uint8_t truncated_carmack[] = {8, 0, 1, 0};
    unsigned output[4] = {0};

    assert(!wolf_native_map_decode_plane(
        wrong_rlew_size, sizeof(wrong_rlew_size),
        output, 4, 0xabcd, false));
    assert(!wolf_native_map_decode_plane(
        wrong_rlew_size, 1, output, 4, 0xabcd, false));
    assert(!wolf_native_map_decode_plane(
        truncated_carmack, sizeof(truncated_carmack),
        output, 4, 0xabcd, true));
}

int main(void)
{
    test_carmack_literals_near_far_and_escape();
    test_carmack_rejects_malformed_streams();
    test_rlew_literals_and_runs();
    test_rlew_rejects_malformed_streams();
    test_complete_carmack_and_rlew_plane_formats();
    test_complete_plane_rejects_bad_size_and_truncation();

    puts("native map codec tests passed");
    return 0;
}
