// Execute the read-only local AIL source as an independent register/slot oracle.
// The small arena exists only to satisfy that source's timbre-cache addressing.
// Neither the reference implementation nor recovered tables are copied here.
#define plat_opl_write serpentReferenceWrite
#include "../../../ultima7-modern/game/sound/drv/ailopl.cpp"
#undef plat_opl_write
#include <array>
#include <vector>

static std::array<uint8_t,131088> reference_memory{};
extern "C" {
uint8_t* LinearBase=reference_memory.data();
uint32_t LinearSize=uint32_t(reference_memory.size());
uint32_t PointerToLinear(void* p) { return uint32_t(static_cast<uint8_t*>(p)-LinearBase); }
}
static std::array<uint8_t,256> observed{};
static uint8_t address=0;
static std::vector<uint32_t> trace;
void serpentReferenceWrite(uint16_t port,uint8_t value) {
	if(port&1) { observed[address]=value; trace.push_back(unsigned(address)|(unsigned(value)<<8)); }
	else address=value;
}
extern "C" void serpentRefReset(bool old,const uint8_t* driver) {
	ResetAiloplGlobals(); reference_memory.fill(0); observed.fill(0); trace.clear();
	OplSetPort(0x388,old); ResetSynth(); InitSynth(); DefineCache(reference_memory.data()+16,3584);
	for(unsigned channel=0;channel<16;++channel) {
		MidiVol[channel]=127; MidiExpress[channel]=127; MidiPitchH[channel]=64;
	}
	unsigned at=old?10342:10960;
	for(unsigned channel=1;channel<=9;++channel) if(driver[at+channel-1]<128)
		SendMidi(uint8_t(0xc0|channel),driver[at+channel-1],0);
}
extern "C" void serpentRefInstall(unsigned bank,unsigned patch,const uint8_t* bytes) { InstallTimbre(uint8_t(bank),uint8_t(patch),bytes); }
extern "C" void serpentRefEvent(uint32_t m) { SendMidi(uint8_t(m),uint8_t(m>>8),uint8_t(m>>16)); }
extern "C" void serpentRefTick() { ServeSynth(); ServeSynth(); }
extern "C" const uint8_t* serpentRefRegisters() { return observed.data(); }
extern "C" void serpentRefClearTrace() { trace.clear(); }
extern "C" unsigned serpentRefTraceCount() { return unsigned(trace.size()); }
extern "C" uint32_t serpentRefTraceAt(unsigned i) { return trace.at(i); }
extern "C" unsigned serpentRefSlot(unsigned i,unsigned field) {
	switch(field) {
	case 0:return SStatus[i]; case 1:return SVoice[i]; case 2:return SChannel[i]; case 3:return SKeynum[i];
	case 4:return SType[i]; case 5:return SDuration[i]; case 6:return SNote[i]; case 7:return STranspose[i];
	case 9:for(unsigned p=0;p<MAX_TIMBS;++p) if((TimbAttribs[p]&TIMB_IN_USE) && uint16_t(CacheOffset+TimbOffsets[p])==STimbre[i]) return (unsigned(TimbBank[p])<<8)|TimbNum[p]; return 65535;
	default:return 0;
	}
}
