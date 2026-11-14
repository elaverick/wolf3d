#ifdef NDEBUG
#undef NDEBUG
#endif

#include "native_sprite_scaler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define VGA_STORAGE_SIZE (WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT)
#define SPRITE_HEADER_SIZE (4u + 64u * 2u)
#define TEST_SHAPE_SIZE 213u
#define POSTS_SLOT_0 167u
#define POSTS_SLOT_1 175u
#define POSTS_SLOT_2 183u
#define POSTS_SLOT_3 191u
#define POSTS_SLOT_4 205u

static uint8_t vga_storage[VGA_STORAGE_SIZE];
static uint8_t framebuffer_pixels[320u * 200u];
static WolfLegacyVideo video;

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)(value >> 8);
}

/*
 * Real t_compshape layout:
 *   leftpix, rightpix, then 64 offsets in centre-out order.
 * For leftpix=30/rightpix=34 the slot sequence is x=30,31,32,33,34.
 * Pixel data occupies [132, 167); each offset points at a post command list.
 */
static size_t make_test_shape(uint8_t *shape, size_t capacity)
{
    assert(capacity >= TEST_SHAPE_SIZE);
    memset(shape, 0, capacity);

    write_u16_le(shape, 30);
    write_u16_le(shape + 2, 34);
    write_u16_le(shape + 4, POSTS_SLOT_0);
    write_u16_le(shape + 6, POSTS_SLOT_1);
    write_u16_le(shape + 8, POSTS_SLOT_2);
    write_u16_le(shape + 10, POSTS_SLOT_3);
    write_u16_le(shape + 12, POSTS_SLOT_4);

    /* Packed virtual pixel rows for each source column. */
    shape[132] = 10;
    shape[133] = 11;
    shape[134] = 22;
    shape[135] = 33;
    shape[139] = 20;
    shape[140] = 21;
    shape[141] = 22;
    shape[142] = 23;
    shape[146] = 30;
    shape[147] = 31;
    shape[148] = 32;
    shape[149] = 33;
    shape[153] = 44;
    shape[158] = 55;
    shape[159] = 66;
    shape[160] = 40;
    shape[161] = 41;
    shape[162] = 42;
    shape[163] = 43;

    /* Slot 0, source x=30, rows [1, 4). */
    write_u16_le(shape + POSTS_SLOT_0, 8);
    write_u16_le(shape + POSTS_SLOT_0 + 2, 132);
    write_u16_le(shape + POSTS_SLOT_0 + 4, 2);
    write_u16_le(shape + POSTS_SLOT_0 + 6, 0);

    /* Slot 1, source x=31, rows [1, 4). */
    write_u16_le(shape + POSTS_SLOT_1, 8);
    write_u16_le(shape + POSTS_SLOT_1 + 2, 139);
    write_u16_le(shape + POSTS_SLOT_1 + 4, 2);
    write_u16_le(shape + POSTS_SLOT_1 + 6, 0);

    /* Slot 2, source x=32, rows [1, 4). */
    write_u16_le(shape + POSTS_SLOT_2, 8);
    write_u16_le(shape + POSTS_SLOT_2 + 2, 146);
    write_u16_le(shape + POSTS_SLOT_2 + 4, 2);
    write_u16_le(shape + POSTS_SLOT_2 + 6, 0);

    /* Slot 3, source x=33, disjoint rows [0, 1) and [5, 7). */
    write_u16_le(shape + POSTS_SLOT_3, 2);
    write_u16_le(shape + POSTS_SLOT_3 + 2, 153);
    write_u16_le(shape + POSTS_SLOT_3 + 4, 0);
    write_u16_le(shape + POSTS_SLOT_3 + 6, 14);
    write_u16_le(shape + POSTS_SLOT_3 + 8, 153);
    write_u16_le(shape + POSTS_SLOT_3 + 10, 10);
    write_u16_le(shape + POSTS_SLOT_3 + 12, 0);

    /* Slot 4, source x=34, rows [1, 4). */
    write_u16_le(shape + POSTS_SLOT_4, 8);
    write_u16_le(shape + POSTS_SLOT_4 + 2, 160);
    write_u16_le(shape + POSTS_SLOT_4 + 4, 2);
    write_u16_le(shape + POSTS_SLOT_4 + 6, 0);

    return TEST_SHAPE_SIZE;
}

