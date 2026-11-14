#ifdef NDEBUG
#undef NDEBUG
#endif

#include "ID_HEADS.H"

#include <assert.h>
#include <stdio.h>
#include <string.h>

char extension[5] = "TST";

void wolf_platform_mouse_command(
    int function,
    int *ax,
    int *bx,
    int *cx,
    int *dx)
{
    (void)function;
    if (ax != NULL) *ax = 0;
    if (bx != NULL) *bx = 0;
    if (cx != NULL) *cx = 0;
    if (dx != NULL) *dx = 0;
}

void Quit(char *error)
{
    fprintf(stderr, "%s\n", error);
    abort();
}

static void write_u16(FILE *file, uint16_t value)
{
    fputc((int)(value & 0xffu), file);
    fputc((int)(value >> 8), file);
}

static void write_u32(FILE *file, uint32_t value)
{
    fputc((int)(value & 0xffu), file);
    fputc((int)((value >> 8) & 0xffu), file);
    fputc((int)((value >> 16) & 0xffu), file);
    fputc((int)((value >> 24) & 0xffu), file);
}

static boolean progress_seen;
static boolean progress(word current, word total)
{
    assert(current > 0);
    assert(total == 3);
    progress_seen = true;
    return true;
}

static void create_fixture(void)
{
    FILE *file = fopen("VSWAP.TST", "wb");
    assert(file != NULL);

    write_u16(file, 3);  /* chunks */
    write_u16(file, 1);  /* first sprite page */
    write_u16(file, 2);  /* first sound page */

    /* The 6-byte header plus 3 offsets and 3 lengths is 24 bytes. */
    write_u32(file, 24);
    write_u32(file, 28);
    write_u32(file, 31);
    write_u16(file, 4);
    write_u16(file, 3);
    write_u16(file, 2);

    fwrite((const uint8_t[]){1, 2, 3, 4, 5, 6, 7, 8, 9}, 1, 9, file);
    assert(fclose(file) == 0);
}

static void test_memory_purge_and_lock_semantics(void)
{
    memptr purgeable = NULL;
    memptr locked = NULL;
    memptr retained = NULL;

    MM_GetPtr(&purgeable, 32);
    MM_GetPtr(&locked, 64);
    MM_GetPtr(&retained, 16);
    assert(purgeable != NULL && locked != NULL && retained != NULL);

    MM_SetPurge(&purgeable, 3);
    MM_SetPurge(&locked, 3);
    MM_SetLock(&locked, true);

    MM_Purge();
    assert(purgeable == NULL);
    assert(locked != NULL);
    assert(retained != NULL);

    MM_SetLock(&locked, false);
    MM_Purge();
    assert(locked == NULL);

    MM_FreePtr(&retained);
    assert(retained == NULL);
}

int main(void)
{
    const uint8_t *page;
    create_fixture();

    MM_Startup();
    test_memory_purge_and_lock_semantics();
    PM_Startup();

    assert(ChunksInFile == 3);
    assert(PMSpriteStart == 1);
    assert(PMSoundStart == 2);
    assert(PM_GetPageAddress(1) == NULL);

    page = (const uint8_t *)PM_GetPage(1);
    assert(page != NULL);
    assert(page[0] == 5 && page[1] == 6 && page[2] == 7);
    assert(PM_GetPage(1) == page);
    assert(PM_GetPageAddress(1) == page);

    PM_SetPageLock(2, pml_Locked);
    assert(PMPages[2].locked == pml_Locked);
    PM_NextFrame();
    PM_Preload(progress);
    assert(progress_seen);
    assert(PM_GetPage(0) != NULL);
    assert(PM_GetPage(1) == page);
    assert(PM_GetPage(2) != NULL);

    /*
     * The main-memory purge setting enables LRU eviction for unlocked VSWAP
     * pages, but an explicitly locked page must remain resident.
     */
    PM_SetMainMemPurge(3);
    MM_Purge();
    assert(PM_GetPageAddress(0) == NULL);
    assert(PM_GetPageAddress(1) == NULL);
    assert(PM_GetPageAddress(2) != NULL);
    assert(PM_GetPage(1) != NULL); /* A purged page reloads transparently. */

    PM_Shutdown();
    MM_Shutdown();
    remove("VSWAP.TST");
    puts("native runtime resource tests passed");
    return 0;
}
