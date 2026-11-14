#ifdef NDEBUG
#undef NDEBUG
#endif

#include "indexed_framebuffer.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 7
#define HEIGHT 5
#define PITCH 10
#define STORAGE (PITCH * HEIGHT)

static uint8_t pixels[STORAGE];
static uint8_t copy_pixels[STORAGE];

static void reset(void)
{
    memset(pixels, 0xee, sizeof(pixels));
}

static uint8_t pixel(int x, int y)
{
    return pixels[(size_t)y * PITCH + (size_t)x];
}

static void assert_padding_untouched(const uint8_t *buffer)
{
    int y;
    int x;

    for (y = 0; y < HEIGHT; ++y) {
        for (x = WIDTH; x < PITCH; ++x) {
            assert(buffer[(size_t)y * PITCH + (size_t)x] == 0xee);
        }
    }
}

static void test_initialisation(void)
{
    WolfFramebuffer framebuffer;

    assert(wolf_framebuffer_init(&framebuffer, WIDTH, HEIGHT, PITCH,
        pixels, sizeof(pixels)));
    assert(!wolf_framebuffer_init(&framebuffer, 0, HEIGHT, PITCH,
        pixels, sizeof(pixels)));
    assert(!wolf_framebuffer_init(&framebuffer, WIDTH, HEIGHT, WIDTH - 1,
        pixels, sizeof(pixels)));
    assert(!wolf_framebuffer_init(&framebuffer, WIDTH, HEIGHT, PITCH,
        pixels, WIDTH));
    assert(!wolf_framebuffer_init(NULL, WIDTH, HEIGHT, PITCH,
        pixels, sizeof(pixels)));
    assert(!wolf_framebuffer_init(&framebuffer, WIDTH, HEIGHT, PITCH,
        NULL, sizeof(pixels)));
}

static void test_clear_and_padding(void)
{
    WolfFramebuffer framebuffer;
    int y;
    int x;

    reset();
    assert(wolf_framebuffer_init(&framebuffer, WIDTH, HEIGHT, PITCH,
        pixels, sizeof(pixels)));
    wolf_framebuffer_clear(&framebuffer, 0x42);

    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; ++x) {
            assert(pixel(x, y) == 0x42);
        }
    }
    assert_padding_untouched(pixels);
}

static void test_pixel_and_lines(void)
{
    WolfFramebuffer framebuffer;
    int x;
    int y;

    reset();
    assert(wolf_framebuffer_init(&framebuffer, WIDTH, HEIGHT, PITCH,
        pixels, sizeof(pixels)));

    wolf_framebuffer_put_pixel(&framebuffer, 3, 2, 0x11);
    wolf_framebuffer_put_pixel(&framebuffer, -1, 0, 0x22);
    wolf_framebuffer_put_pixel(&framebuffer, WIDTH, 0, 0x22);
    assert(pixel(3, 2) == 0x11);

    wolf_framebuffer_hline(&framebuffer, -2, 1, 5, 0x33);
    for (x = 0; x < 3; ++x) assert(pixel(x, 1) == 0x33);
    assert(pixel(3, 1) == 0xee);

    wolf_framebuffer_vline(&framebuffer, 6, -2, 5, 0x44);
    for (y = 0; y < 3; ++y) assert(pixel(6, y) == 0x44);
    assert(pixel(6, 3) == 0xee);

    wolf_framebuffer_hline(&framebuffer, 0, 0, 0, 0x55);
    wolf_framebuffer_vline(&framebuffer, 0, 0, -1, 0x55);
    wolf_framebuffer_hline(&framebuffer, INT_MIN, 0, INT_MAX, 0x55);
    wolf_framebuffer_vline(&framebuffer, 0, INT_MIN, INT_MAX, 0x55);
    assert_padding_untouched(pixels);
}

static void test_rectangle_clipping(void)
{
    WolfFramebuffer framebuffer;
    int x;
    int y;

    reset();
    assert(wolf_framebuffer_init(&framebuffer, WIDTH, HEIGHT, PITCH,
        pixels, sizeof(pixels)));

    wolf_framebuffer_fill_rect(&framebuffer, -2, -1, 5, 4, 0x66);
    for (y = 0; y < 3; ++y) {
        for (x = 0; x < 3; ++x) assert(pixel(x, y) == 0x66);
        for (x = 3; x < WIDTH; ++x) assert(pixel(x, y) == 0xee);
    }

    wolf_framebuffer_fill_rect(&framebuffer, INT_MIN, INT_MIN,
        INT_MAX, INT_MAX, 0x77);
    assert_padding_untouched(pixels);
}

static void test_copy_and_overlap(void)
{
    WolfFramebuffer framebuffer;
    int x;
    int y;

    reset();
    assert(wolf_framebuffer_init(&framebuffer, WIDTH, HEIGHT, PITCH,
        pixels, sizeof(pixels)));

    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; ++x) {
            wolf_framebuffer_put_pixel(&framebuffer, x, y,
                (uint8_t)(y * 10 + x));
        }
    }

    wolf_framebuffer_copy(&framebuffer, 1, 1, &framebuffer, 0, 0, 5, 3);
    assert(pixel(1, 1) == 0);
    assert(pixel(5, 1) == 4);
    assert(pixel(1, 3) == 20);
    assert(pixel(5, 3) == 24);

    wolf_framebuffer_copy(&framebuffer, 0, 0, &framebuffer, 1, 1, 5, 3);
    assert(pixel(0, 0) == 0);
    assert(pixel(4, 0) == 4);
    assert_padding_untouched(pixels);
}

static void test_copy_clipping(void)
{
    WolfFramebuffer source;
    WolfFramebuffer destination;
    int x;
    int y;

    memset(copy_pixels, 0xee, sizeof(copy_pixels));
    reset();

    assert(wolf_framebuffer_init(&source, WIDTH, HEIGHT, PITCH,
        pixels, sizeof(pixels)));
    assert(wolf_framebuffer_init(&destination, WIDTH, HEIGHT, PITCH,
        copy_pixels, sizeof(copy_pixels)));

    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; ++x) {
            wolf_framebuffer_put_pixel(&source, x, y,
                (uint8_t)(y * 10 + x));
        }
    }

    wolf_framebuffer_copy(&destination, -1, -1, &source, 0, 0, 5, 4);
    assert(copy_pixels[0] == 11);
    assert(copy_pixels[1] == 12);
    assert(copy_pixels[PITCH] == 21);
    assert(copy_pixels[2 * PITCH + 2] == 33);
    assert(copy_pixels[2 * PITCH + 3] == 0xee);

    wolf_framebuffer_copy(&destination, 6, 4, &source, 6, 4, 5, 5);
    assert(copy_pixels[4 * PITCH + 6] == 46);
    assert_padding_untouched(copy_pixels);
}

int main(void)
{
    test_initialisation();
    test_clear_and_padding();
    test_pixel_and_lines();
    test_rectangle_clipping();
    test_copy_and_overlap();
    test_copy_clipping();

    puts("indexed framebuffer tests passed");
    return 0;
}
