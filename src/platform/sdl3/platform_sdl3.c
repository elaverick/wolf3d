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
#include <string.h>

struct WolfPlatform {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *frame_texture;
    SDL_AudioStream *audio_stream;
    int framebuffer_width;
    int framebuffer_height;
    uint32_t *rgba_pixels;
    int mouse_x;
    int mouse_y;
    int mouse_buttons;
    float mouse_delta_x;
    float mouse_delta_y;
    bool key_down[128];
    bool pending_key_event;
    int pending_key_scan;
    int pending_ascii;
    bool quit_requested;
};

static WolfPlatform *active_platform;

static int platform_scan_from_sdl(SDL_Scancode scancode)
{
    switch (scancode) {
        case SDL_SCANCODE_ESCAPE: return 0x01;
        case SDL_SCANCODE_1: return 0x02;
        case SDL_SCANCODE_2: return 0x03;
        case SDL_SCANCODE_3: return 0x04;
        case SDL_SCANCODE_4: return 0x05;
        case SDL_SCANCODE_5: return 0x06;
        case SDL_SCANCODE_6: return 0x07;
        case SDL_SCANCODE_7: return 0x08;
        case SDL_SCANCODE_8: return 0x09;
        case SDL_SCANCODE_9: return 0x0a;
        case SDL_SCANCODE_0: return 0x0b;
        case SDL_SCANCODE_MINUS: return 0x0c;
        case SDL_SCANCODE_EQUALS: return 0x0d;
        case SDL_SCANCODE_BACKSPACE: return 0x0e;
        case SDL_SCANCODE_TAB: return 0x0f;
        case SDL_SCANCODE_Q: return 0x10;
        case SDL_SCANCODE_W: return 0x11;
        case SDL_SCANCODE_E: return 0x12;
        case SDL_SCANCODE_R: return 0x13;
        case SDL_SCANCODE_T: return 0x14;
        case SDL_SCANCODE_Y: return 0x15;
        case SDL_SCANCODE_U: return 0x16;
        case SDL_SCANCODE_I: return 0x17;
        case SDL_SCANCODE_O: return 0x18;
        case SDL_SCANCODE_P: return 0x19;
        case SDL_SCANCODE_LEFTBRACKET: return 0x1a;
        case SDL_SCANCODE_RIGHTBRACKET: return 0x1b;
        case SDL_SCANCODE_RETURN: return 0x1c;
        case SDL_SCANCODE_LCTRL: return 0x1d;
        case SDL_SCANCODE_A: return 0x1e;
        case SDL_SCANCODE_S: return 0x1f;
        case SDL_SCANCODE_D: return 0x20;
        case SDL_SCANCODE_F: return 0x21;
        case SDL_SCANCODE_G: return 0x22;
        case SDL_SCANCODE_H: return 0x23;
        case SDL_SCANCODE_J: return 0x24;
        case SDL_SCANCODE_K: return 0x25;
        case SDL_SCANCODE_L: return 0x26;
        case SDL_SCANCODE_SEMICOLON: return 0x27;
        case SDL_SCANCODE_APOSTROPHE: return 0x28;
        case SDL_SCANCODE_GRAVE: return 0x29;
        case SDL_SCANCODE_LSHIFT: return 0x2a;
        case SDL_SCANCODE_BACKSLASH: return 0x2b;
        case SDL_SCANCODE_Z: return 0x2c;
        case SDL_SCANCODE_X: return 0x2d;
        case SDL_SCANCODE_C: return 0x2e;
        case SDL_SCANCODE_V: return 0x2f;
        case SDL_SCANCODE_B: return 0x30;
        case SDL_SCANCODE_N: return 0x31;
        case SDL_SCANCODE_M: return 0x32;
        case SDL_SCANCODE_COMMA: return 0x33;
        case SDL_SCANCODE_PERIOD: return 0x34;
        case SDL_SCANCODE_SLASH: return 0x35;
        case SDL_SCANCODE_RSHIFT: return 0x36;
        case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
        case SDL_SCANCODE_LALT: return 0x38;
        case SDL_SCANCODE_SPACE: return 0x39;
        case SDL_SCANCODE_CAPSLOCK: return 0x3a;
        case SDL_SCANCODE_F1: return 0x3b;
        case SDL_SCANCODE_F2: return 0x3c;
        case SDL_SCANCODE_F3: return 0x3d;
        case SDL_SCANCODE_F4: return 0x3e;
        case SDL_SCANCODE_F5: return 0x3f;
        case SDL_SCANCODE_F6: return 0x40;
        case SDL_SCANCODE_F7: return 0x41;
        case SDL_SCANCODE_F8: return 0x42;
        case SDL_SCANCODE_F9: return 0x43;
        case SDL_SCANCODE_F10: return 0x44;
        case SDL_SCANCODE_NUMLOCKCLEAR: return 0x45;
        case SDL_SCANCODE_SCROLLLOCK: return 0x46;
        case SDL_SCANCODE_HOME: return 0x47;
        case SDL_SCANCODE_UP: return 0x48;
        case SDL_SCANCODE_PAGEUP: return 0x49;
        case SDL_SCANCODE_KP_MINUS: return 0x4a;
        case SDL_SCANCODE_LEFT: return 0x4b;
        case SDL_SCANCODE_KP_5: return 0x4c;
        case SDL_SCANCODE_RIGHT: return 0x4d;
        case SDL_SCANCODE_KP_PLUS: return 0x4e;
        case SDL_SCANCODE_END: return 0x4f;
        case SDL_SCANCODE_DOWN: return 0x50;
        case SDL_SCANCODE_PAGEDOWN: return 0x51;
        case SDL_SCANCODE_INSERT: return 0x52;
        case SDL_SCANCODE_DELETE: return 0x53;
        case SDL_SCANCODE_F11: return 0x57;
        case SDL_SCANCODE_F12: return 0x59;
        case SDL_SCANCODE_PAUSE: return 0x7f;
        case SDL_SCANCODE_RCTRL: return 0x1d;
        case SDL_SCANCODE_RALT: return 0x38;
        default: return 0;
    }
}

