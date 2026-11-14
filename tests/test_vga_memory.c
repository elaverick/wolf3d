#ifdef NDEBUG
#undef NDEBUG
#endif

#include "vga_memory.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STRIDE 4
#define WIDTH (STRIDE * 4)
#define HEIGHT 4
#define PLANE_SIZE 64
#define STORAGE (PLANE_SIZE * WOLF_VGA_PLANE_COUNT)
#define PAGE_OFFSET 32

static uint8_t storage[STORAGE];
static uint8_t output_pixels[20 * HEIGHT];

static void test_initialisation(void)
{
    WolfVgaMemory memory;

    assert(wolf_vga_memory_init(
        &memory, storage, sizeof(storage), PLANE_SIZE, STRIDE, HEIGHT));
    assert(!wolf_vga_memory_init(
        &memory, storage, sizeof(storage) - 1, PLANE_SIZE, STRIDE, HEIGHT));
    assert(!wolf_vga_memory_init(
        &memory, storage, sizeof(storage), PLANE_SIZE, 0, HEIGHT));
    assert(!wolf_vga_memory_init(
        NULL, storage, sizeof(storage), PLANE_SIZE, STRIDE, HEIGHT));
}

static void test_planar_pages_and_primitives(void)
{
    WolfVgaMemory memory;
    WolfFramebuffer output;
    uint8_t planar[8] = {0, 4, 1, 5, 2, 6, 3, 7};

    memset(storage, 0, sizeof(storage));
    memset(output_pixels, 0xee, sizeof(output_pixels));

    assert(wolf_vga_memory_init(
        &memory, storage, sizeof(storage), PLANE_SIZE, STRIDE, HEIGHT));
    assert(wolf_framebuffer_init(
        &output, WIDTH, HEIGHT, 20, output_pixels, sizeof(output_pixels)));

    wolf_vga_memory_clear(&memory, 0);

    wolf_vga_memory_plot(&memory, PAGE_OFFSET, 0, 0, 10);
    wolf_vga_memory_hline(&memory, PAGE_OFFSET, 1, 1, 6, 20);
    wolf_vga_memory_vline(&memory, PAGE_OFFSET, 3, 0, 4, 30);
    wolf_vga_memory_fill_rect(&memory, PAGE_OFFSET, 4, 1, 4, 2, 40);

    assert(wolf_vga_memory_present(&memory, PAGE_OFFSET, &output));
    assert(output_pixels[0] == 10);
    assert(output_pixels[1 * 20 + 1] == 20);
    assert(output_pixels[1 * 20 + 2] == 20);
    assert(output_pixels[0 * 20 + 3] == 30);
    assert(output_pixels[1 * 20 + 3] == 30);
    assert(output_pixels[1 * 20 + 4] == 40);
    assert(output_pixels[2 * 20 + 7] == 40);
    assert(output_pixels[3 * 20 + 7] == 0);

    assert(wolf_vga_memory_mem_to_screen(
        &memory, PAGE_OFFSET, 0, 0, 4, 2, planar, sizeof(planar)));
    assert(wolf_vga_memory_present(&memory, PAGE_OFFSET, &output));
    assert(output_pixels[0] == 0);
    assert(output_pixels[1] == 1);
    assert(output_pixels[2] == 2);
    assert(output_pixels[3] == 3);
    assert(output_pixels[20] == 4);
    assert(output_pixels[23] == 7);

    assert(!wolf_vga_memory_mem_to_screen(
        &memory, PAGE_OFFSET, 1, 0, 4, 2, planar, sizeof(planar)));
    assert(!wolf_vga_memory_mem_to_screen(
        &memory, PAGE_OFFSET, 0, 0, 4, 2, planar, sizeof(planar) - 1));
}

static void test_screen_to_screen_overlap(void)
{
    WolfVgaMemory memory;
    int plane;
    int row;

    memset(storage, 0, sizeof(storage));
    assert(wolf_vga_memory_init(
        &memory, storage, sizeof(storage), PLANE_SIZE, STRIDE, HEIGHT));

    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        for (row = 0; row < HEIGHT; ++row) {
            memset(
                memory.planes[plane] + (size_t)row * STRIDE,
                (int)(plane * 16 + row),
                STRIDE);
        }
    }

    /* Downward overlapping copy must proceed from the bottom row upward. */
    assert(wolf_vga_memory_screen_to_screen(&memory, 0, STRIDE, 4, 3));
    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        assert(memory.planes[plane][STRIDE] == (uint8_t)(plane * 16));
        assert(memory.planes[plane][STRIDE * 2] == (uint8_t)(plane * 16 + 1));
        assert(memory.planes[plane][STRIDE * 3] == (uint8_t)(plane * 16 + 2));
    }

    /* Upward overlapping copy must proceed from the top row downward. */
    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        for (row = 0; row < HEIGHT; ++row) {
            memset(
                memory.planes[plane] + (size_t)row * STRIDE,
                (int)(plane * 16 + row),
                STRIDE);
        }
    }

    assert(wolf_vga_memory_screen_to_screen(
        &memory, STRIDE, 0, 4, 3));
    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        assert(memory.planes[plane][0] == (uint8_t)(plane * 16 + 1));
        assert(memory.planes[plane][STRIDE] == (uint8_t)(plane * 16 + 2));
        assert(memory.planes[plane][STRIDE * 2] == (uint8_t)(plane * 16 + 3));
    }

    /* Horizontal overlap within one row is handled by memmove. */
    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        for (row = 0; row < STRIDE; ++row) {
            memory.planes[plane][row] = (uint8_t)row;
        }
    }

    assert(wolf_vga_memory_screen_to_screen(&memory, 0, 1, 3, 1));
    for (plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        assert(memory.planes[plane][1] == 0);
        assert(memory.planes[plane][2] == 1);
        assert(memory.planes[plane][3] == 2);
    }

    assert(!wolf_vga_memory_screen_to_screen(
        &memory, PLANE_SIZE, 0, 1, 1));
    assert(!wolf_vga_memory_screen_to_screen(&memory, 0, 0, 5, 1));
}

