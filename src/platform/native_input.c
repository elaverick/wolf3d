/*
 * Native C implementation of the original ID_IN input manager.
 *
 * Keyboard state is exposed in the original DOS scan-code namespace so the
 * existing gameplay code and key configuration remain unchanged. SDL3 stays
 * behind the platform API.
 */

#include "ID_HEADS_NATIVE.H"
#include "platform.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

boolean MousePresent;
boolean JoysPresent[MaxJoys];
boolean JoyPadPresent;

boolean Keyboard[NumCodes];
boolean Paused;
char LastASCII;
ScanCode LastScan;

KeyboardDef KbdDefs = {
    0x1d, 0x38, 0x47, 0x48, 0x49,
    0x4b, 0x4d, 0x4f, 0x50, 0x51
};
JoystickDef JoyDefs[MaxJoys];
ControlType Controls[MaxPlayers];

longword MouseDownCount;

Demo DemoMode = demo_Off;
byte *DemoBuffer;
word DemoOffset;
word DemoSize;

static boolean input_started;
static void (*key_hook)(void);
static boolean ack_button_state[8];

static const Direction direction_table[9] = {
    dir_NorthWest, dir_North, dir_NorthEast,
    dir_West, dir_None, dir_East,
    dir_SouthWest, dir_South, dir_SouthEast
};

static void pump_platform_input(void)
{
    int scan_code;
    int ascii;

    (void)wolf_platform_pump_events(NULL);

    while (wolf_platform_take_key_event(&scan_code)) {
        if (scan_code == 0x7f) {
            Paused = true;
        } else if (scan_code > 0 && scan_code < NumCodes) {
            LastScan = (ScanCode)scan_code;
            if (key_hook != NULL) {
                key_hook();
            }
        }
    }

    ascii = wolf_platform_take_ascii();
    if (ascii > 0 && ascii < 128) {
        LastASCII = (char)ascii;
    }

    {
        int code;

        for (code = 0; code < NumCodes; ++code) {
            Keyboard[code] = wolf_platform_key_down(code);
        }
    }
}

static void get_mouse_delta(int *x, int *y)
{
    int ax;
    int bx;
    int cx;
    int dx;

    wolf_platform_mouse_command(11, &ax, &bx, &cx, &dx);
    if (x != NULL) *x = cx;
    if (y != NULL) *y = dx;
}

static word get_mouse_buttons(void)
{
    int ax;
    int bx;
    int cx;
    int dx;

    wolf_platform_mouse_command(3, &ax, &bx, &cx, &dx);
    return (word)bx;
}

static void get_joystick_delta(word joy, int *dx, int *dy)
{
    (void)joy;
    if (dx != NULL) *dx = 0;
    if (dy != NULL) *dy = 0;
}

static word get_joystick_buttons(word joy)
{
    (void)joy;
    return 0;
}

void IN_Startup(void)
{
    int ax;
    int bx;
    int cx;
    int dx;
    int index;

    if (input_started) {
        return;
    }

    wolf_platform_mouse_command(0, &ax, &bx, &cx, &dx);
    MousePresent = ax == 0xffff;
    JoyPadPresent = false;

    for (index = 0; index < MaxJoys; ++index) {
        JoysPresent[index] = false;
    }

    for (index = 0; index < MaxPlayers; ++index) {
        Controls[index] = ctrl_Keyboard1;
    }

    IN_ClearKeysDown();
    input_started = true;
}

void IN_Shutdown(void)
{
    if (!input_started) {
        return;
    }

    MousePresent = false;
    memset(JoysPresent, 0, sizeof(JoysPresent));
    input_started = false;
}

void IN_Default(boolean gotit, ControlType in)
{
    if (!gotit ||
        ((in == ctrl_Joystick1) && !JoysPresent[0]) ||
        ((in == ctrl_Joystick2) && !JoysPresent[1]) ||
        ((in == ctrl_Mouse) && !MousePresent)) {
        in = ctrl_Keyboard1;
    }

    IN_SetControlType(0, in);
}

void IN_SetKeyHook(void (*hook)(void))
{
    key_hook = hook;
}

void IN_ClearKeysDown(void)
{
    int scan_code;

    LastScan = sc_None;
    LastASCII = key_None;
    memset(Keyboard, 0, sizeof(Keyboard));

    while (wolf_platform_take_key_event(&scan_code)) {
        (void)scan_code;
    }
    (void)wolf_platform_take_ascii();
}