static float platform_mouse_scale(
    WolfPlatform *platform, int *window_width, int *window_height)
{
    float scale_x;
    float scale_y;

    SDL_GetWindowSize(platform->window, window_width, window_height);
    scale_x = (float)*window_width / (float)platform->framebuffer_width;
    scale_y = (float)*window_height / (float)platform->framebuffer_height;
    return scale_x < scale_y ? scale_x : scale_y;
}

static void update_mouse_position(
    WolfPlatform *platform, float window_x, float window_y)
{
    int window_width;
    int window_height;
    float scale = platform_mouse_scale(
        platform, &window_width, &window_height);
    float offset_x = ((float)window_width -
        (float)platform->framebuffer_width * scale) * 0.5f;
    float offset_y = ((float)window_height -
        (float)platform->framebuffer_height * scale) * 0.5f;
    float x;
    float y;

    if (scale <= 0.0f) {
        return;
    }

    x = (window_x - offset_x) / scale;
    y = (window_y - offset_y) / scale;

    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x >= (float)platform->framebuffer_width) {
        x = (float)(platform->framebuffer_width - 1);
    }
    if (y >= (float)platform->framebuffer_height) {
        y = (float)(platform->framebuffer_height - 1);
    }

    platform->mouse_x = (int)x;
    platform->mouse_y = (int)y;
}

static void destroy_video_resources(WolfPlatform *platform)
{
    if (platform->audio_stream != NULL) {
        SDL_DestroyAudioStream(platform->audio_stream);
        platform->audio_stream = NULL;
    }

    if (platform->frame_texture != NULL) {
        SDL_DestroyTexture(platform->frame_texture);
    }

    if (platform->renderer != NULL) {
        SDL_DestroyRenderer(platform->renderer);
    }

    if (platform->window != NULL) {
        SDL_StopTextInput(platform->window);
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

    /*
     * Audio is optional: a missing playback device must not prevent the
     * original game from starting. SDL owns device lifetime through stream.
     */
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_AudioSpec audio_spec = { SDL_AUDIO_S16, 1, 22050 };

        platform->audio_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, NULL, NULL);
        if (platform->audio_stream != NULL &&
            !SDL_ResumeAudioStreamDevice(platform->audio_stream)) {
            SDL_DestroyAudioStream(platform->audio_stream);
            platform->audio_stream = NULL;
        }
    }

    platform->window = SDL_CreateWindow(
        title, window_width, window_height, SDL_WINDOW_RESIZABLE);

    if (platform->window == NULL) {
        wolf_platform_destroy(platform);
        return NULL;
    }

    SDL_StartTextInput(platform->window);
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

    active_platform = platform;
    return platform;
}

bool wolf_platform_pump_events(WolfPlatform *platform)
{
    SDL_Event event;

    if (platform == NULL) {
        platform = active_platform;
    }
    if (platform == NULL) {
        return false;
    }

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            platform->quit_requested = true;
            break;
        }

        if (event.type == SDL_EVENT_KEY_DOWN ||
            event.type == SDL_EVENT_KEY_UP) {
            int scan_code = platform_scan_from_sdl(event.key.scancode);

            if (scan_code > 0 && scan_code < 128) {
                platform->key_down[scan_code] =
                    event.type == SDL_EVENT_KEY_DOWN;
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    platform->pending_key_event = true;
                    platform->pending_key_scan = scan_code;
                }
            }
        } else if (event.type == SDL_EVENT_TEXT_INPUT) {
            unsigned char ascii = (unsigned char)event.text.text[0];

            if (ascii > 0 && ascii < 128) {
                platform->pending_ascii = ascii;
            }
        } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            int window_width;
            int window_height;
            float scale = platform_mouse_scale(
                platform, &window_width, &window_height);

            update_mouse_position(
                platform, event.motion.x, event.motion.y);
            if (scale > 0.0f) {
                platform->mouse_delta_x += event.motion.xrel / scale;
                platform->mouse_delta_y += event.motion.yrel / scale;
            }
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                   event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            int mask = 0;

            if (event.button.button == SDL_BUTTON_LEFT) mask = 1;
            if (event.button.button == SDL_BUTTON_RIGHT) mask = 2;
            if (event.button.button == SDL_BUTTON_MIDDLE) mask = 4;

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                platform->mouse_buttons |= mask;
            } else {
                platform->mouse_buttons &= ~mask;
            }
            update_mouse_position(
                platform, event.button.x, event.button.y);
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            platform->mouse_buttons = 0;
            platform->mouse_delta_x = 0.0f;
            platform->mouse_delta_y = 0.0f;
            memset(platform->key_down, 0, sizeof(platform->key_down));
        }
    }

    return !platform->quit_requested;
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

