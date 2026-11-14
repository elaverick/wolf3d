/*
 * Native replacements for the original 16-bit compiled sprite scalers.
 *
 * The sprite page format is unchanged. The rendering work is delegated to a
 * portable, bounds-checked C implementation in native_sprite_scaler.c.
 */
#include "WL_DEF.H"

#include "native_sprite_scaler.h"

#include <stddef.h>
#include <stdint.h>

static int native_max_scale = MAXSCALEHEIGHT / 2 - 1;

static void scale_native_sprite(
    int xcenter,
    int shapenum,
    unsigned projected_height,
    int destination_height,
    const unsigned *occlusion,
    bool clip_walls)
{
    unsigned page_number;
    const uint8_t *shape;
    size_t shape_size;
    WolfLegacyVideo *video;

    if (shapenum < 0 || destination_height <= 0 ||
        destination_height > MAXSCALEHEIGHT * 2) {
        return;
    }

    page_number = (unsigned)PMSpriteStart + (unsigned)shapenum;
    if (PMPages == NULL || page_number >= ChunksInFile) {
        return;
    }

    video = VL_GetVideo();
    if (video == NULL) {
        return;
    }

    shape = (const uint8_t *)PM_GetSpritePage(shapenum);
    if (shape == NULL) {
        return;
    }

    shape_size = (size_t)PMPages[page_number].length;
    (void)wolf_native_sprite_scale(
        video,
        shape,
        shape_size,
        xcenter,
        viewwidth,
        viewheight,
        destination_height,
        clip_walls ? occlusion : NULL,
        projected_height);
}

void SetupScaling(int maxscaleheight)
{
    if (maxscaleheight < 0) {
        maxscaleheight = 0;
    }
    maxscaleheight /= 2;
    native_max_scale = maxscaleheight > 0 ? maxscaleheight - 1 : 0;
    if (native_max_scale >= MAXSCALEHEIGHT / 2) {
        native_max_scale = MAXSCALEHEIGHT / 2 - 1;
    }
}

void ScaleShape(int xcenter, int shapenum, unsigned height)
{
    unsigned scale = height >> 3;

    if (scale == 0 || scale > (unsigned)native_max_scale) {
        return;
    }

    scale_native_sprite(
        xcenter,
        shapenum,
        height,
        (int)(scale * 2u),
        wallheight,
        true);
}

void SimpleScaleShape(int xcenter, int shapenum, unsigned height)
{
    unsigned scale = height >> 1;

    if (scale == 0 || scale > MAXSCALEHEIGHT) {
        return;
    }

    scale_native_sprite(
        xcenter,
        shapenum,
        height,
        (int)(scale * 2u),
        NULL,
        false);
}
