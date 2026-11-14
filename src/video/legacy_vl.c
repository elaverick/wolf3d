#include "legacy_vl.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

unsigned bufferofs;
unsigned displayofs;
unsigned pelpan;
unsigned linewidth;
unsigned ylookup[200];
bool screenfaded;
unsigned bordercolor;

static WolfLegacyVideo default_video;
static uint8_t default_vga_storage[
    WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT];
static uint8_t default_framebuffer_storage[320 * 200];
static WolfLegacyVideo *active_video;
static bool default_video_initialised;

static size_t legacy_image_size(int width, int height)
{
    if (width <= 0 || height <= 0 ||
        (size_t)width > SIZE_MAX / (size_t)height) {
        return 0;
    }

    return (size_t)width * (size_t)height;
}

static WolfLegacyVideo *get_video(void)
{
    if (active_video == NULL) {
        VL_Startup();
    }

    if (active_video != NULL) {
        wolf_legacy_video_set_draw_page(active_video, bufferofs);
        wolf_legacy_video_set_display_page(active_video, displayofs);
    }

    return active_video;
}

static void update_video_globals(void)
{
    unsigned row;

    if (active_video == NULL) {
        return;
    }

    bufferofs = (unsigned)active_video->draw_page_offset;
    displayofs = (unsigned)active_video->display_page_offset;
    linewidth = WOLF_VGA_LEGACY_STRIDE_BYTES;

    for (row = 0; row < 200; ++row) {
        ylookup[row] = row * linewidth;
    }

    screenfaded = active_video->screen_faded;
}

void VL_Bind(WolfLegacyVideo *video)
{
    active_video = video;
    update_video_globals();
}

void VL_Startup(void)
{
    if (active_video == NULL) {
        if (!default_video_initialised) {
            default_video_initialised = wolf_legacy_video_init(
                &default_video,
                default_vga_storage,
                sizeof(default_vga_storage),
                default_framebuffer_storage,
                sizeof(default_framebuffer_storage),
                320);
        }

        if (!default_video_initialised) {
            return;
        }

        active_video = &default_video;
    }

    update_video_globals();
}

void VL_Shutdown(void)
{
    /* All storage is caller-owned or static; there is no hardware to release. */
}

void VL_SetVGAPlane(void)
{
    /* The software VGA model is always addressable in four-plane mode. */
}

void VL_SetTextMode(void)
{
    /* Native builds remain in the SDL window; there is no DOS text mode. */
}

void VL_DePlaneVGA(void)
{
    /* Compatibility no-op: software planes do not have VGA register modes. */
}

void VL_SetVGAPlaneMode(void)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL) {
        wolf_legacy_video_clear(video, 0);
    }
}

void VL_ClearVideo(uint8_t colour)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL) {
        wolf_legacy_video_clear(video, colour);
    }
}

void VL_SetLineWidth(unsigned width)
{
    unsigned row;

    if (width == WOLF_VGA_LEGACY_STRIDE_BYTES / 2u) {
        linewidth = WOLF_VGA_LEGACY_STRIDE_BYTES;
        for (row = 0; row < 200; ++row) {
            ylookup[row] = row * linewidth;
        }
    }
}

void VL_SetSplitScreen(int line_number)
{
    /*
     * The original split was a VGA CRTC line-compare trick. The native renderer
     * will implement this as a normal software viewport when that UI path is
     * connected. Keep the argument consumed in this compatibility stub.
     */
    (void)line_number;
}

void VL_WaitVBL(int vbls)
{
    wolf_legacy_video_wait_vbl(get_video(), vbls);
}

void VL_CrtcStart(int crtc)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL && crtc >= 0) {
        wolf_legacy_video_set_display_page(video, (size_t)crtc);
        displayofs = (unsigned)crtc;
    }
}

void VL_SetScreen(int crtc, int horizontal_pan)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL && crtc >= 0) {
        wolf_legacy_video_set_display_page(video, (size_t)crtc);
        displayofs = (unsigned)crtc;
        pelpan = horizontal_pan < 0 ? 0u : (unsigned)horizontal_pan;
    }
}

void VL_FillPalette(int red, int green, int blue)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL) {
        wolf_legacy_video_fill_palette(video, red, green, blue);
    }
}

void VL_SetColor(int index, int red, int green, int blue)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL) {
        wolf_legacy_video_set_color(video, index, red, green, blue);
    }
}

void VL_GetColor(int index, int *red, int *green, int *blue)
{
    wolf_legacy_video_get_color(get_video(), index, red, green, blue);
}

void VL_SetPalette(const uint8_t *palette)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL) {
        wolf_legacy_video_set_palette(
            video, (const uint8_t (*)[3])palette);
    }
}

void VL_GetPalette(uint8_t *palette)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL && palette != NULL) {
        memcpy(palette, video->palette_dac, sizeof(video->palette_dac));
    }
}

