/*
 * Small cross-platform 32-bit tick counter.
 *
 * The SDL timer thread updates the native game clock while legacy game code
 * polls it in tight loops. This avoids relying on C11 stdatomic support in
 * every MSVC toolset.
 */
#ifndef WOLF3D_NATIVE_TICK_H
#define WOLF3D_NATIVE_TICK_H

#include <stdint.h>

#if defined(_MSC_VER)
typedef volatile long WolfNativeTick;
#else
typedef volatile uint32_t WolfNativeTick;
#endif

uint32_t wolf_native_tick_load(const WolfNativeTick *tick);
void wolf_native_tick_store(WolfNativeTick *tick, uint32_t value);
uint32_t wolf_native_tick_increment(WolfNativeTick *tick);

#endif
