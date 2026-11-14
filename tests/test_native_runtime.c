#ifdef NDEBUG
#undef NDEBUG
#endif

#include "ID_HEADS.H"

#include <assert.h>
#include <stdio.h>
#include <string.h>

char extension[5] = "TST";

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

int main(void)
{
    const uint8_t *page;
    create_fixture();

    MM_Startup();
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
    assert(PM_GetPage(2) != NULL);

    PM_Shutdown();
    MM_Shutdown();
    remove("VSWAP.TST");
    puts("native runtime resource tests passed");
    return 0;
}
