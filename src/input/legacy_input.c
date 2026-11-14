/*
 * Minimal legacy mouse-register compatibility state.
 *
 * The SDL-backed input layer will update this state when it is connected.
 * Keeping the register-shaped interface here lets original C gameplay code
 * compile without DOS interrupt calls.
 */

#include <stdbool.h>

int wolf_native_ax;
int wolf_native_bx;
int wolf_native_cx;
int wolf_native_dx;

void wolf_native_mouse_command(int command)
{
    switch (command) {
    case 0: /* Reset */
        wolf_native_ax = 0;
        wolf_native_bx = 0;
        wolf_native_cx = 0;
        wolf_native_dx = 0;
        break;

    case 3: /* Query buttons and position */
        wolf_native_ax = 0;
        break;

    case 11: /* Clear accumulated movement */
        wolf_native_cx = 0;
        wolf_native_dx = 0;
        break;

    default:
        break;
    }
}
