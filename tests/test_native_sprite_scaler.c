#ifdef NDEBUG
#undef NDEBUG
#endif

#include "native_sprite_scaler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define VGA_STORAGE_SIZE (WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT)
#define TEST_SHAPE_SIZE 36u
#define TEST_POSTS_COLUMN_0 14u
#define TEST_POSTS_COLUMN_1 22u

static uint8_t vga_storage[VGA_STORAGE_SIZE];
static uint8_t framebuffer_pixels[320u * 200u];
static WolfLegacyVideo video;

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)(value >> 8);
}

/* Two columns, six packed pixels and two independent post lists. */
static size_t make_test_shape(uint8_t *shape, size_t capacity)
{
    assert(capacity >= TEST_SHAPE_SIZE);
    memset(shape, 0, capacity);

    write_u16_le(shape, 0);
    write_u16_le(shape + 2, 1);
    write_u16_le(shape + 4, TEST_POSTS_COLUMN_0);
    write_u16_le(shape + 6, TEST_POSTS_COLUMN_1);

    shape[8] = 11;
    shape[9] = 22;
    shape[10] = 33;
    shape[11] = 44;
    shape[12] = 55;
    shape[13] = 66;

    /* Column 0: rows [1, 4), virtual top 7 means pixel bytes at [8, 11). */
    write_u16_le(shape + TEST_POSTS_COLUMN_0, 8);
    write_u16_le(shape + TEST_POSTS_COLUMN_0 + 2, 7);
    write_u16_le(shape + TEST_POSTS_COLUMN_0 + 4, 2);
    write_u16_le(shape + TEST_POSTS_COLUMN_0 + 6, 0);

    /* Column 1: row 0, then rows [5, 7), pixel bytes at [11, 12), [12, 14). */
    write_u16_le(shape + TEST_POSTS_COLUMN_1, 2);
    write_u16_le(shape + TEST_POSTS_COLUMN_1 + 2, 11);
    write_u16_le(shape + TEST_POSTS_COLUMN_1 + 4, 0);
    write_u16_le(shape + TEST_POSTS_COLUMN_1 + 6, 14);
    write_u16_le(shape + TEST_POSTS_COLUMN_1 + 8, 7);
    write_u16_le(shape + TEST_POSTS_COLUMN_1 + 10, 10);
    write_u16_le(shape + TEST_POSTS_COLUMN_1 + 12, 0);

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

static void test_pixel_pool_and_multiple_posts(void)
{
    uint8_t shape[TEST_SHAPE_SIZE];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));
    present_video();

    assert(framebuffer_pixels[68u * 320u + 128u] == 0);
    assert(framebuffer_pixels[69u * 320u + 128u] == 11);
    assert(framebuffer_pixels[70u * 320u + 128u] == 22);
    assert(framebuffer_pixels[71u * 320u + 128u] == 33);
    assert(framebuffer_pixels[72u * 320u + 128u] == 0);

    assert(framebuffer_pixels[68u * 320u + 129u] == 44);
    assert(framebuffer_pixels[69u * 320u + 129u] == 0);
    assert(framebuffer_pixels[73u * 320u + 129u] == 55);
    assert(framebuffer_pixels[74u * 320u + 129u] == 66);
}

static void test_wall_occlusion(void)
{
    uint8_t shape[TEST_SHAPE_SIZE];
    unsigned wall_height[320];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    memset(wall_height, 0, sizeof(wall_height));
    wall_height[128] = 64;

    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64,
        wall_height, 64));
    assert(video.memory.planes[0][(size_t)69 * video.memory.stride_bytes +
        128u / 4u] == 0);

    wall_height[128] = 63;
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64,
        wall_height, 64));
    assert(video.memory.planes[0][(size_t)69 * video.memory.stride_bytes +
        128u / 4u] == 11);
}

static void test_horizontal_and_vertical_clipping(void)
{
    uint8_t shape[TEST_SHAPE_SIZE];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 64, 320, 200, 128, NULL, 128));
    assert(video.memory.planes[0][(size_t)38 * video.memory.stride_bytes] == 11);
    assert(video.memory.planes[0][(size_t)40 * video.memory.stride_bytes] == 22);
    assert(video.memory.planes[0][(size_t)42 * video.memory.stride_bytes] == 33);
}

static void test_rejects_malformed_shape_data(void)
{
    uint8_t shape[TEST_SHAPE_SIZE];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    write_u16_le(shape + TEST_POSTS_COLUMN_0, 130);
    clear_video();
    assert(!wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));

    make_test_shape(shape, sizeof(shape));
    write_u16_le(shape + TEST_POSTS_COLUMN_0 + 2, 0);
    clear_video();
    assert(!wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));

    make_test_shape(shape, sizeof(shape));
    write_u16_le(shape + TEST_POSTS_COLUMN_0 + 6, 8);
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

    test_pixel_pool_and_multiple_posts();
    test_wall_occlusion();
    test_horizontal_and_vertical_clipping();
    test_rejects_malformed_shape_data();

    puts("native sprite scaler tests passed");
    return 0;
}
