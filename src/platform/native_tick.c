#include "native_tick.h"

#if defined(_MSC_VER)
#include <intrin.h>

uint32_t wolf_native_tick_load(const WolfNativeTick *tick)
{
    return (uint32_t)_InterlockedCompareExchange((volatile long *)tick, 0, 0);
}

void wolf_native_tick_store(WolfNativeTick *tick, uint32_t value)
{
    (void)_InterlockedExchange((volatile long *)tick, (long)value);
}

uint32_t wolf_native_tick_increment(WolfNativeTick *tick)
{
    return (uint32_t)_InterlockedIncrement((volatile long *)tick);
}
#else
uint32_t wolf_native_tick_load(const WolfNativeTick *tick)
{
    return __atomic_load_n(tick, __ATOMIC_RELAXED);
}

void wolf_native_tick_store(WolfNativeTick *tick, uint32_t value)
{
    __atomic_store_n(tick, value, __ATOMIC_RELAXED);
}

uint32_t wolf_native_tick_increment(WolfNativeTick *tick)
{
    return __atomic_add_fetch(tick, 1u, __ATOMIC_RELAXED);
}
#endif
