// Execute read-only local references. No recovered driver code or asset bytes are copied.
#include <vector>
#include <cstdint>
struct ReferenceMessage {std::vector<uint8_t> bytes;unsigned wait=0;};
static std::vector<ReferenceMessage> referenceMessages;
void plat_midi_send(const uint8_t* bytes,int16_t count){referenceMessages.push_back({std::vector<uint8_t>(bytes,bytes+count),0});}
#include "../../../ultima7-modern/game/shared/mt32drv.cpp"
#include "../../../ultima7-modern/game/sound/drv/ailmt32.cpp"
void XmidiWait(uint16_t retraces){if(!referenceMessages.empty())referenceMessages.back().wait+=retraces;}
extern "C" void rolandRefClear(){referenceMessages.clear();}
extern "C" unsigned rolandRefCount(){return unsigned(referenceMessages.size());}
extern "C" unsigned rolandRefSize(unsigned i){return unsigned(referenceMessages.at(i).bytes.size());}
extern "C" unsigned rolandRefByte(unsigned i,unsigned j){return referenceMessages.at(i).bytes.at(j);}
extern "C" unsigned rolandRefWait(unsigned i){return referenceMessages.at(i).wait;}
extern "C" void rolandRefClassic(void* bank){ResetMt32drvGlobals();referenceMessages.clear();Shared::Mt32Init(bank);uint8_t volume=70;Shared::Mt32Sysex(0x100016,1,&volume);}
extern "C" void rolandRefAil(const uint8_t* driver,bool newer){
    ResetAilmt32Globals();referenceMessages.clear();ResetSynth();InitSynth();
    unsigned controls=newer?4567:4549,defaults=newer?4576:4558,programs=newer?4585:4567;
    for(unsigned i=0;i<9;++i)for(unsigned c=1;c<=9;++c)SendMidi(uint8_t(0xb0|c),driver[controls+i],driver[defaults+i]);
    XmidiWait(10);
    for(unsigned c=1;c<=9;++c){SendMidi(uint8_t(0xe0|c),0,64);if(driver[programs+c-1]<128)SendMidi(uint8_t(0xc0|c),driver[programs+c-1],0);}
    XmidiWait(10);
}
extern "C" void rolandRefEvent(uint32_t message,bool classic){
    uint8_t s=uint8_t(message),a=uint8_t(message>>8),b=uint8_t(message>>16);
    if(!classic){SendMidi(s,a,b);return;}
    unsigned channel=(s&15)+1;
    switch(s&240){case 0x90:Shared::Mt32Note(channel,a,b);break;case 0x80:Shared::Mt32Note(channel,a,0);break;case 0xb0:Shared::Mt32Control(channel,a,b);break;case 0xc0:Shared::Mt32Program(channel,a);break;case 0xe0:Shared::Mt32Pitch(channel,a,b);break;default:break;}
}
extern "C" void rolandRefInstall(unsigned bank,unsigned program,const uint8_t* timbre){InstallTimbre(uint8_t(bank),uint8_t(program),timbre);}

extern "C" bool rolandRefHasTimbre(unsigned bank,unsigned program){return IndexTimbre(uint8_t(bank),uint8_t(program))>=0;}
