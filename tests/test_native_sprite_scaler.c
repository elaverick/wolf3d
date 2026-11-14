#include "native_sprite_scaler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SHAPE_HEADER_SIZE (4u + 64u * 2u)
#define VGA_STORAGE_SIZE (WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT)

static uint8_t vga_storage[VGA_STORAGE_SIZE];
static uint8_t framebuffer_pixels[320u * 200u];
static WolfLegacyVideo video;

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)(value >> 8);
}

static size_t make_test_shape(uint8_t *shape, size_t capacity)
{
    const size_t column = SHAPE_HEADER_SIZE;
    const size_t pixels = 256;

    assert(capacity >= pixels + 64u);
    memset(shape, 0, capacity);

    write_u16_le(shape, 0);
    write_u16_le(shape + 2, 0);
    write_u16_le(shape + 4, (uint16_t)column);

    /* One opaque post covering source rows [1, 4). */
    write_u16_le(shape + column, 8);
    write_u16_le(shape + column + 2, (uint16_t)pixels);
    write_u16_le(shape + column + 4, 2);
    write_u16_le(shape + column + 6, 0);

    shape[pixels + 1] = 11;
    shape[pixels + 2] = 22;
    shape[pixels + 3] = 33;
    return pixels + 64u;
}

static void clear_video(void)
{
    wolf_legacy_video_clear(&video, 0);
}

static void test_one_to_one_sprite_post(void)
{
    uint8_t shape[320];
    WolfFramebuffer output;
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));
    assert(wolf_framebuffer_init(
        &output, 320, 200, 320,
        framebuffer_pixels, sizeof(framebuffer_pixels)));
    assert(wolf_vga_memory_present(
        &video.memory, video.draw_page_offset, &output));

    assert(framebuffer_pixels[68u * 320u + 128u] == 0);
    assert(framebuffer_pixels[69u * 320u + 128u] == 11);
    assert(framebuffer_pixels[70u * 320u + 128u] == 22);
    assert(framebuffer_pixels[71u * 320u + 128u] == 33);
    assert(framebuffer_pixels[72u * 320u + 128u] == 0);
}

static void test_wall_occlusion(void)
{
    uint8_t shape[320];
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
    uint8_t shape[320];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    clear_video();
    assert(wolf_native_sprite_scale(
        &video, shape, shape_size, 64, 320, 200, 128, NULL, 128));
    assert(video.memory.planes[0][(size_t)38 * video.memory.stride_bytes] == 11);
    assert(video.memory.planes[0][(size_t)39 * video.memory.stride_bytes] == 11);
}

static void test_rejects_malformed_shape_data(void)
{
    uint8_t shape[320];
    size_t shape_size = make_test_shape(shape, sizeof(shape));

    write_u16_le(shape + SHAPE_HEADER_SIZE, 130);
    clear_video();
    assert(!wolf_native_sprite_scale(
        &video, shape, shape_size, 160, 320, 200, 64, NULL, 64));

    write_u16_le(shape + SHAPE_HEADER_SIZE, 8);
    write_u16_le(shape + SHAPE_HEADER_SIZE + 2, 319);
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

    test_one_to_one_sprite_post();
    test_wall_occlusion();
    test_horizontal_and_vertical_clipping();
    test_rejects_malformed_shape_data();

    puts("native sprite scaler tests passed");
    return 0;
}