static void test_scaler_matches_original_fixed_point(void)
{
    static uint8_t scale_storage[
        128 * WOLF_VGA_PLANE_COUNT];
    static uint8_t scale_pixels[16 * 32];
    uint8_t source[64];
    uint8_t expected[24];
    WolfVgaMemory memory;
    WolfFramebuffer output;
    int destination_height;
    int source_y;
    int y;

    for (source_y = 0; source_y < 64; ++source_y) {
        source[source_y] = (uint8_t)(source_y + 1);
    }

    assert(wolf_vga_memory_init(
        &memory, scale_storage, sizeof(scale_storage), 128, 4, 32));
    assert(wolf_framebuffer_init(
        &output, 16, 32, 16, scale_pixels, sizeof(scale_pixels)));

    for (destination_height = 2;
         destination_height <= 24;
         destination_height += 2) {
        uint64_t step = ((uint64_t)destination_height << 16) / 64;
        uint64_t fraction = 0;
        int top = (24 - destination_height) / 2;

        memset(expected, 0, sizeof(expected));
        wolf_vga_memory_clear(&memory, 0);

        assert(wolf_vga_memory_scale_post(
            &memory, 0, 4, 0, 24, 1,
            source, sizeof(source), 64, destination_height));
        assert(wolf_vga_memory_present(&memory, 0, &output));

        for (source_y = 0; source_y < 64; ++source_y) {
            int start = (int)(fraction >> 16);
            int end;

            fraction += step;
            end = (int)(fraction >> 16);

            for (y = start; y < end && y < 24; ++y) {
                expected[top + y] = source[source_y];
            }
        }

        for (y = 0; y < 24; ++y) {
            assert(scale_pixels[(size_t)y * 16 + 4] == expected[y]);
        }
    }
}

static void test_original_vga_layout(void)
{
    static uint8_t legacy_storage[
        WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT];
    static uint8_t legacy_output[320 * 200];
    WolfVgaMemory memory;
    WolfFramebuffer output;

    assert(WOLF_VGA_LEGACY_PAGE_SIZE == 16640);
    assert(WOLF_VGA_LEGACY_PAGE2_OFFSET == 16640);
    assert(WOLF_VGA_LEGACY_PAGE3_OFFSET == 33280);
    assert(WOLF_VGA_LEGACY_LATCH_OFFSET == 49920);

    memset(legacy_storage, 0, sizeof(legacy_storage));
    assert(wolf_vga_memory_init_legacy(
        &memory, legacy_storage, sizeof(legacy_storage)));
    assert(wolf_framebuffer_init(
        &output, 320, 200, 320, legacy_output, sizeof(legacy_output)));

    wolf_vga_memory_plot(
        &memory, WOLF_VGA_LEGACY_PAGE2_OFFSET, 319, 199, 0x7a);
    assert(wolf_vga_memory_present(
        &memory, WOLF_VGA_LEGACY_PAGE2_OFFSET, &output));
    assert(legacy_output[199 * 320 + 319] == 0x7a);

    assert(wolf_vga_memory_mem_to_latch(
        &memory, WOLF_VGA_LEGACY_LATCH_OFFSET, 4, 2,
        (const uint8_t[]){0, 4, 1, 5, 2, 6, 3, 7}, 8));
    assert(legacy_storage[WOLF_VGA_LEGACY_LATCH_OFFSET] == 0);
    assert(legacy_storage[WOLF_VGA_LEGACY_PLANE_SIZE +
        WOLF_VGA_LEGACY_LATCH_OFFSET] == 1);
}

