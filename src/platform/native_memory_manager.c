/*
 * Native heap implementation of the original ID memory-manager API.
 *
 * The DOS manager exposed relocatable 16-byte paragraphs backed by
 * conventional/EMS/XMS memory. Native pointers are stable malloc allocations,
 * so compaction, EMS mapping and purge levels are deliberately unnecessary.
 * The API surface remains compatible with the original gameplay sources.
 */
#include "ID_HEADS.H"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct NativeAllocation {
    void *address;
    size_t size;
    struct NativeAllocation *next;
} NativeAllocation;

static NativeAllocation *allocations;
static boolean memory_started;
static boolean bomb_on_error = true;

mminfotype mminfo;
memptr bufferseg;
boolean mmerror;
void (*beforesort)(void);
void (*aftersort)(void);

static void register_allocation(void *address, size_t size)
{
    NativeAllocation *entry = (NativeAllocation *)malloc(sizeof(*entry));

    if (entry == NULL) {
        free(address);
        Quit("Native memory manager: allocation tracking failed");
        return;
    }

    entry->address = address;
    entry->size = size;
    entry->next = allocations;
    allocations = entry;
}

static boolean release_allocation(void *address)
{
    NativeAllocation **cursor = &allocations;

    while (*cursor != NULL) {
        NativeAllocation *entry = *cursor;
        if (entry->address == address) {
            *cursor = entry->next;
            free(entry->address);
            free(entry);
            return true;
        }
        cursor = &entry->next;
    }

    return false;
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

    address = malloc(allocation_size);
    if (address == NULL) {
        *baseptr = NULL;
        mmerror = true;
        if (bomb_on_error) {
            Quit("Native memory manager: out of memory");
        }
        return;
    }

    register_allocation(address, allocation_size);
    *baseptr = address;
    mmerror = false;
}

void MM_FreePtr(memptr *baseptr)
{
    if (baseptr == NULL || *baseptr == NULL) {
        return;
    }

    if (!release_allocation(*baseptr)) {
        /* All memory returned by MM_GetPtr is tracked. Do not double free. */
        Quit("Native memory manager: untracked pointer released");
        return;
    }

    *baseptr = NULL;
}

void MM_SetPurge(memptr *baseptr, int purge)
{
    (void)baseptr;
    (void)purge;
    /* Stable native allocations are retained until explicit release. */
}

void MM_SetLock(memptr *baseptr, boolean locked)
{
    (void)baseptr;
    (void)locked;
    /* A stable native allocation cannot be moved or purged implicitly. */
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

    for (entry = allocations; entry != NULL; entry = entry->next) {
        ++allocation_count;
        allocated_bytes += entry->size;
    }

    printf("Native heap: %zu allocations, %zu bytes\n",
        allocation_count, allocated_bytes);
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
