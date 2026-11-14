/*
 * Native heap implementation of the original ID memory-manager API.
 *
 * Native pointers are stable malloc allocations, so compaction and EMS/XMS
 * mapping are unnecessary. Purge and lock semantics are retained: the cache
 * manager may mark graphics/audio chunks as purgeable, and those handles are
 * cleared when memory pressure requires their storage to be reclaimed.
 */
#include "ID_HEADS.H"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct NativeAllocation {
    void *address;
    memptr *handle;
    size_t size;
    uint64_t last_touched;
    unsigned purge_level;
    boolean locked;
    struct NativeAllocation *next;
} NativeAllocation;

static NativeAllocation *allocations;
static boolean memory_started;
static boolean bomb_on_error = true;
static uint64_t allocation_clock;

mminfotype mminfo;
memptr bufferseg;
boolean mmerror;
void (*beforesort)(void);
void (*aftersort)(void);

static NativeAllocation *find_allocation(void *address)
{
    NativeAllocation *entry;

    for (entry = allocations; entry != NULL; entry = entry->next) {
        if (entry->address == address) {
            return entry;
        }
    }
    return NULL;
}

static boolean register_allocation(
    void *address, size_t size, memptr *handle)
{
    NativeAllocation *entry = (NativeAllocation *)malloc(sizeof(*entry));

    if (entry == NULL) {
        free(address);
        if (handle != NULL) {
            *handle = NULL;
        }
        Quit("Native memory manager: allocation tracking failed");
        return false;
    }

    entry->address = address;
    /* Only purgeable allocations retain a handle to a caller-owned pointer slot. */
    entry->handle = NULL;
    entry->size = size;
    entry->last_touched = ++allocation_clock;
    entry->purge_level = 0;
    entry->locked = false;
    entry->next = allocations;
    allocations = entry;
    return true;
}

static void release_entry(NativeAllocation **link)
{
    NativeAllocation *entry = *link;

    *link = entry->next;
    if (entry->handle != NULL && *entry->handle == entry->address) {
        *entry->handle = NULL;
    }
    free(entry->address);
    free(entry);
}

static boolean purge_one(unsigned purge_level)
{
    NativeAllocation **cursor = &allocations;
    NativeAllocation **oldest_link = NULL;
    uint64_t oldest_time = UINT64_MAX;

    while (*cursor != NULL) {
        NativeAllocation *entry = *cursor;

        if (!entry->locked &&
            entry->purge_level == purge_level &&
            entry->last_touched < oldest_time) {
            oldest_time = entry->last_touched;
            oldest_link = cursor;
        }
        cursor = &entry->next;
    }

    if (oldest_link == NULL) {
        return false;
    }

    release_entry(oldest_link);
    return true;
}

/*
 * Explicit native trim hook, also used by the allocator after malloc fails.
 * Purge level 3 is the least important data; lower levels are reclaimed only
 * after all higher-level candidates have been exhausted.
 */
void MM_Purge(void)
{
    unsigned level;

    for (level = 3u; level > 0u; --level) {
        while (purge_one(level)) {
            /* Reclaim every eligible allocation at this level. */
        }
    }
}

static void *allocate_with_purge(size_t size)
{
    void *address;
    unsigned level;

    address = malloc(size);
    if (address != NULL) {
        return address;
    }

    for (level = 3u; level > 0u; --level) {
        while (purge_one(level)) {
            address = malloc(size);
            if (address != NULL) {
                return address;
            }
        }
    }

    return NULL;
}

void MM_Startup(void)
{
    if (memory_started) {
        return;
    }

    memory_started = true;
    mmerror = false;
    bomb_on_error = true;
    mminfo.nearheap = 0;
    mminfo.farheap = 0;
    mminfo.EMSmem = 0;
    mminfo.XMSmem = 0;
    /* Prevent the DOS low-memory warning from rejecting a native build. */
    mminfo.mainmem = LONG_MAX;

    if (bufferseg == NULL) {
        MM_GetPtr(&bufferseg, BUFFERSIZE);
    }
}