static void test_wall_post_scaling(void)
{
    WolfVgaMemory memory;
    WolfFramebuffer output;
    uint8_t source[4] = {10, 20, 30, 40};

    memset(storage, 0, sizeof(storage));
    memset(output_pixels, 0xee, sizeof(output_pixels));

    assert(wolf_vga_memory_init(
        &memory, storage, sizeof(storage), PLANE_SIZE, STRIDE, HEIGHT));
    assert(wolf_framebuffer_init(
        &output, WIDTH, HEIGHT, 20, output_pixels, sizeof(output_pixels)));

    assert(wolf_vga_memory_scale_post(
        &memory, PAGE_OFFSET, 4, 0, 4, 2,
        source, sizeof(source), 4, 2));
    assert(wolf_vga_memory_present(&memory, PAGE_OFFSET, &output));

    assert(output_pixels[4] == 0);
    assert(output_pixels[20 + 4] == 20);
    assert(output_pixels[20 + 5] == 20);
    assert(output_pixels[40 + 4] == 40);
    assert(output_pixels[40 + 5] == 40);
    assert(output_pixels[60 + 4] == 0);

    wolf_vga_memory_clear(&memory, 0);
    assert(wolf_vga_memory_scale_post(
        &memory, PAGE_OFFSET, 4, 0, 4, 2,
        source, sizeof(source), 4, 8));
    assert(wolf_vga_memory_present(&memory, PAGE_OFFSET, &output));

    assert(output_pixels[4] == 20);
    assert(output_pixels[20 + 4] == 20);
    assert(output_pixels[40 + 4] == 30);
    assert(output_pixels[60 + 4] == 30);

    assert(!wolf_vga_memory_scale_post(
        &memory, PAGE_OFFSET, 4, 0, 4, 2,
        source, sizeof(source) - 1, 4, 8));
}

static void test_overlapping_page_copy(void)
{
    WolfVgaMemory memory;

    memset(storage, 0, sizeof(storage));
    assert(wolf_vga_memory_init(
        &memory, storage, sizeof(storage), PLANE_SIZE, STRIDE, HEIGHT));

    for (int plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        for (int row = 0; row < HEIGHT; ++row) {
            for (int column = 0; column < STRIDE; ++column) {
                memory.planes[plane][(size_t)row * STRIDE + column] =
                    (uint8_t)(plane * 40 + row * 4 + column);
            }
        }
    }

    assert(wolf_vga_memory_copy_page(&memory, 0, 1, 2, 3));
    for (int plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        assert(memory.planes[plane][1] == (uint8_t)(plane * 40));
        assert(memory.planes[plane][2] == (uint8_t)(plane * 40 + 1));
        assert(memory.planes[plane][STRIDE + 1] ==
            (uint8_t)(plane * 40 + 4));
    }

    assert(wolf_vga_memory_copy_page(&memory, 1, 0, 2, 3));
    for (int plane = 0; plane < WOLF_VGA_PLANE_COUNT; ++plane) {
        assert(memory.planes[plane][0] == (uint8_t)(plane * 40));
        assert(memory.planes[plane][1] == (uint8_t)(plane * 40 + 1));
        assert(memory.planes[plane][STRIDE] == (uint8_t)(plane * 40 + 4));
    }

    assert(!wolf_vga_memory_copy_page(&memory, PLANE_SIZE, 0, 2, 2));
    assert(!wolf_vga_memory_copy_page(&memory, 0, 0, STRIDE + 1, 2));
}

static void test_latch_memory(void)
{
    WolfVgaMemory memory;
    WolfFramebuffer output;
    uint8_t planar[8] = {0, 4, 1, 5, 2, 6, 3, 7};

    memset(storage, 0, sizeof(storage));
    memset(output_pixels, 0xee, sizeof(output_pixels));

    assert(wolf_vga_memory_init(
        &memory, storage, sizeof(storage), PLANE_SIZE, STRIDE, HEIGHT));
    assert(wolf_framebuffer_init(
        &output, WIDTH, HEIGHT, 20, output_pixels, sizeof(output_pixels)));

    assert(wolf_vga_memory_mem_to_latch(
        &memory, 0, 4, 2, planar, sizeof(planar)));
    assert(wolf_vga_memory_latch_to_screen(
        &memory, 0, PAGE_OFFSET, 4, 0, 1, 2));
    assert(wolf_vga_memory_present(&memory, PAGE_OFFSET, &output));

    assert(output_pixels[4] == 0);
    assert(output_pixels[5] == 1);
    assert(output_pixels[6] == 2);
    assert(output_pixels[7] == 3);
    assert(output_pixels[20 + 4] == 4);
    assert(output_pixels[20 + 7] == 7);

    assert(!wolf_vga_memory_latch_to_screen(
        &memory, PLANE_SIZE, PAGE_OFFSET, 4, 0, 1, 2));
}

int main(void)
{
    test_initialisation();
    test_planar_pages_and_primitives();
    test_latch_memory();
    test_wall_post_scaling();
    test_original_vga_layout();
    test_scaler_matches_original_fixed_point();
    test_screen_to_screen_overlap();

    puts("software VGA memory tests passed");
    return 0;
}
