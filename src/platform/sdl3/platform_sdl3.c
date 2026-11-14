/*
 * SDL3 implementation of the platform services.
 *
 * This is deliberately a small adapter. It owns the SDL window and event
 * pump, but does not contain game logic or rendering rules.
 */

#include "platform.h"

#include <SDL3/SDL.h>

#include <stdlib.h>

struct WolfPlatform {
    SDL_Window *window;
};

WolfPlatform *wolf_platform_create(const WolfPlatformConfig *config)
{
    WolfPlatform *platform;
    const char *title = "Wolfenstein 3D";
    int width = 640;
    int height = 400;

    if (config != NULL) {
        if (config->window_title != NULL) {
            title = config->window_title;
        }

        if (config->window_width > 0) {
            width = config->window_width;
        }

        if (config->window_height > 0) {
            height = config->window_height;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER)) {
        return NULL;
    }

    platform = calloc(1, sizeof(*platform));
    if (platform == NULL) {
        SDL_Quit();
        return NULL;
    }

    platform->window = SDL_CreateWindow(title, width, height, 0);
    if (platform->window == NULL) {
        free(platform);
        SDL_Quit();
        return NULL;
    }

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

uint64_t wolf_platform_time_ms(void)
{
    return SDL_GetTicks();
}

void wolf_platform_destroy(WolfPlatform *platform)
{
    if (platform == NULL) {
        return;
    }

    if (platform->window != NULL) {
        SDL_DestroyWindow(platform->window);
    }

    free(platform);
    SDL_Quit();
}
