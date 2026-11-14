#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "native_compat.h"
#include "platform.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

int _AX;
int _BX;
int _CX;
int _DX;

void wolf_native_mouse_interrupt(int function)
{
    wolf_platform_mouse_command(
        function, &_AX, &_BX, &_CX, &_DX);
}

static void copy_filename(char *destination, size_t capacity, const char *path)
{
    const char *filename = path;
    const char *separator;

    if (destination == NULL || capacity == 0 || path == NULL) {
        return;
    }

    separator = strrchr(path, '/');
    if (separator != NULL && separator[1] != '\0') {
        filename = separator + 1;
    }

    separator = strrchr(filename, '\\');
    if (separator != NULL && separator[1] != '\0') {
        filename = separator + 1;
    }

    (void)snprintf(destination, capacity, "%s", filename);
}

int findfirst(const char *pattern, struct ffblk *entry, int attributes)
{
    if (pattern == NULL || entry == NULL) {
        return -1;
    }

    memset(entry, 0, sizeof(*entry));
    (void)attributes;

#ifdef _WIN32
    WIN32_FIND_DATAA *find_data =
        (WIN32_FIND_DATAA *)malloc(sizeof(*find_data));
    HANDLE handle;

    if (find_data == NULL) {
        return -1;
    }

    handle = FindFirstFileA(pattern, find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        free(find_data);
        return -1;
    }

    entry->handle = (void *)handle;
    entry->find_data = find_data;
    copy_filename(
        entry->ff_name, sizeof(entry->ff_name), find_data->cFileName);
    return 0;
#else
    if (glob(pattern, 0, NULL, &entry->matches) != 0 ||
        entry->matches.gl_pathc == 0) {
        globfree(&entry->matches);
        return -1;
    }

    entry->index = 0;
    copy_filename(
        entry->ff_name,
        sizeof(entry->ff_name),
        entry->matches.gl_pathv[entry->index]);
    return 0;
#endif
}

int findnext(struct ffblk *entry)
{
    if (entry == NULL) {
        return 1;
    }

#ifdef _WIN32
    WIN32_FIND_DATAA *find_data = (WIN32_FIND_DATAA *)entry->find_data;
    HANDLE handle = (HANDLE)entry->handle;

    if (find_data == NULL || handle == INVALID_HANDLE_VALUE ||
        !FindNextFileA(handle, find_data)) {
        if (handle != INVALID_HANDLE_VALUE && handle != NULL) {
            FindClose(handle);
        }
        free(find_data);
        entry->handle = NULL;
        entry->find_data = NULL;
        return 1;
    }

    copy_filename(
        entry->ff_name, sizeof(entry->ff_name), find_data->cFileName);
    return 0;
#else
    if (entry->index + 1 >= entry->matches.gl_pathc) {
        globfree(&entry->matches);
        entry->index = 0;
        return 1;
    }

    ++entry->index;
    copy_filename(
        entry->ff_name,
        sizeof(entry->ff_name),
        entry->matches.gl_pathv[entry->index]);
    return 0;
#endif
}

int _dos_write(int handle, const void *buffer, unsigned count, unsigned *written)
{
#ifdef _WIN32
    int result = _write(handle, buffer, count);
#else
    ssize_t result = write(handle, buffer, (size_t)count);
#endif

    if (result < 0) {
        if (written != NULL) {
            *written = 0;
        }
        return errno != 0 ? errno : -1;
    }

    if (written != NULL) {
        *written = (unsigned)result;
    }

    return 0;
}

static char *convert_unsigned(unsigned long value, int negative, char *buffer, int radix)
{
    char temporary[sizeof(unsigned long) * CHAR_BIT + 1];
    size_t count = 0;
    size_t output = 0;
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    if (buffer == NULL || radix < 2 || radix > 36) {
        return buffer;
    }

    do {
        temporary[count++] = digits[value % (unsigned)radix];
        value /= (unsigned)radix;
    } while (value != 0);

    if (negative) {
        buffer[output++] = '-';
    }

    while (count > 0) {
        buffer[output++] = temporary[--count];
    }

    buffer[output] = '\0';
    return buffer;
}

char *wolf_native_itoa(int value, char *buffer, int radix)
{
    unsigned int magnitude;
    int negative = value < 0 && radix == 10;

    magnitude = negative ?
        (unsigned int)(-(value + 1)) + 1u : (unsigned int)value;

    return convert_unsigned(magnitude, negative, buffer, radix);
}

char *wolf_native_ltoa(long value, char *buffer, int radix)
{
    unsigned long magnitude;
    int negative = value < 0 && radix == 10;

    magnitude = negative ?
        (unsigned long)(-(value + 1L)) + 1UL : (unsigned long)value;

    return convert_unsigned(magnitude, negative, buffer, radix);
}

char *wolf_native_ultoa(unsigned long value, char *buffer, int radix)
{
    return convert_unsigned(value, 0, buffer, radix);
}
