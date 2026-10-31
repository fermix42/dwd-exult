/* Copyright (C) 2026 The Exult Team
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Independently expressed Black Gate driver behavior. Original executable
 * data is validated and read at runtime; it is never redistributed here.
 */
#include "pent_include.h"
#ifdef USE_FMOPL_MIDI
#include "OriginalOpl2MidiDriver.h"
#include "databuf.h"
#include "sha1/sha1.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>

const MidiDriver::MidiDriverDesc OriginalOpl2MidiDriver::desc("OriginalOPL2", createInstance);
namespace {
int word(const std::array<uint8,48>& t, unsigned at) {
	return sint16(uint16(t[at]) | (uint16(t[at+1])<<8));
}
int sar(int value, unsigned count) {
	count&=31;
	return count>15 ? (sint16(value)<0?-1:0) : sint16(value)>>count;
}
std::vector<uint8> originalData(const char* path, unsigned size, const char* hash) {
	IFileDataSource source(path);
	if (!source.good() || source.getSize()!=size)
		throw std::runtime_error(std::string("Original OPL2: unsupported resource ")+path);
	std::vector<uint8> data(size);
	source.read(data.data(),size);
	if (std::string(sha1::toHexString(sha1::calc(data.data(),int(size))).data())!=hash)
		throw std::runtime_error(std::string("Original OPL2: unrecognized resource ")+path);
	return data;
}
void eraseVoice(std::deque<unsigned>& list, unsigned v) {
	list.erase(std::remove(list.begin(),list.end(),v),list.end());
}
}
int OriginalOpl2MidiDriver::open() {
	chip=FMOpl_Pentagram::makeAdLibOPL(sample_rate);
	reset();
	return chip?0:1;
}
void OriginalOpl2MidiDriver::write(unsigned reg, uint8 value) {
	if (reg>=256 || !chip) return;
	registers[reg]=value;
	FMOpl_Pentagram::OPLWrite(chip,0,reg);
	FMOpl_Pentagram::OPLWrite(chip,1,value);
}
void OriginalOpl2MidiDriver::reset() {
	ready=false; ending=false; clock=0; melodic=9; rhythm=0;
	slots={}; owners.fill(-1);
	voices={}; programs.fill(-1); bends.fill(0); volumes.fill(256); vibrato.fill(0);
	free.clear(); busy.clear();
	for(unsigned v=0;v<9;++v) { free.push_back(v); write(0xa0+v,0); write(0xb0+v,0); }
	write(1,32); write(0xbd,0); write(8,0);
}
void OriginalOpl2MidiDriver::close() {
	reset(); bank.clear(); legacy.reset(); ail_patches.clear();
	if(chip) FMOpl_Pentagram::OPLDestroy(chip);
	chip=nullptr;
}
void OriginalOpl2MidiDriver::loadClassicBank(IDataSource* source) {
	auto driver=originalData(si_menu?"<STATIC>/mainmenu.drv":"<STATIC>/u7strax.drv",5379,"5aae6e31b61ee26b38e8d21a020bf7a698da86c7");
	for(unsigned i=0;i<13;++i) frequencies[i]=driver[1351+0x215+i*2]|(uint16(driver[1352+0x215+i*2])<<8);
	for(unsigned i=0;i<24;++i) {
		unsigned at=1351+0x215-22+i*2;
		frequency_window[i]=driver[at]|(uint16(driver[at+1])<<8);
	}
	if(!source) throw std::runtime_error("Original OPL2: missing FM bank");
	source->seek(0);
	unsigned count=source->read1();
	if(!count || source->getAvail()!=count*48) throw std::runtime_error("Original OPL2: invalid 48-byte FM bank");
	bank.resize(count);
	for(auto& timbre:bank) source->read(timbre.data(),48);
	std::vector<uint8> executable;
	if(si_menu) {
		executable=originalData("<STATIC>/../mainmenu.exe",132080,"0638d773ad604acc9b1f7763807052279338ef0e");
		std::copy_n(executable.begin()+115599,88,percussion_channel.begin());
		std::copy_n(executable.begin()+115687,88,percussion_key.begin());
		constexpr unsigned offsets[]={83236,83245,83253,83262,83270,83279,83288,83297,83306,83315,83324,83333,83342,83351,83360,83369,83378,83387,83396,83405,83414};
		for(unsigned c=0;c<21;++c) program(c+9,executable[offsets[c]]);
	} else if(intro) {
		executable=originalData("<STATIC>/../intro.exe",116068,"c8ea335ef7392865389c55f13b8ef1102fc4d427");
		std::copy_n(executable.begin()+111619,88,percussion_channel.begin());
		std::copy_n(executable.begin()+111707,88,percussion_key.begin());
		constexpr unsigned offsets[]={59199,59208,59216,59225,59233,59242,59251,59260,59269,59278,59287,59296,59305,59314,59323,59332,59341,59350,59359,59368,59377};
		for(unsigned c=0;c<21;++c) program(c+9,executable[offsets[c]]);
	} else {
		executable=originalData("<STATIC>/../u7.exe",689248,"5f2887c84474cbbd0752634925d8dd5d6425e981");
		std::copy_n(executable.begin()+309755,88,percussion_channel.begin());
		std::copy_n(executable.begin()+309843,88,percussion_key.begin());
		for(unsigned c=0;c<21;++c) program(c+9,executable[250833+12*c]);
	}
	ready=true;
}
void OriginalOpl2MidiDriver::loadTimbreLibrary(IDataSource* source,TimbreLibraryType type) {
	reset();
	if(serpent) {
		legacy.reset();
		try { loadSerpentBank(source,type); }
		catch(const std::exception& error) { reset(); bank.clear(); std::cerr<<error.what()<<"; Serpent Isle historical music unavailable\n"; }
		return;
	}
	if(!bg) {
		if(!legacy) legacy=std::make_unique<Legacy>(sample_rate,stereo);
		legacy->loadTimbreLibrary(source,type);
		std::cerr<<"Original OPL2: generic launcher score uses legacy FMOpl playback\n";
		return;
	}
	legacy.reset();
	try {
		if(type==TIMBRE_LIBRARY_XMIDI_AD) loadEndingBank(source);
		else if(type==TIMBRE_LIBRARY_U7VOICE_AD) loadClassicBank(source);
		else throw std::runtime_error("Original OPL2: unsupported music bank type");
	} catch(const std::exception& error) {
		reset(); bank.clear(); std::cerr<<error.what()<<"; historical music unavailable\n";
	}
}
unsigned OriginalOpl2MidiDriver::modulator(unsigned v) const {
	if(melodic==6 && v>=7) return v==7?20:v==8?18:v==9?21:17;
	return (v/3)*8+v%3;
}
unsigned OriginalOpl2MidiDriver::carrier(unsigned v) const {
	if(melodic==6 && v>=7) return modulator(v);
	return modulator(v)+3;
}
void OriginalOpl2MidiDriver::loadTimbre(unsigned v,unsigned index) {
	const auto& t=bank[index];
	for(unsigned op=0;op<2;++op) {
		if(op && melodic<9 && t[11]>=7) break;
		unsigned slot=op?carrier(v):modulator(v), at=op?5:0;
		write(0x20+slot,t[at]); write(0x40+slot,t[at+1]);
		write(0x60+slot,t[at+2]); write(0x80+slot,t[at+3]); write(0xe0+slot,t[at+4]);
	}
	if(melodic==9 || t[11]<7) write(0xc0+v,t[10]);
}
void OriginalOpl2MidiDriver::program(unsigned ch,unsigned number) {
	if(bank.empty() || ch>=32) return;
	for(unsigned v=0;v<11;++v) if(voices[v].channel==int(ch)) {
		note(ch,voices[v].key,0); voices[v].channel=-1; voices[v].timbre=-1;
	}
	unsigned found=0;
	for(unsigned i=0;i<bank.size();++i) if(bank[i][47]==number) { found=i; break; }
	programs[ch]=found; vibrato[ch]=bank[found][17];
	if(bank[found][11] && melodic==9) {
		write(0xa6,0); write(0xb6,0); write(0xa7,0); write(0xb7,10); write(0xa8,84); write(0xb8,9);
		melodic=6;
		for(unsigned v=6;v<9;++v) { eraseVoice(free,v); eraseVoice(busy,v); }
		rhythm=32; write(0xbd,rhythm);
	}
}
void OriginalOpl2MidiDriver::frequency(unsigned index) {
	const auto& v=voices[index];
	if(v.timbre<0 || v.channel<0) return;
	const auto& t=bank[v.timbre];
	int pitch=sint16(v.pitch+v.slide+v.vibrato+bends[v.channel]+word(t,36));
	int semitone=pitch>>8, step=(semitone+6)%12;
	int f=frequency_window[step+11];
	if(pitch&255) f+=sint16((frequency_window[12+(semitone-18)%12]-f)*(pitch&255))>>8;
	unsigned packed=(std::clamp((semitone+6)/12-2,0,7)<<10)+f;
	if(v.stage>1 && !t[11]) packed+=0x2000;
	unsigned reg=index>8?17-index:index;
	write(0xa0+reg,uint8(packed)); write(0xb0+reg,uint8(packed>>8));
}
void OriginalOpl2MidiDriver::note(unsigned ch,unsigned key,unsigned velocity) {
	if(ch>=32 || programs[ch]<0) return;
	for(unsigned index=programs[ch];index<bank.size();++index) {
		const auto& t=bank[index]; int selected=-1;
		if(melodic<9 && t[11]) {
			if(t[11]<6 || t[11]>10) return;
			selected=t[11]; rhythm&=~(16>>(selected-6)); write(0xbd,rhythm);
		} else if(velocity) {
			if(!free.empty()) { selected=free.front(); free.pop_front(); }
			else if(index==unsigned(programs[ch]) && !busy.empty()) {
				selected=busy.front(); busy.pop_front(); write(0xa0+selected,0); write(0xb0+selected,0);
			}
			if(selected>=0) { busy.push_back(selected); voices[selected].stage=2; voices[selected].key=key; }
		} else for(unsigned v=0;v<melodic;++v) {
			const auto& state=voices[v];
			if(state.stage>1 && state.channel==int(ch) && state.key==int(key) && state.timbre==int(index)) {
				selected=v; voices[v].stage=0; eraseVoice(busy,v); free.push_back(v); break;
			}
		}
		if(selected>=0) {
			auto& v=voices[selected];
			if(v.channel!=int(ch) || v.timbre!=int(index)) { loadTimbre(selected,index); v.channel=ch; v.timbre=index; }
			if(velocity) {
				v.phase=0; v.slide=word(t,18); v.vibrato=0;
				// Original rhythm slots do not participate in the melodic state list.
				// Original writes carrier attenuation before modulator attenuation;
				// the order matters for the single-operator rhythm voices.
				for(unsigned pass=0;pass<2;++pass) {
					unsigned op=1-pass;
					unsigned at=op?5:0,touch=t[op?12:13];
					if(!touch && volumes[ch]>=256) continue;
					int level=sar(63-int(velocity),7-touch)+(t[at+1]&63),scaled=level;
					if(op || (t[10]&1)) { scaled=sar(volumes[ch]*(63-level),8); level=63-scaled; }
					if(op?level>63:scaled>63) level=63;
					level=std::max(0,level);
					write(0x40+(op?carrier(selected):modulator(selected)),uint8((t[at+1]&192)+level));
				}
			}
			int pitch=sint16(uint16(uint8(key-60))<<8);
			pitch=sint8(t[39])<0?sar(-pitch,uint8(~t[39])):sar(pitch,t[39]);
			v.pitch=sint16(pitch+(60<<8));
			if(!t[11] || selected==6 || (!velocity && !(selected>int(melodic) && selected>6))) frequency(selected);
			if(t[11]) { if(velocity) rhythm|=16>>(selected-6); write(0xbd,rhythm); }
		}
		if(!t[38]) break;
	}
}
void OriginalOpl2MidiDriver::control(unsigned ch,unsigned number,unsigned value) {
	if(ch==9) for(unsigned c=10;c<30;++c) control(c,number,value);
	if(number==1 && programs[ch]>=0) vibrato[ch]=sar(bank[programs[ch]][15]*value,7)+bank[programs[ch]][17];
	else if(number==7) volumes[ch]=value+128;
	else if(number==121) { control(ch,1,0); control(ch,7,127); bends[ch]=0;
		for(unsigned v=0;v<melodic;++v) if(voices[v].channel==int(ch) && voices[v].stage>1) frequency(v);
	} else if(number==123) {
		bool other_rhythm=false;
		for(unsigned v=0;v<11;++v) if(voices[v].stage>1) {
			if(voices[v].channel==int(ch)) note(ch,voices[v].key,0);
			else if(v>=melodic) other_rhythm=true;
		}
		if(!other_rhythm && melodic<9) { write(0xbd,0); melodic=9; free.push_front(8); free.push_front(7); free.push_front(6); }
	}
}
void OriginalOpl2MidiDriver::send(uint32 message) {
	if(legacy) { legacy->send(message); return; }
	if(!ready) return;
	if(ending) { endingSend(message); return; }
	unsigned ch=message&15,type=message&240,a=(message>>8)&127,b=(message>>16)&127;
	if(type==0x90 || type==0x80) {
		if(ch==9) { if(a<88 && percussion_channel[a]) note(percussion_channel[a]-1,percussion_key[a],type==0x80?0:b); }
		else note(ch,a,type==0x80?0:b);
	} else if(type==0xc0 && ch!=9) program(ch,a);
	else if(type==0xb0) control(ch,a,b);
	else if(type==0xe0 && programs[ch]>=0) {
		bends[ch]=((int((b<<7)|a)-8192)*bank[programs[ch]][14])>>8;
		for(unsigned v=0;v<melodic;++v) if(voices[v].channel==int(ch) && voices[v].stage>1) frequency(v);
	}
}
void OriginalOpl2MidiDriver::tick() {
	for(unsigned index=0;index<melodic;++index) {
		auto& v=voices[index]; if(v.timbre<0 || v.channel<0) continue;
		const auto& t=bank[v.timbre];
		unsigned at=v.stage==0?32:v.stage==2?20:v.stage==3?24:v.stage==4?28:0;
		bool changed=false;
		if(at) {
			int rate=word(t,at),target=word(t,at+2);
			if(uint16(std::abs(target-v.slide))<uint16(rate)) { v.slide=target; ++v.stage; }
			else v.slide=sint16(v.slide+(v.slide<target?rate:-rate));
			changed=true;
		}
		unsigned rate=programs[v.channel]<0?0:bank[programs[v.channel]][16];
		if(rate) {
			v.phase+=rate; int phase=sint8(v.phase);
			if(phase>63 || phase<-64) phase=sint8(128-phase);
			v.vibrato=sar(vibrato[v.channel]*phase,4); changed=true;
		}
		if(changed || word(t,36)) frequency(index);
	}
}
void OriginalOpl2MidiDriver::lowLevelProduceSamples(sint16* samples,uint32 frames) {
	if(legacy) { legacy->lowLevelProduceSamples(samples,frames); return; }
	for(uint32 i=0;i<frames;++i) {
		clock+=60;
		if(clock>=sample_rate) { clock-=sample_rate; if(ready) { if(ending) endingTick(); else tick(); } }
		sint16 mono=0;
		if(chip) FMOpl_Pentagram::YM3812UpdateOne_Mono(chip,&mono,1);
		mono=sint16(int(mono)*LowLevelMidiDriver::getGlobalVolume()/100);
		samples[i*(stereo?2:1)]=mono;
		if(stereo) samples[i*2+1]=mono;
	}
}
#endif
