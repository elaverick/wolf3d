/*
 * Portable replacement declarations for the original ID_VL video manager.
 *
 * Native engine builds should include this header instead of the DOS/VGA
 * declarations in WOLFSRC/ID_VL.H. The implementation is backed by
 * WolfLegacyVideo and contains no SDL types.
 */

#ifndef WOLF3D_LEGACY_VL_H
#define WOLF3D_LEGACY_VL_H

#include "legacy_video.h"

#include <stdbool.h>
#include <stdint.h>

extern unsigned bufferofs;
extern unsigned displayofs;
extern unsigned pelpan;
extern unsigned linewidth;
extern unsigned screenseg;
extern unsigned ylookup[200];
extern bool screenfaded;
extern unsigned bordercolor;

void VL_Bind(WolfLegacyVideo *video);
void VL_Startup(void);
void VL_Shutdown(void);
void VL_SetVGAPlane(void);
void VL_SetTextMode(void);
void VL_DePlaneVGA(void);
void VL_SetVGAPlaneMode(void);
void VL_ClearVideo(uint8_t colour);
void VL_SetLineWidth(unsigned width);
void VL_SetSplitScreen(int line_number);
void VL_WaitVBL(int vbls);
void VL_CrtcStart(int crtc);
void VL_SetCRTC(int crtc);
void VL_SetScreen(int crtc, int horizontal_pan);

void VL_FillPalette(int red, int green, int blue);
void VL_SetColor(int index, int red, int green, int blue);
void VL_GetColor(int index, int *red, int *green, int *blue);
void VL_SetPalette(const uint8_t *palette);
void VL_GetPalette(uint8_t *palette);
void VL_FadeOut(
    int first_colour,
    int last_colour,
    int red,
    int green,
    int blue,
    int steps);
void VL_FadeIn(
    int first_colour,
    int last_colour,
    const uint8_t *palette,
    int steps);
void VL_ColorBorder(int colour);

void VL_Plot(int x, int y, int colour);
void VL_Hlin(unsigned x, unsigned y, unsigned width, unsigned colour);
void VL_Vlin(int x, int y, int height, int colour);
void VL_Bar(int x, int y, int width, int height, int colour);

void VL_MungePic(uint8_t *source, unsigned width, unsigned height);
void VL_DrawPicBare(int x, int y, uint8_t *picture, int width, int height);
void VL_MemToLatch(uint8_t *source, int width, int height, unsigned destination);
void VL_ScreenToScreen(unsigned source, unsigned destination, int width, int height);
void VL_LatchToScreen(
    unsigned source, int width_bytes, int height, int x, int y);
void VL_MemToScreen(
    uint8_t *source, int width, int height, int x, int y);
void VL_MaskedToScreen(
    uint8_t *source, int width, int height, int x, int y);
void VL_DrawTile8String(
    const char *text, const char *tile8_data, int x, int y);
void VL_DrawLatch8String(
    const char *text, unsigned tile8_offset, int x, int y);
void VL_SizeTile8String(const char *text, int *width, int *height);
void VL_TestPaletteSet(void);

/* Native presentation accessors; the host submits these to wolf_platform. */
WolfLegacyVideo *VL_GetVideo(void);
bool VL_Present(void);
const WolfFramebuffer *VL_GetFramebuffer(void);
const uint8_t (*VL_GetRGBPalette(void))[3];

#endif
