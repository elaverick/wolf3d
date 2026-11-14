#ifdef NDEBUG
#undef NDEBUG
#endif

#include "legacy_vl.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t tile_data[256 * 64];
static uint8_t glyph_planar[64];

static void test_basic_video_api(void)
{
    uint8_t palette[256][3] = {{0}};
    const WolfFramebuffer *framebuffer;
    const uint8_t (*rgb_palette)[3];
    uint8_t palette_copy[256 * 3];
    int width;
    int height;

    VL_Startup();
    VL_SetVGAPlaneMode();
    VL_ClearVideo(0);

    VL_SetLineWidth(40);
    assert(linewidth == 80);
    assert(ylookup[0] == 0);
    assert(ylookup[1] == 80);
    assert(ylookup[199] == 15920);

    VL_SetScreen((int)WOLF_VGA_LEGACY_PAGE2_OFFSET, 0);
    bufferofs = WOLF_VGA_LEGACY_PAGE2_OFFSET;
    VL_Plot(319, 199, 7);
    assert(VL_Present());

    framebuffer = VL_GetFramebuffer();
    rgb_palette = VL_GetRGBPalette();
    assert(framebuffer != NULL);
    assert(rgb_palette != NULL);
    assert(framebuffer->pixels[199 * 320 + 319] == 7);

    palette[7][0] = 63;
    palette[7][1] = 32;
    palette[7][2] = 16;
    VL_SetPalette((const uint8_t *)palette);
    assert(rgb_palette[7][0] == 255);
    assert(rgb_palette[7][1] == 130);
    assert(rgb_palette[7][2] == 65);
    VL_GetPalette(palette_copy);
    assert(palette_copy[7 * 3] == 63);
    assert(palette_copy[7 * 3 + 1] == 32);
    assert(palette_copy[7 * 3 + 2] == 16);

    VL_FadeOut(7, 7, 0, 0, 0, 1);
    assert(screenfaded);
    VL_FadeIn(7, 7, (const uint8_t *)palette, 1);
    assert(!screenfaded);

    VL_SizeTile8String("ABC", &width, &height);
    assert(width == 24);
    assert(height == 8);
}

static void test_image_and_text_api(void)
{
    const WolfFramebuffer *framebuffer;
    int x;
    int y;

    VL_SetScreen((int)WOLF_VGA_LEGACY_PAGE3_OFFSET, 0);
    bufferofs = WOLF_VGA_LEGACY_PAGE3_OFFSET;
    VL_SetLineWidth(40);
    VL_ClearVideo(0);

    for (y = 0; y < 8; ++y) {
        for (x = 0; x < 8; ++x) {
            glyph_planar[(size_t)y * 8 + (size_t)x] = (uint8_t)(x + y);
        }
    }

    assert(wolf_image_chunky_to_planar4(
        glyph_planar, sizeof(glyph_planar), 8, 8));

    VL_MemToScreen(glyph_planar, 8, 8, 0, 0);
    assert(VL_Present());
    framebuffer = VL_GetFramebuffer();
    assert(framebuffer->pixels[0] == 0);
    assert(framebuffer->pixels[7] == 7);
    assert(framebuffer->pixels[7 * 320 + 7] == 14);

    memset(tile_data + 65 * 64, 0xaa, 64);
    VL_DrawTile8String("A", (const char *)tile_data, 8, 8);
    assert(VL_Present());
    assert(framebuffer->pixels[8 * 320 + 8] == 0xaa);
    assert(framebuffer->pixels[15 * 320 + 15] == 0xaa);

    memset(glyph_planar, 0xbb, sizeof(glyph_planar));
    {
        unsigned atlas_offset = WOLF_VGA_LEGACY_LATCH_OFFSET + 256;
        unsigned glyph_offset = atlas_offset + (unsigned)'A' * 16u;

        VL_MemToLatch(glyph_planar, 8, 8, glyph_offset);
        VL_DrawLatch8String("A", atlas_offset, 16, 8);
    }
    assert(VL_Present());
    assert(framebuffer->pixels[8 * 320 + 16] == 0xbb);
    assert(framebuffer->pixels[15 * 320 + 23] == 0xbb);
}

int main(void)
{
    test_basic_video_api();
    test_image_and_text_api();

    VL_Shutdown();
    puts("legacy VL API tests passed");
    return 0;
}
