// Exult OPL3 music adapter. GPL-2.0-or-later.
// The unmodified included emulator is LGPL-2.1-or-later; see nuked-opl3/LICENSE.
#include "pent_include.h"
#include "NukedOpl3.h"
#define OPL_ENABLE_STEREOEXT 1
#include "nuked-opl3/opl3.c"
#include <algorithm>
#include <new>

namespace NukedStereo {
struct Chip {
    opl3_chip core{};
    unsigned address=0;
};
Chip* makeAdLibOPL(unsigned rate) {
    if (!rate) return nullptr;
    auto* chip=new (std::nothrow) Chip;
    if (!chip) return nullptr;
    OPL3_Reset(&chip->core,rate);
    OPL3_WriteReg(&chip->core,0x105,3); // OPL3 and fine-position stereo extension.
    OPL3_WriteReg(&chip->core,0x104,0); // Existing two-operator arrangement only.
    for(unsigned voice=0;voice<18;++voice) {
        OPL3_WriteReg(&chip->core,0xc0+(voice%9)+((voice/9)<<8),0x30);
        OPL3_WriteReg(&chip->core,0xd0+(voice%9)+((voice/9)<<8),128);
    }
    return chip;
}
void OPLDestroy(Chip* chip) { delete chip; }
void OPLWrite(Chip* chip,unsigned port,unsigned value) {
    if (!(port&1)) { chip->address=(value&255)|((port&2)?0x100:0); return; }
    // Preserve FM algorithm/feedback while enabling both output buses.
    if ((chip->address&255)>=0xc0 && (chip->address&255)<=0xc8) value=(value&15)|0x30;
    // Exult applies each MIDI event before rendering its next audio block.
    // Do not render intermediate states of a voice being reprogrammed.
    OPL3_WriteReg(&chip->core,chip->address,value&255);
}
void OPLSetPan(Chip* chip,unsigned voice,int pan) {
    if(voice>=18) return;
    pan=std::clamp(pan,0,127);
    // Original Roland pan direction, endpoints and exact central position.
    unsigned position=pan<=64 ? 255-(pan*127+32)/64 : ((127-pan)*128+31)/63;
    OPL3_WriteReg(&chip->core,0xd0+(voice%9)+((voice/9)<<8),position);
}
void YM3812UpdateOne_Stereo(Chip* chip,sint16* output,unsigned frames) {
    OPL3_GenerateStream(&chip->core,output,frames);
}
void YM3812UpdateOne_Mono(Chip* chip,sint16* output,unsigned frames) {
    sint16 stereo[2];
    for(unsigned i=0;i<frames;++i) {
        OPL3_GenerateResampled(&chip->core,stereo);
        output[i]=sint16((int(stereo[0])+stereo[1])/2);
    }
}
}