void MM_Shutdown(void)
{
    NativeAllocation *entry = allocations;

    while (entry != NULL) {
        NativeAllocation *next = entry->next;

        free(entry->address);
        free(entry);
        entry = next;
    }

    allocations = NULL;
    bufferseg = NULL;
    memory_started = false;
    mmerror = false;
    allocation_clock = 0;
}

void MM_MapEMS(void)
{
    /* Native memory is directly addressable; there is no EMS page frame. */
}

void MM_GetPtr(memptr *baseptr, unsigned long size)
{
    void *address;
    size_t allocation_size = (size_t)size;

    if (baseptr == NULL) {
        Quit("Native memory manager: null pointer handle");
        return;
    }

    if (allocation_size == 0) {
        allocation_size = 1;
    }

    address = allocate_with_purge(allocation_size);
    if (address == NULL) {
        *baseptr = NULL;
        mmerror = true;
        if (bomb_on_error) {
            Quit("Native memory manager: out of memory");
        }
        return;
    }

    if (!register_allocation(address, allocation_size, baseptr)) {
        mmerror = true;
        return;
    }

    *baseptr = address;
    mmerror = false;
}

void MM_FreePtr(memptr *baseptr)
{
    NativeAllocation **cursor;

    if (baseptr == NULL || *baseptr == NULL) {
        return;
    }

    cursor = &allocations;
    while (*cursor != NULL) {
        if ((*cursor)->address == *baseptr) {
            release_entry(cursor);
            *baseptr = NULL;
            return;
        }
        cursor = &(*cursor)->next;
    }

    /* All memory returned by MM_GetPtr is tracked. Do not double free. */
    Quit("Native memory manager: untracked pointer released");
}

void MM_SetPurge(memptr *baseptr, int purge)
{
    NativeAllocation *entry;

    if (baseptr == NULL || *baseptr == NULL) {
        return;
    }

    entry = find_allocation(*baseptr);
    if (entry == NULL) {
        return;
    }

    if (purge < 0) purge = 0;
    if (purge > 3) purge = 3;
    entry->purge_level = (unsigned)purge;
    entry->handle = purge > 0 ? baseptr : NULL;
    entry->last_touched = ++allocation_clock;
}

void MM_SetLock(memptr *baseptr, boolean locked)
{
    NativeAllocation *entry;

    if (baseptr == NULL || *baseptr == NULL) {
        return;
    }

    entry = find_allocation(*baseptr);
    if (entry == NULL) {
        return;
    }

    entry->locked = locked;
    entry->last_touched = ++allocation_clock;
}

void MM_SortMem(void)
{
    /* No paragraph heap to compact. */
    if (beforesort != NULL) {
        beforesort();
    }
    if (aftersort != NULL) {
        aftersort();
    }
}

void MM_ShowMemory(void)
{
    NativeAllocation *entry;
    size_t allocation_count = 0;
    size_t allocated_bytes = 0;
    size_t purgeable_bytes = 0;

    for (entry = allocations; entry != NULL; entry = entry->next) {
        ++allocation_count;
        allocated_bytes += entry->size;
        if (!entry->locked && entry->purge_level != 0) {
            purgeable_bytes += entry->size;
        }
    }

    printf("Native heap: %zu allocations, %zu bytes (%zu purgeable)\n",
        allocation_count, allocated_bytes, purgeable_bytes);
}

long MM_UnusedMemory(void)
{
    return LONG_MAX;
}

long MM_TotalFree(void)
{
    return LONG_MAX;
}

void MM_BombOnError(boolean bomb)
{
    bomb_on_error = bomb;
}

void MML_UseSpace(unsigned segstart, unsigned seglength)
{
    (void)segstart;
    (void)seglength;
}