void VL_FadeOut(
    int first_colour,
    int last_colour,
    int red,
    int green,
    int blue,
    int steps)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL) {
        wolf_legacy_video_fade_out(
            video, first_colour, last_colour, red, green, blue, steps);
        screenfaded = video->screen_faded;
    }
}

void VL_FadeIn(
    int first_colour,
    int last_colour,
    const uint8_t *palette,
    int steps)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL) {
        wolf_legacy_video_fade_in(
            video, first_colour, last_colour,
            (const uint8_t (*)[3])palette, steps);
        screenfaded = video->screen_faded;
    }
}

void VL_ColorBorder(int colour)
{
    bordercolor = colour < 0 ? 0u : (unsigned)colour;
}

void VL_Plot(int x, int y, int colour)
{
    wolf_legacy_video_plot(get_video(), x, y, (uint8_t)colour);
}

void VL_Hlin(unsigned x, unsigned y, unsigned width, unsigned colour)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL && x <= INT_MAX && y <= INT_MAX && width <= INT_MAX) {
        wolf_legacy_video_hline(
            video, (int)x, (int)y, (int)width, (uint8_t)colour);
    }
}

void VL_Vlin(int x, int y, int height, int colour)
{
    wolf_legacy_video_vline(
        get_video(), x, y, height, (uint8_t)colour);
}

void VL_Bar(int x, int y, int width, int height, int colour)
{
    wolf_legacy_video_fill_rect(
        get_video(), x, y, width, height, (uint8_t)colour);
}

void VL_MungePic(uint8_t *source, unsigned width, unsigned height)
{
    if (width <= INT_MAX && height <= INT_MAX) {
        if ((size_t)width <= SIZE_MAX / (size_t)height) {
            (void)wolf_legacy_video_munge_picture(
                source, (size_t)width * (size_t)height,
                (int)width, (int)height);
        }
    }
}

void VL_DrawPicBare(int x, int y, uint8_t *picture, int width, int height)
{
    VL_MemToScreen(picture, width, height, x, y);
}

void VL_MemToLatch(
    uint8_t *source, int width, int height, unsigned destination)
{
    WolfLegacyVideo *video = get_video();

    if (video != NULL) {
        (void)wolf_legacy_video_mem_to_latch(
            video, destination, width, height,
            source, legacy_image_size(width, height));
    }
}

void VL_LatchToScreen(
    unsigned source, int width_bytes, int height, int x, int y)
{
    (void)wolf_legacy_video_latch_to_screen(
        get_video(), source, x, y, width_bytes, height);
}

void VL_MemToScreen(
    uint8_t *source, int width, int height, int x, int y)
{
    (void)wolf_legacy_video_mem_to_screen(
        get_video(), x, y, width, height, source,
        legacy_image_size(width, height));
}

void VL_MaskedToScreen(
    uint8_t *source, int width, int height, int x, int y)
{
    VL_MemToScreen(source, width, height, x, y);
}

void VL_DrawTile8String(
    const char *text, const char *tile8_data, int x, int y)
{
    WolfLegacyVideo *video = get_video();

    if (video == NULL || text == NULL || tile8_data == NULL) {
        return;
    }

    while (*text != '\0') {
        unsigned character = (unsigned char)*text;
        const uint8_t *glyph =
            (const uint8_t *)tile8_data + (size_t)character * 64u;

        (void)wolf_legacy_video_mem_to_screen(
            video, x, y, 8, 8, glyph, 64);
        if (x > INT_MAX - 8) {
            break;
        }
        x += 8;
        ++text;
    }
}

void VL_DrawLatch8String(
    const char *text, unsigned tile8_offset, int x, int y)
{
    WolfLegacyVideo *video = get_video();

    if (video == NULL || text == NULL) {
        return;
    }

    while (*text != '\0') {
        size_t glyph_offset =
            (size_t)tile8_offset + ((size_t)(unsigned char)*text << 4);

        (void)wolf_legacy_video_latch_to_screen(
            video, glyph_offset, x, y, 2, 8);
        if (x > INT_MAX - 8) {
            break;
        }
        x += 8;
        ++text;
    }
}

void VL_SizeTile8String(const char *text, int *width, int *height)
{
    size_t length = text != NULL ? strlen(text) : 0;

    if (width != NULL) {
        *width = length > (size_t)INT_MAX / 8 ?
            INT_MAX : (int)(length * 8);
    }
    if (height != NULL) {
        *height = 8;
    }
}

void VL_TestPaletteSet(void)
{
    /* SDL textures consume the in-memory palette directly; no DAC probe needed. */
}

WolfLegacyVideo *VL_GetVideo(void)
{
    return get_video();
}

bool VL_Present(void)
{
    return wolf_legacy_video_present(get_video());
}

const WolfFramebuffer *VL_GetFramebuffer(void)
{
    WolfLegacyVideo *video = get_video();

    return video != NULL ? &video->framebuffer : NULL;
}

const uint8_t (*VL_GetRGBPalette(void))[3]
{
    WolfLegacyVideo *video = get_video();

    return video != NULL ?
        (const uint8_t (*)[3])video->palette_rgb : NULL;
}