static void clear_video(void)
{
    wolf_legacy_video_clear(&video, 0);
}

static void present_video(void)
{
    WolfFramebuffer output;

    assert(wolf_framebuffer_init(
        &output, 320, 200, 320,
        framebuffer_pixels, sizeof(framebuffer_pixels)));
    assert(wolf_vga_memory_present(
        &video.memory, video.draw_page_offset, &output));
}

static void test_centre_out_offsets_and_multiple_posts(void)
{
    uint8_t shape[TEST_SHAPE_SIZE];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));
    present_video();

    /* x=30 maps to x=158; x=31 to 159; right half begins at x=160. */
    assert(framebuffer_pixels[69u * 320u + 158u] == 11);
    assert(framebuffer_pixels[70u * 320u + 158u] == 22);
    assert(framebuffer_pixels[71u * 320u + 158u] == 33);

    assert(framebuffer_pixels[69u * 320u + 159u] == 21);
    assert(framebuffer_pixels[70u * 320u + 159u] == 22);
    assert(framebuffer_pixels[71u * 320u + 159u] == 23);

    assert(framebuffer_pixels[69u * 320u + 160u] == 31);
    assert(framebuffer_pixels[70u * 320u + 160u] == 32);
    assert(framebuffer_pixels[71u * 320u + 160u] == 33);

    assert(framebuffer_pixels[68u * 320u + 161u] == 44);
    assert(framebuffer_pixels[69u * 320u + 161u] == 0);
    assert(framebuffer_pixels[73u * 320u + 161u] == 55);
    assert(framebuffer_pixels[74u * 320u + 161u] == 66);

    assert(framebuffer_pixels[69u * 320u + 162u] == 41);
    assert(framebuffer_pixels[70u * 320u + 162u] == 42);
    assert(framebuffer_pixels[71u * 320u + 162u] == 43);
}

static void test_wall_occlusion(void)
{
    uint8_t shape[TEST_SHAPE_SIZE];
    unsigned wall_height[320];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    memset(wall_height, 0, sizeof(wall_height));
    wall_height[159] = 64;

    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64,
        wall_height, 64));
    present_video();
    assert(framebuffer_pixels[69u * 320u + 159u] == 0);

    wall_height[159] = 63;
    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64,
        wall_height, 64));
    present_video();
    assert(framebuffer_pixels[69u * 320u + 159u] == 11);
}

static void test_horizontal_and_vertical_clipping(void)
{
    uint8_t shape[TEST_SHAPE_SIZE];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 32, 320, 200, 128, NULL, 128));
    assert(video.memory.planes[0][(size_t)38 * video.memory.stride_bytes +
        28u / 4u] == 21);
    assert(video.memory.planes[0][(size_t)40 * video.memory.stride_bytes +
        28u / 4u] == 22);
    assert(video.memory.planes[0][(size_t)42 * video.memory.stride_bytes +
        28u / 4u] == 23);
}

static void test_rejects_malformed_shape_data(void)
{
    uint8_t shape[TEST_SHAPE_SIZE];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    write_u16_le(shape + POSTS_SLOT_0, 130);
    clear_video();
    assert(!wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));

    make_test_shape(shape, sizeof(shape));
    write_u16_le(shape + POSTS_SLOT_0 + 2, 0);
    clear_video();
    assert(!wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));

    make_test_shape(shape, sizeof(shape));
    write_u16_le(shape + POSTS_SLOT_0 + 6, 2);
    clear_video();
    assert(!wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));
}

int main(void)
{
    assert(wolf_legacy_video_init(
        &video,
        vga_storage,
        sizeof(vga_storage),
        framebuffer_pixels,
        sizeof(framebuffer_pixels),
        320));

    test_centre_out_offsets_and_multiple_posts();
    test_wall_occlusion();
    test_horizontal_and_vertical_clipping();
    test_rejects_malformed_shape_data();

    puts("native sprite scaler tests passed");
    return 0;
}
