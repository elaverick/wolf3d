/*
 * Portable replacement for the DOS compiler and memory-model setup in
 * ID_HEADS.H. Keep the original game data headers and public game types.
 */

#ifndef WOLF3D_ID_HEADS_NATIVE_H
#define WOLF3D_ID_HEADS_NATIVE_H

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "platform.h"

#ifndef far
#define far
#endif
#ifndef near
#define near
#endif
#ifndef huge
#define huge
#endif
#ifndef _seg
#define _seg
#endif

#define __ID_GLOB__
#include "VERSION.H"

typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t longword;
typedef byte *Ptr;

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point ul;
    Point lr;
} Rect;

#define nil ((void *)0)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef SCREENWIDTH
#define SCREENWIDTH WOLF_VGA_LEGACY_STRIDE_BYTES
#endif

static inline char *wolf3d_unsigned_to_base(
    uint64_t value, char *buffer, unsigned radix, bool negative)
{
    char reversed[65];
    size_t length = 0;
    size_t output = 0;
    const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    if (buffer == NULL || radix < 2 || radix > 36) {
        return buffer;
    }

    do {
        reversed[length++] = digits[value % radix];
        value /= radix;
    } while (value != 0 && length < sizeof(reversed));

    if (negative) {
        buffer[output++] = '-';
    }

    while (length > 0) {
        buffer[output++] = reversed[--length];
    }

    buffer[output] = '\\0';
    return buffer;
}

static inline char *wolf3d_itoa(int value, char *buffer, int radix)
{
    bool negative = value < 0 && radix == 10;
    uint64_t magnitude = negative ?
        (uint64_t)(-(int64_t)value) : (uint64_t)(unsigned)value;

    return wolf3d_unsigned_to_base(
        magnitude, buffer, (unsigned)radix, negative);
}

static inline char *wolf3d_ltoa(long value, char *buffer, int radix)
{
    bool negative = value < 0 && radix == 10;
    uint64_t magnitude = negative ?
        (uint64_t)(-(int64_t)value) : (uint64_t)(unsigned long)value;

    return wolf3d_unsigned_to_base(
        magnitude, buffer, (unsigned)radix, negative);
}

static inline char *wolf3d_ultoa(unsigned long value, char *buffer, int radix)
{
    return wolf3d_unsigned_to_base(
        (uint64_t)value, buffer, (unsigned)radix, false);
}

#define itoa(value, buffer, radix) wolf3d_itoa((value), (buffer), (radix))
#define ltoa(value, buffer, radix) wolf3d_ltoa((value), (buffer), (radix))
#define ultoa(value, buffer, radix) wolf3d_ultoa((value), (buffer), (radix))

#define _fstrlen strlen
#define _fmemcpy memcpy
#define _fmemset memset
#define _fmemcmp memcmp
#define _fstrcpy strcpy
#define _fstrcat strcat
#define _fstrcmp strcmp

struct ffblk { int unused; };
#define FA_ARCH 0x20
#define findfirst(pattern, result, attributes) \\
    (wolf_platform_any_file_matches(pattern) ? 0 : -1)

extern int wolf_native_ax;
extern int wolf_native_bx;
extern int wolf_native_cx;
extern int wolf_native_dx;
void wolf_native_mouse_command(int command);
#define _AX wolf_native_ax
#define _BX wolf_native_bx
#define _CX wolf_native_cx
#define _DX wolf_native_dx
#define Mouse(command) wolf_native_mouse_command(command)

extern char signon;
#define introscn signon

#ifdef JAPAN
#ifdef JAPDEMO
#include "FOREIGN/JAPAN/GFXV_WJ1.H"
#else
#include "FOREIGN/JAPAN/GFXV_WJ6.H"
#endif
#include "AUDIOWL6.H"
#include "MAPSWL6.H"
#else
#ifndef SPEAR
#include "GFXV_WL6.H"
#include "AUDIOWL6.H"
#include "MAPSWL6.H"
#else
#ifndef SPEARDEMO
#include "GFXV_SOD.H"
#include "AUDIOSOD.H"
#include "MAPSSOD.H"
#else
#include "GFXV_SDM.H"
#include "AUDIOSDM.H"
#include "MAPSSDM.H"
#endif
#endif
#endif

#define GREXT "VGA"

#include "ID_MM.H"
#include "ID_PM.H"
#include "ID_CA.H"
#include "legacy_vl.h"
#include "ID_VH.H"
#include "ID_IN.H"
#include "ID_SD.H"
#include "ID_US.H"

void Quit(char *error);

#define PORTTILESWIDE 20
#define PORTTILESHIGH 13
#define UPDATEWIDE PORTTILESWIDE
#define UPDATEHIGH PORTTILESHIGH

#define MAXTICS 10
#define DEMOTICS 4
#define UPDATETERMINATE 0x0301

extern unsigned mapwidth;
extern unsigned mapheight;
extern unsigned tics;
extern boolean compatability;

extern byte *updateptr;
extern unsigned uwidthtable[UPDATEHIGH];
extern unsigned blockstarts[UPDATEWIDE * UPDATEHIGH];

extern byte fontcolor;
extern byte backcolor;

#define SETFONTCOLOR(f, b) fontcolor = (f); backcolor = (b);

#endif
