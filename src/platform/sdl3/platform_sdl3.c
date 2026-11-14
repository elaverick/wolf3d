/*
 * SDL3 implementation of the platform services.
 *
 * This adapter owns the SDL window, renderer and presentation texture. It
 * deliberately knows nothing about Wolfenstein's VGA registers or renderer.
 */

#include "platform.h"

#include <SDL3/SDL.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

struct WolfPlatform {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame_texture;
    int framebuffer_width;
    int framebuffer_height;
    uint32_t *rgba_pixels;
};

static void destroy_video_resources(WolfPlatform *platform)
{
    if (platform->frame_texture != NULL) {
        SDL_DestroyTexture(platform->frame_texture);
    }

    if (platform->renderer != NULL) {
        SDL_DestroyRenderer(platform->renderer);
    }

    if (platform->window != NULL) {
        SDL_DestroyWindow(platform->window);
    }
}

WolfPlatform *wolf_platform_create(const WolfPlatformConfig *config)
{
    WolfPlatform *platform;
    const char *title = "Wolfenstein 3D";
    int window_width = 960;
    int window_height = 600;
    int framebuffer_width = 320;
    int framebuffer_height = 200;

    if (config != NULL) {
        if (config->window_title != NULL) {
            title = config->window_title;
        }

        if (config->window_width > 0) {
            window_width = config->window_width;
        }

        if (config->window_height > 0) {
            window_height = config->window_height;
        }

        if (config->framebuffer_width > 0) {
            framebuffer_width = config->framebuffer_width;
        }

        if (config->framebuffer_height > 0) {
            framebuffer_height = config->framebuffer_height;
        }
    }

    if (framebuffer_width > INT_MAX / (int)sizeof(uint32_t) ||
        (size_t)framebuffer_width >
            SIZE_MAX / (size_t)framebuffer_height / sizeof(uint32_t)) {
        return NULL;
    }

    platform = calloc(1, sizeof(*platform));
    if (platform == NULL) {
        return NULL;
    }

    platform->framebuffer_width = framebuffer_width;
    platform->framebuffer_height = framebuffer_height;
    platform->rgba_pixels = calloc(
        (size_t)framebuffer_width * (size_t)framebuffer_height,
        sizeof(*platform->rgba_pixels));

    if (platform->rgba_pixels == NULL) {
        free(platform);
        return NULL;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        free(platform->rgba_pixels);
        free(platform);
        return NULL;
    }

    platform->window = SDL_CreateWindow(
        title, window_width, window_height, SDL_WINDOW_RESIZABLE);

    if (platform->window == NULL) {
        wolf_platform_destroy(platform);
        return NULL;
    }

    platform->renderer = SDL_CreateRenderer(platform->window, NULL);
    if (platform->renderer == NULL) {
        wolf_platform_destroy(platform);
        return NULL;
    }

    platform->frame_texture = SDL_CreateTexture(
        platform->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        framebuffer_width,
        framebuffer_height);

    if (platform->frame_texture == NULL) {
        wolf_platform_destroy(platform);
        return NULL;
    }

    SDL_SetTextureScaleMode(platform->frame_texture, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderLogicalPresentation(
        platform->renderer,
        framebuffer_width,
        framebuffer_height,
        SDL_LOGICAL_PRESENTATION_LETTERBOX);

    return platform;
}

bool wolf_platform_pump_events(WolfPlatform *platform)
{
    SDL_Event event;

    if (platform == NULL) {
        return false;
    }

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }
    }

    return true;
}

bool wolf_platform_present_indexed(
    WolfPlatform *platform,
    const uint8_t *pixels,
    int pitch,
    const uint8_t palette[256][3])
{
    int x;
    int y;

    if (platform == NULL || pixels == NULL || palette == NULL ||
        pitch < platform->framebuffer_width) {
        return false;
    }

    for (y = 0; y < platform->framebuffer_height; ++y) {
        const uint8_t *source = pixels + (size_t)y * (size_t)pitch;
        uint32_t *destination =
            platform->rgba_pixels + (size_t)y * (size_t)platform->framebuffer_width;

        for (x = 0; x < platform->framebuffer_width; ++x) {
            const uint8_t *colour = palette[source[x]];

            destination[x] =
                UINT32_C(0xff000000) |
                ((uint32_t)colour[0] << 16) |
                ((uint32_t)colour[1] << 8) |
                (uint32_t)colour[2];
        }
    }

    if (!SDL_UpdateTexture(
            platform->frame_texture,
            NULL,
            platform->rgba_pixels,
            platform->framebuffer_width * (int)sizeof(*platform->rgba_pixels))) {
        return false;
    }

    if (!SDL_RenderClear(platform->renderer)) {
        return false;
    }

    if (!SDL_RenderTexture(platform->renderer, platform->frame_texture, NULL, NULL)) {
        return false;
    }

    if (!SDL_RenderPresent(platform->renderer)) {
        return false;
    }

    return true;
}

uint64_t wolf_platform_time_ms(void)
{
    return SDL_GetTicks();
}

void wolf_platform_delay_ms(uint32_t milliseconds)
{
    SDL_Delay(milliseconds);
}

void wolf_platform_destroy(WolfPlatform *platform)
{
    if (platform == NULL) {
        return;
    }

    destroy_video_resources(platform);
    free(platform->rgba_pixels);
    free(platform);
    SDL_Quit();
}
