#pragma once
#include <cstdint>
inline bool plat_opl3_enabled() { return false; }
inline bool plat_opl_available() { return true; }
inline uint8_t plat_opl_read(uint16_t) { return 0; }
inline uint8_t plat_opl_pan_value(uint8_t pan) { return pan; }
inline void plat_sound_lock() {}
inline void plat_sound_unlock() {}
void plat_opl_write(uint16_t port,uint8_t value);