void IN_ReadCursor(CursorInfo *info)
{
    IN_ReadControl(0, info);
}

void IN_ReadControl(int player, ControlInfo *info)
{
    boolean real_delta = false;
    byte demo_byte = 0;
    word buttons = 0;
    int dx = 0;
    int dy = 0;
    Motion mx = motion_None;
    Motion my = motion_None;
    ControlType type;
    KeyboardDef *definition;

    if (info == NULL || player < 0 || player >= MaxPlayers) {
        return;
    }

    pump_platform_input();

    if (DemoMode == demo_Playback) {
        if (DemoBuffer == NULL ||
            (size_t)DemoOffset + 1u >= (size_t)DemoSize) {
            DemoMode = demo_PlayDone;
        } else {
            demo_byte = DemoBuffer[DemoOffset + 1];
            my = (Motion)((demo_byte & 3u) - 1);
            mx = (Motion)(((demo_byte >> 2) & 3u) - 1);
            buttons = (word)((demo_byte >> 4) & 3u);

            if (--DemoBuffer[DemoOffset] == 0) {
                DemoOffset = (word)(DemoOffset + 2u);
                if (DemoOffset >= DemoSize) {
                    DemoMode = demo_PlayDone;
                }
            }
        }
    }

    if (DemoMode == demo_PlayDone) {
        mx = motion_None;
        my = motion_None;
        buttons = 0;
        real_delta = false;
    } else if (DemoMode != demo_Playback) {
        type = Controls[player];

        switch (type) {
            case ctrl_Keyboard:
                definition = &KbdDefs;

                if (Keyboard[definition->upleft]) {
                    mx = motion_Left;
                    my = motion_Up;
                } else if (Keyboard[definition->upright]) {
                    mx = motion_Right;
                    my = motion_Up;
                } else if (Keyboard[definition->downleft]) {
                    mx = motion_Left;
                    my = motion_Down;
                } else if (Keyboard[definition->downright]) {
                    mx = motion_Right;
                    my = motion_Down;
                }

                if (Keyboard[definition->up]) {
                    my = motion_Up;
                } else if (Keyboard[definition->down]) {
                    my = motion_Down;
                }

                if (Keyboard[definition->left]) {
                    mx = motion_Left;
                } else if (Keyboard[definition->right]) {
                    mx = motion_Right;
                }

                if (Keyboard[definition->button0]) buttons |= 1u;
                if (Keyboard[definition->button1]) buttons |= 2u;
                break;

            case ctrl_Joystick1:
            case ctrl_Joystick2:
                get_joystick_delta(
                    (word)(type - ctrl_Joystick), &dx, &dy);
                buttons = get_joystick_buttons(
                    (word)(type - ctrl_Joystick));
                real_delta = true;
                break;

            case ctrl_Mouse:
                get_mouse_delta(&dx, &dy);
                buttons = get_mouse_buttons();
                real_delta = true;
                break;

            default:
                break;
        }
    }

    if (real_delta) {
        mx = dx < 0 ? motion_Left : (dx > 0 ? motion_Right : motion_None);
        my = dy < 0 ? motion_Up : (dy > 0 ? motion_Down : motion_None);
    } else {
        dx = (int)mx * 127;
        dy = (int)my * 127;
    }

    info->x = dx;
    info->xaxis = mx;
    info->y = dy;
    info->yaxis = my;
    info->button0 = (buttons & (1u << 0)) != 0;
    info->button1 = (buttons & (1u << 1)) != 0;
    info->button2 = (buttons & (1u << 2)) != 0;
    info->button3 = (buttons & (1u << 3)) != 0;
    info->dir = direction_table[((my + 1) * 3) + (mx + 1)];

    if (DemoMode == demo_Record && DemoBuffer != NULL &&
        DemoSize >= 2) {
        demo_byte = (byte)((buttons << 4) |
            (((unsigned)mx + 1u) << 2) |
            ((unsigned)my + 1u));

        if ((size_t)DemoOffset + 1u < (size_t)DemoSize &&
            DemoBuffer[DemoOffset + 1] == demo_byte &&
            DemoBuffer[DemoOffset] < 255) {
            ++DemoBuffer[DemoOffset];
        } else {
            if (DemoOffset != 0 || DemoBuffer[DemoOffset] != 0) {
                DemoOffset = (word)(DemoOffset + 2u);
            }

            if ((size_t)DemoOffset + 1u >= (size_t)DemoSize) {
                DemoMode = demo_Off;
            } else {
                DemoBuffer[DemoOffset] = 1;
                DemoBuffer[DemoOffset + 1] = demo_byte;
            }
        }
    }
}

