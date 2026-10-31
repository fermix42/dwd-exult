// Executes the existing local original-driver reference without copying its
// recovered image/data into this repository or altering the reference project.
#include "../../../ultima7-modern/game/blackgate/sound/drv/adlibdrv.cpp"
#include <array>
static std::array<uint8_t,256> observed{};
static uint8_t address=0;
void plat_opl_write(uint16_t port,uint8_t value) {
	if(port&1) observed[address]=value; else address=value;
}
namespace NativeOpl3 {
int16_t Init(void*,int) { return 0; }
void SetPort(int16_t) {}
void Shutdown() {}
void Note(int16_t,int16_t,int16_t) {}
void Pitch(int16_t,int16_t,int16_t) {}
void Control(int16_t,int16_t,int16_t) {}
void Program(int16_t,int16_t) {}
void Timer() {}
}
extern "C" void originalOpl2Reset(void* bank) {
	observed.fill(0); ResetBgSoundAdlibdrvGlobals(); BgSound::AdlibInit(bank);
}
extern "C" void originalOpl2Event(unsigned channel,unsigned type,unsigned a,unsigned b) {
	if(type==0x90) BgSound::AdlibNote(channel+1,a,b);
	else if(type==0x80) BgSound::AdlibNote(channel+1,a,0);
	else if(type==0xc0) BgSound::AdlibProgram(channel+1,a);
	else if(type==0xb0) BgSound::AdlibControl(channel+1,a,b);
	else if(type==0xe0) BgSound::AdlibPitch(channel+1,a,b);
}
extern "C" void originalOpl2Tick() { BgSound::AdlibTimer(); }
extern "C" const uint8_t* originalOpl2Registers() { return observed.data(); }
extern "C" int originalOpl2State(unsigned field,unsigned index) { return BgSound::Get(field,index); }
