// Exult OPL3 music adapter. GPL-2.0-or-later.
#ifndef EXULT_NUKED_STEREO_H
#define EXULT_NUKED_STEREO_H
#include "common_types.h"
namespace NukedStereo {
struct Chip;
Chip* makeAdLibOPL(unsigned rate);
void OPLDestroy(Chip* chip);
void OPLWrite(Chip* chip, unsigned port, unsigned value);
void OPLSetPan(Chip* chip, unsigned voice, int pan);
void YM3812UpdateOne_Stereo(Chip* chip, sint16* output, unsigned frames);
void YM3812UpdateOne_Mono(Chip* chip, sint16* output, unsigned frames);
}
#endif