void IN_SetControlType(int player, ControlType type)
{
    if (player >= 0 && player < MaxPlayers) {
        Controls[player] = type;
    }
}

void IN_GetJoyAbs(word joy, word *xp, word *yp)
{
    (void)joy;
    if (xp != NULL) *xp = 0;
    if (yp != NULL) *yp = 0;
}

void IN_SetupJoy(
    word joy, word minx, word maxx, word miny, word maxy)
{
    (void)joy;
    (void)minx;
    (void)maxx;
    (void)miny;
    (void)maxy;
}

void IN_StopDemo(void)
{
    DemoMode = demo_Off;
    DemoBuffer = NULL;
    DemoOffset = 0;
    DemoSize = 0;
}

void IN_FreeDemoBuffer(void)
{
    /* Demo memory is owned by the engine memory manager. */
    DemoBuffer = NULL;
    DemoOffset = 0;
    DemoSize = 0;
}

void IN_StartAck(void)
{
    unsigned buttons;
    unsigned index;

    IN_ClearKeysDown();
    memset(ack_button_state, 0, sizeof(ack_button_state));

    buttons = (unsigned)IN_JoyButtons() << 4;
    if (MousePresent) {
        buttons |= IN_MouseButtons();
    }

    for (index = 0; index < 8; ++index, buttons >>= 1) {
        if (buttons & 1u) {
            ack_button_state[index] = true;
        }
    }
}

boolean IN_CheckAck(void)
{
    unsigned buttons;
    unsigned index;

    pump_platform_input();
    if (LastScan != sc_None || LastASCII != key_None) {
        return true;
    }

    buttons = (unsigned)IN_JoyButtons() << 4;
    if (MousePresent) {
        buttons |= IN_MouseButtons();
    }

    for (index = 0; index < 8; ++index, buttons >>= 1) {
        if (buttons & 1u) {
            if (!ack_button_state[index]) {
                return true;
            }
        } else {
            ack_button_state[index] = false;
        }
    }

    return false;
}

void IN_Ack(void)
{
    IN_StartAck();
    while (!IN_CheckAck()) {
        wolf_platform_delay_ms(1);
    }
}

void IN_AckBack(void)
{
    IN_Ack();
}

boolean IN_UserInput(longword delay)
{
    uint64_t start_time;
    uint64_t delay_ms;

    start_time = wolf_platform_time_ms();
    delay_ms = ((uint64_t)delay * 1000u + 69u) / 70u;
    IN_StartAck();

    do {
        if (IN_CheckAck()) {
            return true;
        }
        if (wolf_platform_time_ms() - start_time >= delay_ms) {
            break;
        }
        wolf_platform_delay_ms(1);
    } while (true);

    return false;
}

char IN_WaitForASCII(void)
{
    while (LastASCII == key_None) {
        pump_platform_input();
        if (LastASCII == key_None) {
            wolf_platform_delay_ms(1);
        }
    }

    {
        char result = LastASCII;
        LastASCII = key_None;
        return result;
    }
}

ScanCode IN_WaitForKey(void)
{
    while (LastScan == sc_None) {
        pump_platform_input();
        if (LastScan == sc_None) {
            wolf_platform_delay_ms(1);
        }
    }

    {
        ScanCode result = LastScan;
        LastScan = sc_None;
        return result;
    }
}

word IN_GetJoyButtonsDB(word joy)
{
    (void)joy;
    return 0;
}

byte *IN_GetScanName(ScanCode scan_code)
{
    static byte name[8];

    (void)snprintf((char *)name, sizeof(name), "0x%02X", scan_code);
    return name;
}

byte IN_MouseButtons(void)
{
    return MousePresent ? (byte)get_mouse_buttons() : 0;
}

byte IN_JoyButtons(void)
{
    return 0;
}

void INL_GetJoyDelta(word joy, int *dx, int *dy)
{
    get_joystick_delta(joy, dx, dy);
}
