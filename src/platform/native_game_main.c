/* Native SDL3 host for the original Wolfenstein 3-D game. */
#include "native_compat.h"
#include "platform.h"
#include "legacy_vl.h"
#include <stdio.h>

void WolfMain(void);

static bool present_frame(const uint8_t *pixels, int pitch,
    const uint8_t palette[256][3], void *userdata)
{
    return wolf_platform_present_indexed((WolfPlatform *)userdata, pixels, pitch, palette);
}

static void delay_frame(uint32_t milliseconds, void *userdata)
{
    (void)userdata;
    wolf_platform_delay_ms(milliseconds);
}

int main(int argc, char **argv)
{
    WolfPlatformConfig config = { "Wolfenstein 3D", 960, 600, 320, 200 };
    WolfPlatform *platform;
    static WolfLegacyVideo video;
    static uint8_t vga_storage[WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT];
    static uint8_t framebuffer[320u * 200u];

    _argc = argc;
    _argv = argv;
    platform = wolf_platform_create(&config);
    if (platform == NULL) {
        fputs("Unable to initialise the SDL3 game window.\n", stderr);
        return 1;
    }
    if (!wolf_legacy_video_init(&video, vga_storage, sizeof(vga_storage),
            framebuffer, sizeof(framebuffer), 320u)) {
        fputs("Unable to initialise the software VGA framebuffer.\n", stderr);
        wolf_platform_destroy(platform);
        return 1;
    }
    wolf_legacy_video_set_present_callback(&video, present_frame, platform);
    wolf_legacy_video_set_delay_callback(&video, delay_frame, platform);
    VL_Bind(&video);

    WolfMain();

    VL_Shutdown();
    wolf_platform_destroy(platform);
    return 0;
}
