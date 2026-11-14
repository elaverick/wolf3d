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

#include "native_compat.h"

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

#ifndef SCREENWIDTH
#define SCREENWIDTH WOLF_VGA_LEGACY_STRIDE_BYTES
#endif

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