bool wolf_platform_key_down(int scan_code)
{
    return active_platform != NULL &&
        scan_code >= 0 && scan_code < 128 &&
        active_platform->key_down[scan_code];
}

bool wolf_platform_take_key_event(int *scan_code)
{
    if (active_platform == NULL || !active_platform->pending_key_event) {
        return false;
    }

    if (scan_code != NULL) {
        *scan_code = active_platform->pending_key_scan;
    }

    active_platform->pending_key_event = false;
    return true;
}

int wolf_platform_take_ascii(void)
{
    int ascii;

    if (active_platform == NULL) {
        return 0;
    }

    ascii = active_platform->pending_ascii;
    active_platform->pending_ascii = 0;
    return ascii;
}

void wolf_platform_mouse_command(
    int function,
    int *ax,
    int *bx,
    int *cx,
    int *dx)
{
    WolfPlatform *platform = active_platform;
    int input_cx = cx != NULL ? *cx : 0;
    int input_dx = dx != NULL ? *dx : 0;

    if (ax != NULL) *ax = 0;
    if (bx != NULL) *bx = 0;
    if (cx != NULL) *cx = 0;
    if (dx != NULL) *dx = 0;

    if (platform == NULL) {
        return;
    }

    switch (function) {
        case 0:
            if (ax != NULL) *ax = 0xffff;
            if (bx != NULL) *bx = 3;
            break;

        case 3:
            if (bx != NULL) *bx = platform->mouse_buttons;
            if (cx != NULL) *cx = platform->mouse_x;
            if (dx != NULL) *dx = platform->mouse_y;
            break;

        case 4:
        {
            int window_width;
            int window_height;
            float scale = platform_mouse_scale(
                platform, &window_width, &window_height);
            float offset_x =
                ((float)window_width -
                (float)platform->framebuffer_width * scale) * 0.5f;
            float offset_y =
                ((float)window_height -
                (float)platform->framebuffer_height * scale) * 0.5f;
            int target_x = cx != NULL ? input_cx : platform->mouse_x;
            int target_y = dx != NULL ? input_dx : platform->mouse_y;

            if (target_x < 0) target_x = 0;
            if (target_y < 0) target_y = 0;
            if (target_x >= platform->framebuffer_width) {
                target_x = platform->framebuffer_width - 1;
            }
            if (target_y >= platform->framebuffer_height) {
                target_y = platform->framebuffer_height - 1;
            }

            platform->mouse_x = target_x;
            platform->mouse_y = target_y;
            if (scale > 0.0f) {
                platform->mouse_delta_x = 0.0f;
                platform->mouse_delta_y = 0.0f;
                SDL_WarpMouseInWindow(
                    platform->window,
                    offset_x + (float)target_x * scale,
                    offset_y + (float)target_y * scale);
            }
            break;
        }

        case 11:
            if (cx != NULL) *cx = (int)platform->mouse_delta_x;
            if (dx != NULL) *dx = (int)platform->mouse_delta_y;
            platform->mouse_delta_x = 0.0f;
            platform->mouse_delta_y = 0.0f;
            break;

        default:
            break;
    }
}

bool wolf_platform_audio_submit(const int16_t *samples, size_t sample_count)
{
    WolfPlatform *platform = active_platform;
    size_t byte_count;

    if (platform == NULL || platform->audio_stream == NULL ||
        (samples == NULL && sample_count != 0) ||
        sample_count > (size_t)INT_MAX / sizeof(*samples)) {
        return false;
    }

    byte_count = sample_count * sizeof(*samples);
    if (byte_count == 0) {
        return true;
    }

    return SDL_PutAudioStreamData(
        platform->audio_stream, samples, (int)byte_count);
}

void wolf_platform_audio_clear(void)
{
    if (active_platform != NULL && active_platform->audio_stream != NULL) {
        (void)SDL_ClearAudioStream(active_platform->audio_stream);
    }
}

int wolf_platform_audio_queued_bytes(void)
{
    if (active_platform == NULL || active_platform->audio_stream == NULL) {
        return -1;
    }

    return SDL_GetAudioStreamQueued(active_platform->audio_stream);
}

void wolf_platform_destroy(WolfPlatform *platform)
{
    if (platform == NULL) {
        return;
    }

    if (active_platform == platform) {
        active_platform = NULL;
    }

    destroy_video_resources(platform);
    free(platform->rgba_pixels);
    free(platform);
    SDL_Quit();
}
