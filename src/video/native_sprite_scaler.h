/*
 * Portable C sprite scaler for the original Wolfenstein 3-D compressed
 * sprite-page format. It preserves 64x64 source geometry, transparent posts
 * and per-column wall occlusion without generated machine code.
 */
#ifndef WOLF3D_NATIVE_SPRITE_SCALER_H
#define WOLF3D_NATIVE_SPRITE_SCALER_H

#include "legacy_video.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool wolf_native_sprite_scale(
    WolfLegacyVideo *video,
    const uint8_t *shape,
    size_t shape_size,
    int x_center,
    int view_width,
    int view_height,
    int destination_height,
    const unsigned *wall_height,
    unsigned projected_height);

#endif
