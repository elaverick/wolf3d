#ifdef NDEBUG
#undef NDEBUG
#endif

#include "native_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int last_mouse_function;

void wolf_platform_mouse_command(
    int function, int *ax, int *bx, int *cx, int *dx)
{
    last_mouse_function = function;
    if (ax != NULL) *ax = 0x1234;
    if (bx != NULL) *bx = 0x5678;
    if (cx != NULL) *cx = 0x9abc;
    if (dx != NULL) *dx = 0xdef0;
}

static FILE *open_test_file(const char *filename)
{
#ifdef _WIN32
    FILE *file = NULL;
    if (fopen_s(&file, filename, "wb") != 0) {
        return NULL;
    }
    return file;
#else
    return fopen(filename, "wb");
#endif
}

static void test_integer_conversion(void)
{
    char buffer[64];

    assert(strcmp(itoa(-12345, buffer, 10), "-12345") == 0);
    assert(strcmp(ltoa(-2147483647L, buffer, 10), "-2147483647") == 0);
    assert(strcmp(ultoa(65535UL, buffer, 16), "ffff") == 0);
    assert(strcmp(itoa(255, buffer, 2), "11111111") == 0);
}

static void test_file_search(void)
{
    const char *filename = "native-compat-test.tmp";
    struct ffblk entry;
    FILE *file = open_test_file(filename);

    assert(file != NULL);
    fputs("test", file);
    fclose(file);

    assert(findfirst(filename, &entry, FA_ARCH) == 0);
    assert(strcmp(entry.ff_name, filename) == 0);
    assert(entry.active != 0);

    /* Episode detection only needs the first match, so verify explicit close. */
    wolf_native_findclose(&entry);
    assert(entry.active == 0);
    assert(findnext(&entry) != 0);

    remove(filename);
}

static void test_file_search_iteration(void)
{
    const char *first = "native-compat-one.tmp";
    const char *second = "native-compat-two.tmp";
    struct ffblk entry;
    FILE *file;
    int count = 0;

    file = open_test_file(first);
    assert(file != NULL);
    fputs("one", file);
    assert(fclose(file) == 0);

    file = open_test_file(second);
    assert(file != NULL);
    fputs("two", file);
    assert(fclose(file) == 0);

    assert(findfirst("native-compat-*.tmp", &entry, FA_ARCH) == 0);
    do {
        ++count;
    } while (findnext(&entry) == 0);

    assert(count == 2);
    assert(entry.active == 0);
    wolf_native_findclose(&entry); /* Closing an exhausted search is harmless. */

    remove(first);
    remove(second);
}

static void test_mouse_register_bridge(void)
{
    wolf_native_mouse_interrupt(3);

    assert(last_mouse_function == 3);
    assert(_AX == 0x1234);
    assert(_BX == 0x5678);
    assert(_CX == 0x9abc);
    assert(_DX == 0xdef0);
}

int main(void)
{
    test_integer_conversion();
    test_file_search();
    test_file_search_iteration();
    test_mouse_register_bridge();

    puts("native compatibility tests passed");
    return 0;
}
