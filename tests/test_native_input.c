#ifdef NDEBUG
#undef NDEBUG
#endif

#include "ID_HEADS_NATIVE.H"
#include "platform.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static boolean fake_keys[NumCodes];
static int fake_pending_scan;
static int fake_pending_ascii;
static int fake_mouse_buttons;
static int fake_mouse_dx;
static int fake_mouse_dy;
static uint64_t fake_time_ms;
static boolean inject_key_on_delay;
static boolean fake_quit_requested;

bool wolf_platform_key_down(int scan_code)
{
    return scan_code >= 0 && scan_code < NumCodes &&
        fake_keys[scan_code];
}

bool wolf_platform_take_key_event(int *scan_code)
{
    if (fake_pending_scan == 0) {
        return false;
    }

    if (scan_code != NULL) {
        *scan_code = fake_pending_scan;
    }
    fake_pending_scan = 0;
    return true;
}

int wolf_platform_take_ascii(void)
{
    int result = fake_pending_ascii;
    fake_pending_ascii = 0;
    return result;
}

void wolf_platform_mouse_command(
    int function, int *ax, int *bx, int *cx, int *dx)
{
    if (ax != NULL) *ax = 0;
    if (bx != NULL) *bx = 0;
    if (cx != NULL) *cx = 0;
    if (dx != NULL) *dx = 0;

    if (function == 0 && ax != NULL) {
        *ax = 0xffff;
    } else if (function == 3) {
        if (bx != NULL) *bx = fake_mouse_buttons;
    } else if (function == 11) {
        if (cx != NULL) *cx = fake_mouse_dx;
        if (dx != NULL) *dx = fake_mouse_dy;
        fake_mouse_dx = 0;
        fake_mouse_dy = 0;
    }
}

bool wolf_platform_pump_events(WolfPlatform *platform)
{
    (void)platform;
    return !fake_quit_requested;
}

void wolf_platform_delay_ms(uint32_t milliseconds)
{
    fake_time_ms += milliseconds;
    if (inject_key_on_delay) {
        inject_key_on_delay = false;
        fake_pending_scan = sc_Space;
        fake_pending_ascii = ' ';
    }
}

uint64_t wolf_platform_time_ms(void)
{
    return fake_time_ms;
}

static void test_keyboard_control(void)
{
    ControlInfo info;

    memset(fake_keys, 0, sizeof(fake_keys));
    fake_keys[sc_UpArrow] = true;
    fake_keys[sc_Control] = true;

    IN_SetControlType(0, ctrl_Keyboard1);
    IN_ReadControl(0, &info);

    assert(info.yaxis == motion_Up);
    assert(info.xaxis == motion_None);
    assert(info.button0);
    assert(!info.button1);
    assert(info.dir == dir_North);
}

static void test_mouse_control(void)
{
    ControlInfo info;

    memset(fake_keys, 0, sizeof(fake_keys));
    fake_mouse_dx = 7;
    fake_mouse_dy = -4;
    fake_mouse_buttons = 3;

    IN_SetControlType(0, ctrl_Mouse);
    IN_ReadControl(0, &info);

    assert(info.x == 7);
    assert(info.y == -4);
    assert(info.xaxis == motion_Right);
    assert(info.yaxis == motion_Up);
    assert(info.button0);
    assert(info.button1);
}

static void test_key_events_and_acknowledgement(void)
{
    fake_pending_scan = sc_A;
    fake_pending_ascii = 'a';

    assert(IN_WaitForKey() == sc_A);
    assert(IN_WaitForASCII() == 'a');

    inject_key_on_delay = true;
    assert(IN_UserInput(70));
    assert(!inject_key_on_delay);

    assert(!IN_UserInput(7));
    assert(fake_time_ms >= 100);
}

static void test_window_close_becomes_escape(void)
{
    fake_quit_requested = true;
    IN_ClearKeysDown();

    assert(IN_CheckAck());
    assert(LastScan == sc_Escape);
    assert(Keyboard[sc_Escape]);

    fake_quit_requested = false;
    IN_ClearKeysDown();
}

int main(void)
{
    IN_Startup();
    assert(MousePresent);

    test_keyboard_control();
    test_window_close_becomes_escape();
    test_mouse_control();
    test_key_events_and_acknowledgement();

    assert(IN_GetScanName(sc_Escape) != NULL);
    IN_Shutdown();

    puts("native input tests passed");
    return 0;
}
