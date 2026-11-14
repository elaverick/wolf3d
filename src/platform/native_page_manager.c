/*
 * Native VSWAP page manager.
 *
 * Keeps the original page-file layout (header, 32-bit offsets, 16-bit lengths)
 * while replacing DOS conventional/EMS/XMS paging with stable heap-backed
 * pages. Pages are loaded lazily and remain resident for the life of the game.
 */
#include "ID_HEADS.H"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

boolean XMSPresent;
boolean EMSPresent;
word XMSPagesAvail;
word EMSPagesAvail;

word ChunksInFile;
word PMSpriteStart;
word PMSoundStart;
PageListStruct *PMPages;
char PageFileName[13] = "VSWAP.";
static int page_file = -1;
static boolean page_manager_started;
static longword page_frame;
static int main_memory_purge_level;
static byte **page_addresses;

extern char extension[5];

static uint16_t read_u16_le(const byte *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t read_u32_le(const byte *data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static boolean read_exact(int handle, void *destination, size_t length)
{
    byte *cursor = (byte *)destination;
    size_t remaining = length;

    while (remaining > 0) {
        unsigned request = remaining > UINT_MAX ? UINT_MAX : (unsigned)remaining;
        int count = (int)read(handle, cursor, request);

        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (count == 0) {
            return false;
        }

        cursor += count;
        remaining -= (size_t)count;
    }

    return true;
}

static boolean read_page_header(void)
{
    byte header[6];
    byte *offsets = NULL;
    byte *lengths = NULL;
    size_t offsets_size;
    size_t lengths_size;
    uint64_t table_end;
    long page_file_size;
    unsigned index;

    if (!read_exact(page_file, header, sizeof(header))) {
        return false;
    }

    ChunksInFile = read_u16_le(header);
    PMSpriteStart = read_u16_le(header + 2);
    PMSoundStart = read_u16_le(header + 4);

    if (ChunksInFile == 0 ||
        PMSpriteStart > PMSoundStart ||
        PMSoundStart > ChunksInFile) {
        return false;
    }

    offsets_size = (size_t)ChunksInFile * 4u;
    lengths_size = (size_t)ChunksInFile * 2u;
    table_end = 6u + (uint64_t)offsets_size + (uint64_t)lengths_size;
    page_file_size = filelength(page_file);
    if (page_file_size < 0 || table_end > (uint64_t)page_file_size) {
        return false;
    }

    offsets = (byte *)malloc(offsets_size);
    lengths = (byte *)malloc(lengths_size);
    if (offsets == NULL || lengths == NULL) {
        free(offsets);
        free(lengths);
        return false;
    }

    if (!read_exact(page_file, offsets, offsets_size) ||
        !read_exact(page_file, lengths, lengths_size)) {
        free(offsets);
        free(lengths);
        return false;
    }

    MM_GetPtr((memptr *)&PMPages,
        (unsigned long)((size_t)ChunksInFile * sizeof(*PMPages)));
    if (PMPages == NULL) {
        free(offsets);
        free(lengths);
        return false;
    }
    memset(PMPages, 0, (size_t)ChunksInFile * sizeof(*PMPages));

    page_addresses = (byte **)calloc(ChunksInFile, sizeof(*page_addresses));
    if (page_addresses == NULL) {
        MM_FreePtr((memptr *)&PMPages);
        free(offsets);
        free(lengths);
        return false;
    }

    for (index = 0; index < ChunksInFile; ++index) {
        uint32_t page_offset =
            read_u32_le(offsets + (size_t)index * 4u);
        uint16_t page_length =
            read_u16_le(lengths + (size_t)index * 2u);

        if (page_length > PMPageSize ||
            (page_length != 0 &&
             ((uint64_t)page_offset < table_end ||
              (uint64_t)page_offset + (uint64_t)page_length >
                  (uint64_t)page_file_size))) {
            MM_FreePtr((memptr *)&PMPages);
            free(page_addresses);
            page_addresses = NULL;
            free(offsets);
            free(lengths);
            return false;
        }

        PMPages[index].offset = page_offset;
        PMPages[index].length = page_length;
        PMPages[index].xmsPage = -1;
        PMPages[index].emsPage = -1;
        PMPages[index].mainPage = -1;
        PMPages[index].locked = pml_Unlocked;
    }

    free(offsets);
    free(lengths);
    return true;
}

static boolean purge_one_unlocked_page(void)
{
    unsigned index;
    unsigned oldest_index = ChunksInFile;
    longword oldest_hit = UINT32_MAX;

    if (main_memory_purge_level <= 0 ||
        page_addresses == NULL || PMPages == NULL) {
        return false;
    }

    for (index = 0; index < ChunksInFile; ++index) {
        if (page_addresses[index] != NULL &&
            PMPages[index].locked == pml_Unlocked &&
            PMPages[index].lastHit <= oldest_hit) {
            oldest_hit = PMPages[index].lastHit;
            oldest_index = index;
        }
    }

    if (oldest_index >= ChunksInFile) {
        return false;
    }

    free(page_addresses[oldest_index]);
    page_addresses[oldest_index] = NULL;
    PMPages[oldest_index].mainPage = -1;
    PMPages[oldest_index].lastHit = 0;
    return true;
}

void PM_Purge(void)
{
    while (purge_one_unlocked_page()) {
        /* Release every unlocked page; allocation pressure uses one at a time. */
    }
}

void PM_Startup(void)
{
    char filename[64];
    size_t base_length;
    size_t extension_length;

    if (page_manager_started) {
        return;
    }

    base_length = strlen(PageFileName);
    extension_length = strlen(extension);
    if (base_length == 0 || base_length + extension_length >= sizeof(filename)) {
        Quit("Native page manager: invalid page filename");
        return;
    }

    memcpy(filename, PageFileName, base_length);
    memcpy(filename + base_length, extension, extension_length + 1u);

    page_file = open(filename, O_RDONLY | O_BINARY);
    if (page_file < 0) {
        Quit("Native page manager: unable to open VSWAP page file");
        return;
    }

    if (!read_page_header()) {
        close(page_file);
        page_file = -1;
        Quit("Native page manager: invalid VSWAP page table");
        return;
    }

    XMSPresent = false;
    EMSPresent = false;
    XMSPagesAvail = 0;
    EMSPagesAvail = 0;
    page_frame = 0;
    main_memory_purge_level = 0;
    page_manager_started = true;
}

void PM_Shutdown(void)
{
    unsigned index;

    if (page_addresses != NULL) {
        for (index = 0; index < ChunksInFile; ++index) {
            free(page_addresses[index]);
        }
        free(page_addresses);
        page_addresses = NULL;
    }

    if (PMPages != NULL) {
        MM_FreePtr((memptr *)&PMPages);
        PMPages = NULL;
    }

    if (page_file >= 0) {
        close(page_file);
        page_file = -1;
    }

    ChunksInFile = 0;
    PMSpriteStart = 0;
    PMSoundStart = 0;
    page_manager_started = false;
    page_frame = 0;
    main_memory_purge_level = 0;
}

void PM_Reset(void)
{
    unsigned index;

    page_frame = 0;
    if (PMPages == NULL) {
        return;
    }

    for (index = 0; index < ChunksInFile; ++index) {
        PMPages[index].lastHit = 0;
    }
}

void PM_NextFrame(void)
{
    ++page_frame;
}

void PM_SetPageLock(int pagenum, PMLockType lock)
{
    if (pagenum < 0 || pagenum >= ChunksInFile || PMPages == NULL) {
        Quit("Native page manager: invalid page lock index");
        return;
    }
    PMPages[pagenum].locked = lock;
}

void PM_SetMainMemPurge(int level)
{
    main_memory_purge_level = level < 0 ? 0 : level;
}

/* Retain the original header/API spelling as a compatibility alias. */
void PM_SetMainPurge(int level)
{
    PM_SetMainMemPurge(level);
}

void PM_CheckMainMem(void)
{
}

memptr PM_GetPageAddress(int pagenum)
{
    if (pagenum < 0 || pagenum >= ChunksInFile ||
        page_addresses == NULL) {
        return NULL;
    }
    return page_addresses[pagenum];
}

memptr PM_GetPage(int pagenum)
{
    PageListStruct *page;
    byte *data;
    size_t remaining;
    long offset;

    if (!page_manager_started || pagenum < 0 || pagenum >= ChunksInFile) {
        Quit("Native page manager: invalid page request");
        return NULL;
    }

    page = &PMPages[pagenum];
    if (page_addresses[pagenum] != NULL) {
        page->lastHit = ++page_frame;
        return page_addresses[pagenum];
    }

    if (page->offset == 0 || page->length == 0) {
        Quit("Native page manager: attempted to load a sparse page");
        return NULL;
    }
    if (page->length > PMPageSize) {
        Quit("Native page manager: page metadata outside supported limits");
        return NULL;
    }

#if LONG_MAX < UINT32_MAX
    if (page->offset > (uint32_t)LONG_MAX) {
        Quit("Native page manager: page offset exceeds file API limits");
        return NULL;
    }
#endif
    offset = (long)page->offset;
    if (lseek(page_file, offset, SEEK_SET) != offset) {
        Quit("Native page manager: VSWAP seek failed");
        return NULL;
    }

    data = (byte *)malloc(page->length);
    while (data == NULL && purge_one_unlocked_page()) {
        data = (byte *)malloc(page->length);
    }
    if (data == NULL) {
        Quit("Native page manager: page allocation failed");
        return NULL;
    }

    remaining = page->length;
    {
        byte *cursor = data;
        while (remaining > 0) {
            int count = (int)read(page_file, cursor, (unsigned)remaining);
            if (count < 0 && errno == EINTR) {
                continue;
            }
            if (count <= 0) {
                free(data);
                Quit("Native page manager: VSWAP page read failed");
                return NULL;
            }
            cursor += count;
            remaining -= (size_t)count;
        }
    }

    page_addresses[pagenum] = data;
    page->lastHit = ++page_frame;
    page->mainPage = pagenum;
    return data;
}

void PM_Preload(boolean (*update)(word current, word total))
{
    unsigned index;

    if (!page_manager_started) {
        return;
    }

    for (index = 0; index < ChunksInFile; ++index) {
        if (PMPages[index].offset != 0 && PMPages[index].length != 0) {
            (void)PM_GetPage((int)index);
        }
        if (update != NULL && !update((word)(index + 1), ChunksInFile)) {
            break;
        }
    }
}
