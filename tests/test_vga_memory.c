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

    puts("software VGA memory tests passed");
    return 0;
}
