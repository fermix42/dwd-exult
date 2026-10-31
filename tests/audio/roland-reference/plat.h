#pragma once
#include <cstdint>
inline bool plat_midi_available(){return true;}
inline void plat_sound_lock(){}
inline void plat_sound_unlock(){}
void plat_midi_send(const uint8_t*,int16_t);
