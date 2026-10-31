#pragma once
#include <cstdint>
inline bool plat_opl3_enabled() { return false; }
inline void plat_sound_lock() {}
inline void plat_sound_unlock() {}
void plat_opl_write(uint16_t port,uint8_t value);
