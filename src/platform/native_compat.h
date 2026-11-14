/*
 * Small, explicit compatibility layer for Borland/Turbo C library calls
 * still used by the original gameplay sources.
 *
 * Keep these shims isolated. New native code should use the standard C
 * library and platform services directly.
 */

#ifndef WOLF3D_NATIVE_COMPAT_H
#define WOLF3D_NATIVE_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <direct.h>
#else
#include <glob.h>
#include <unistd.h>
#endif

#ifndef O_BINARY
#ifdef _WIN32
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif
#ifndef S_IREAD
#ifdef _WIN32
#define S_IREAD _S_IREAD
#else
#define S_IREAD S_IRUSR
#endif
#endif
#ifndef S_IWRITE
#ifdef _WIN32
#define S_IWRITE _S_IWRITE
#else
#define S_IWRITE S_IWUSR
#endif
#endif
#ifndef FA_ARCH
#define FA_ARCH 0x20
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef _fmemcpy
#define _fmemcpy memcpy
#endif
#ifndef _fmemset
#define _fmemset memset
#endif
#ifndef _fmemcmp
#define _fmemcmp memcmp
#endif
#ifndef _fstrcpy
#define _fstrcpy strcpy
#endif
#ifndef _fstrncpy
#define _fstrncpy strncpy
#endif
#ifndef _fstrcat
#define _fstrcat strcat
#endif
#ifndef _fstrlen
#define _fstrlen strlen
#endif
#ifndef _fstrcmp
#define _fstrcmp strcmp
#endif

#ifdef _WIN32
#define _fstrnicmp _strnicmp
#define unlink _unlink
#define read _read
#define write _write
#define close _close
#define lseek _lseek
#define open _open
#define creat _creat
#else
#include <strings.h>
#define _fstrnicmp strncasecmp
#endif

typedef struct ffblk {
    char ff_name[260];
#ifdef _WIN32
    HANDLE handle;
    WIN32_FIND_DATAA find_data;
#else
    glob_t matches;
    size_t index;
#endif
} ffblk;

int findfirst(const char *pattern, struct ffblk *entry, int attributes);
int findnext(struct ffblk *entry);

int _dos_write(int handle, const void *buffer, unsigned count, unsigned *written);

char *wolf_native_itoa(int value, char *buffer, int radix);
char *wolf_native_ltoa(long value, char *buffer, int radix);
char *wolf_native_ultoa(unsigned long value, char *buffer, int radix);

#define itoa wolf_native_itoa
#define ltoa wolf_native_ltoa
#define ultoa wolf_native_ultoa

/* DOS mouse interrupt register compatibility used by ID_IN.H and game code. */
extern int _AX;
extern int _BX;
extern int _CX;
extern int _DX;
void wolf_native_mouse_interrupt(int function);

/* BIOS services have no equivalent in a windowed native build. */
#define geninterrupt(interrupt_number) ((void)(interrupt_number))
#define peek(segment, offset) ((void)(segment), (void)(offset), 0)

#endif
