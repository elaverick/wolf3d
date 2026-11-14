/* Native SDL3 host for the original Wolfenstein 3-D game. */
#include "native_compat.h"
#include "platform.h"
#include "legacy_vl.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define native_change_directory _chdir
#else
#include <unistd.h>
#define native_change_directory chdir
#endif

void WolfMain(void);

static WolfPlatform *platform_at_exit;

static void shutdown_platform_at_exit(void)
{
    if (platform_at_exit != NULL) {
        wolf_platform_destroy(platform_at_exit);
        platform_at_exit = NULL;
    }
}

static bool present_frame(const uint8_t *pixels, int pitch,
    const uint8_t palette[256][3], void *userdata)
{
    /*
     * The original engine advances its simulation clock once per rendered
     * frame. Keep that clock at 70 Hz instead of letting fast PCs run the
     * simulation at an arbitrary uncapped frame rate.
     *
     * Express deadlines in 1/70 ms units so fractional frame durations are
     * carried forward rather than rounded to a fixed 14 or 15 ms delay.
     */
    static uint64_t next_frame_deadline;
    uint64_t now = wolf_platform_time_ms() * 70u;
    bool presented;

    presented = wolf_platform_present_indexed(
        (WolfPlatform *)userdata, pixels, pitch, palette);
    if (!presented) {
        return false;
    }

    if (next_frame_deadline == 0) {
        next_frame_deadline = now;
    }
    next_frame_deadline += 1000u;

    if (next_frame_deadline > now) {
        uint64_t remaining = next_frame_deadline - now;
        uint64_t delay_ms = (remaining + 69u) / 70u;
        if (delay_ms > 0 && delay_ms <= UINT32_MAX) {
            wolf_platform_delay_ms((uint32_t)delay_ms);
        }
    } else if (now - next_frame_deadline > 4000u) {
        /* Avoid long catch-up delays after a debugger break or window drag. */
        next_frame_deadline = now;
    }

    return true;
}

static void delay_frame(uint32_t milliseconds, void *userdata)
{
    (void)userdata;
    wolf_platform_delay_ms(milliseconds);
}

static void print_usage(const char *program)
{
    fprintf(stdout,
        "Usage: %s [--data-dir DIRECTORY]\n"
        "Run using original Wolfenstein 3-D game data in DIRECTORY.\n"
        "The directory must contain matching WL6, WL3, or WL1 game data files.\n",
        program);
}

static int select_data_directory(int argc, char **argv)
{
    int index;

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--help") == 0 ||
            strcmp(argv[index], "-h") == 0) {
            print_usage(argv[0]);
            return 1;
        }

        if (strcmp(argv[index], "--data-dir") == 0 ||
            strcmp(argv[index], "-d") == 0) {
            if (index + 1 >= argc || argv[index + 1][0] == '\0') {
                fprintf(stderr, "%s requires a directory path.\n", argv[index]);
                return -1;
            }
            if (native_change_directory(argv[index + 1]) != 0) {
                fprintf(stderr, "Unable to use game data directory '%s': %s\n",
                    argv[index + 1], strerror(errno));
                return -1;
            }
            ++index;
            continue;
        }

    }

    return 0;
}

int main(int argc, char **argv)
{
    WolfPlatformConfig config = { "Wolfenstein 3D", 960, 600, 320, 200 };
    WolfPlatform *platform;
    static WolfLegacyVideo video;
    static uint8_t vga_storage[WOLF_VGA_LEGACY_PLANE_SIZE * WOLF_VGA_PLANE_COUNT];
    static uint8_t framebuffer[320u * 200u];
    int argument_status;

    _argc = argc;
    _argv = argv;
    argument_status = select_data_directory(argc, argv);
    if (argument_status != 0) {
        return argument_status > 0 ? 0 : 2;
    }
    platform = wolf_platform_create(&config);
    if (platform == NULL) {
        fputs("Unable to initialise the SDL3 game window.\n", stderr);
        return 1;
    }
    platform_at_exit = platform;
    if (atexit(shutdown_platform_at_exit) != 0) {
        fputs("Unable to register SDL3 shutdown handler.\n", stderr);
        shutdown_platform_at_exit();
        return 1;
    }
    if (!wolf_legacy_video_init(&video, vga_storage, sizeof(vga_storage),
            framebuffer, sizeof(framebuffer), 320u)) {
        fputs("Unable to initialise the software VGA framebuffer.\n", stderr);
        shutdown_platform_at_exit();
        return 1;
    }
    wolf_legacy_video_set_present_callback(&video, present_frame, platform);
    wolf_legacy_video_set_delay_callback(&video, delay_frame, platform);
    VL_Bind(&video);

    WolfMain();

    VL_Shutdown();
    shutdown_platform_at_exit();
    return 0;
}
