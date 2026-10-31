/* Copyright (C) 2026 The Exult Team
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Black Gate ending's 1991 AIL policy, including original TVFX parameters.
 */
#include "pent_include.h"
#ifdef USE_FMOPL_MIDI
#include "OriginalOpl2MidiDriver.h"
#include "databuf.h"
#include "sha1/sha1.h"
#include <algorithm>
#include <stdexcept>
namespace {
constexpr uint8 all=0xf9, amplitude=0x40, frequency_update=1;
unsigned scaled(unsigned x,unsigned y) {
	unsigned result=uint16((x*y)<<1)>>8;
	return result?result+1:0;
}
}
void OriginalOpl2MidiDriver::loadEndingBank(IDataSource* source) {
	IFileDataSource driver("<STATIC>/adlib.adv");
	if(!driver.good() || driver.getSize()!=16144) throw std::runtime_error("Original OPL2: unsupported ending ADLIB.ADV");
	std::vector<uint8> bytes(driver.getSize()); driver.read(bytes.data(),bytes.size());
	if(std::string(sha1::toHexString(sha1::calc(bytes.data(),int(bytes.size()))).data())!="3f663b98310b7c938c47f4cec580a0b36359979d")
		throw std::runtime_error("Original OPL2: unrecognized ending ADLIB.ADV");
	for(unsigned i=0;i<192;++i) ail_frequencies[i]=bytes[2720+i*2]|(uint16(bytes[2721+i*2])<<8);
	loadAilBank(source);
}
void OriginalOpl2MidiDriver::loadAilBank(IDataSource* source) {
	if(!source) throw std::runtime_error("Original OPL2: missing ending XMIDI.AD");
	ail_patches.clear();
	for(unsigned i=0;i*6+6<=source->getSize();++i) {
		source->seek(i*6); unsigned patch=source->read1(),bank=source->read1(),offset=source->read4();
		if(patch==255 || bank==255) break;
		if(offset+2>source->getSize()) throw std::runtime_error("Original OPL2: invalid ending bank offset");
		source->seek(offset); unsigned size=source->read2();
		if(size<14 || offset+size>source->getSize()) throw std::runtime_error("Original OPL2: invalid ending timbre size");
		std::vector<uint8> timbre(size); source->seek(offset); source->read(timbre.data(),size);
		if(size!=14 && (size<54 || (timbre[3]!=1 && timbre[3]!=2)))
			throw std::runtime_error("Original OPL2: unsupported ending timbre type");
		ail_patches.emplace((bank<<8)|patch,std::move(timbre));
	}
	slots={}; ail_channels={}; owners.fill(-1); priority_clock=0; volume_update=0;
	ending=ready=true;
}
unsigned OriginalOpl2MidiDriver::ailByte(const AilSlot& slot,unsigned at) const {
	auto found=ail_patches.find(slot.patch);
	return found!=ail_patches.end() && at<found->second.size()?found->second[at]:0;
}
uint16 OriginalOpl2MidiDriver::ailWord(const AilSlot& slot,unsigned at) const {
	return uint16(ailByte(slot,at)|(ailByte(slot,uint16(at+1))<<8));
}
void OriginalOpl2MidiDriver::ailUpdate(unsigned index) {
	auto& s=slots[index]; if(s.voice<0) return;
	unsigned v=s.voice,m=(v/3)*8+v%3,c=m+3;
	const auto& ch=ail_channels[s.channel];
	auto put=[this](unsigned reg,uint8 value) { if(si_new_ail || registers[reg]!=value) write(reg,value); };
	unsigned gain=scaled(scaled(ch.volume,ch.expression),s.velocity);
	if(s.update&0x80) {
		unsigned vib=ch.modulation>=64?64:0;
		put(0x20+m,uint8((s.value[5]>>12)|vib|s.avekm[0]));
		put(0x20+c,uint8((s.value[6]>>12)|vib|s.avekm[1]));
	}
	if(s.update&amplitude) for(unsigned op=0;op<2;++op) {
		unsigned level=s.value[op+1]>>10;
		if(s.scale&(1<<op)) level=uint8(level*gain/127);
		put(0x40+(op?c:m),uint8((~level&63)|s.ksl[op]));
	}
	if(s.update&0x20) {
		put(0x60+m,s.ad[0]); put(0x60+c,s.ad[1]); put(0x80+m,s.sr[0]); put(0x80+c,s.sr[1]);
	}
	if(s.update&0x10) { put(0xe0+c,uint8(s.value[7])); put(0xe0+m,uint8(s.value[7]>>8)); }
	if(s.update&8) put(0xc0+v,uint8(((s.value[4]>>12)&14)|(si_new_ail?(s.feedback&1):s.feedback)));
	if(s.update&frequency_update) {
		unsigned f;
		if(s.type==2) f=s.value[0]>>6;
		else if(!(s.block&32)) { put(0xb0+v,s.key_shadow&~32); s.update=0; return; }
		else {
			int bend=((int(ch.bend)-8192)>>5)*12;
			int note=int(s.note)+sint8(s.transpose)-24;
			do { note+=12; } while(note<0);
			note+=12; do { note-=12; } while(note>95);
			int pitch=sint16(sint16(bend+(note<<8))+8)>>4;
			pitch-=192; do { pitch+=192; } while(pitch<0);
			pitch+=192; do { pitch-=192; } while(pitch>1535);
			int block=pitch/192-1;
			uint16 raw=ail_frequencies[((pitch>>4)%12)*16+(pitch&15)];
			if(sint16(raw)<0) ++block;
			if(block<0) { ++block; raw=uint16(sint16(raw)>>1); }
			f=(raw&1023)|(uint8(block<<2)<<8);
		}
		put(0xa0+v,uint8(f)); s.key_shadow=uint8((f>>8)|s.block); put(0xb0+v,s.key_shadow);
	}
	s.update=0;
}
void OriginalOpl2MidiDriver::ailRelease(unsigned index) {
	auto& s=slots[index]; if(s.voice<0) return;
	s.block&=~32; s.update|=frequency_update; ailUpdate(index); owners[s.voice]=-1; s.voice=-1;
	if(si_new_ail && s.type==0) s.status=0;
}
void OriginalOpl2MidiDriver::ailPriority() {
	std::array<unsigned,16> priorities{}; unsigned active=0;
	std::array<unsigned,16> count{};
	for(const auto& s:slots) if(s.voice>=0) ++count[s.channel];
	for(unsigned i=0;i<16;++i) if(slots[i].status) {
		++active; auto& s=slots[i]; unsigned p=ail_channels[s.channel].protection>=64?65535:s.value[3];
		priorities[i]=p<count[s.channel]?0:p-count[s.channel];
	}
	if(!si_new_ail && active<=9) return;
	// The original bounds this reassignment pass by the number of active slots.
	while(active--) {
		int high=-1,low=-1; unsigned highest=0,lowest=65535;
		for(unsigned i=0;i<16;++i) if(slots[i].status) {
			if(slots[i].voice<0) { if(priorities[i]>=highest) { highest=priorities[i]; high=i; } }
			else if(priorities[i]<=lowest) { lowest=priorities[i]; low=i; }
		}
		if(high<0 || low<0 || highest<lowest || (si_new_ail && !highest)) return;
		int voice=slots[low].voice; ailRelease(low);
		if(slots[low].type==0) slots[low].status=0;
		slots[high].voice=voice; owners[voice]=high; slots[high].update=all; ailUpdate(high);
	}
}
void OriginalOpl2MidiDriver::ailAssign(unsigned index) {
	for(unsigned attempt=0;attempt<9;++attempt) {
		unsigned v=si_new_ail?(ail_rover+1)%9:attempt;
		if(si_new_ail) ail_rover=v;
		if(owners[v]<0) {
			slots[index].voice=v; owners[v]=index; slots[index].update=all; ailUpdate(index); return;
		}
	}
	ailPriority();
}
void OriginalOpl2MidiDriver::ailSwitch() {
	for(unsigned i=0;i<16;++i) if(slots[i].status && slots[i].voice<0) {
		for(unsigned v=0;v<9;++v) if(owners[v]<0) {
			slots[i].voice=v; owners[v]=i; slots[i].update=all; ailPriority(); return;
		}
		return;
	}
}
void OriginalOpl2MidiDriver::ailPhase(unsigned index) {
	auto& s=slots[index];
	s.feedback=0; s.ksl={}; s.scale=2; s.avekm={32,32}; s.type=ailByte(s,3); s.block=s.type==1?32:40;
	uint16 e0=0xff0f,e1=0xff0f;
	if(uint16(ailWord(s,8)+2)!=54) {
		e0=ailWord(s,s.status==2?58:54); e1=ailWord(s,s.status==2?60:56);
	}
	s.ad={uint8(e1>>8),uint8(e0>>8)}; s.sr={uint8(e1),uint8(e0)};
	if(s.status!=2) s.duration=s.type==1?65535:uint16(ailWord(s,4)+1);
	for(unsigned p=0;p<8;++p) {
		unsigned at=6+6*p;
		if(s.status!=2) s.value[p]=ailWord(s,at);
		s.position[p]=uint16(ailWord(s,at+(s.status==2?4:2))+2); s.count[p]=1; s.step[p]=0;
	}
	s.update=all;
}
void OriginalOpl2MidiDriver::ailFetch(unsigned index,unsigned p) {
	auto& s=slots[index];
	for(unsigned n=0;n<10;++n) {
		unsigned at=s.position[p]; uint16 op=ailWord(s,at),operand=ailWord(s,uint16(at+2));
		if(!op) { s.position[p]+=operand; continue; }
		s.position[p]+=4;
		if(op==65535) { s.value[p]=operand; continue; }
		if(op!=65534) { s.count[p]=op; s.step[p]=operand; return; }
		if(p==5 || p==6) s.avekm[p-5]=uint8(operand);
		else if(p==1 || p==2) s.ksl[p-1]=uint8(operand);
		else if(p==0) { s.block=uint8(operand>>8); if(s.type==1) s.block&=0xe0; }
		else if(p==4) { s.feedback=uint8(operand>>8); s.scale=uint8((operand>>8)|2); }
	}
	s.step[p]=0; s.count[p]=65535;
}
void OriginalOpl2MidiDriver::ailOn(unsigned channel,unsigned key,unsigned velocity) {
	int patch=channel==9?int(0x7f00|key):ail_channels[channel].patch;
	if(ail_patches.find(patch)==ail_patches.end()) return;
	unsigned index=0; while(index<16 && slots[index].status) ++index;
	if(index==16) return;
	auto& s=slots[index]; s=AilSlot{};
	s.patch=patch; s.channel=channel; s.key=key; s.status=1;
	s.note=channel==9?ailByte(s,2):key; s.transpose=channel==9?0:ailByte(s,2);
	s.velocity=82+3*(velocity>>3);
	if(ailWord(s,0)==14) {
		s.type=0; s.block=32; s.duration=65535; s.value[3]=32767;
		s.feedback=ailByte(s,8)&1; s.value[4]=uint16(ailByte(s,8)<<12); s.scale=s.feedback|2;
		for(unsigned op=0;op<2;++op) {
			unsigned at=3+op*6;
			s.avekm[op]=ailByte(s,at)&0xf0; s.value[5+op]=uint16(ailByte(s,at)<<12);
			s.ksl[op]=ailByte(s,at+1)&0xc0; s.value[1+op]=uint16((~ailByte(s,at+1)&63)<<10);
			s.ad[op]=ailByte(s,at+2); s.sr[op]=ailByte(s,at+3);
		}
		s.value[7]=uint16(ailByte(s,13)|(ailByte(s,7)<<8)); s.update=all;
	} else ailPhase(index);
	ailAssign(index);
}
void OriginalOpl2MidiDriver::ailOff(unsigned channel,unsigned key) {
	for(unsigned i=0;i<16;++i) {
		auto& s=slots[i]; if(s.status!=1 || s.channel!=channel || s.key!=key) continue;
		if(ail_channels[channel].sustain>=64) s.sustained=true;
		else if(s.type==0) { ailRelease(i); s.status=0; ailSwitch(); }
		else s.duration=1;
	}
}
void OriginalOpl2MidiDriver::endingSend(uint32 message) {
	unsigned channel=message&15,type=message&240,a=(message>>8)&127,b=(message>>16)&127;
	auto& ch=ail_channels[channel]; uint8 flags=0;
	if(type==0x90 && b) { if(channel>=1 && channel<=9) ailOn(channel,a,b); return; }
	if(type==0x80 || type==0x90) { ailOff(channel,a); return; }
	if(type==0xc0) { ch.program=a; unsigned id=(ch.bank<<8)|a; ch.patch=ail_patches.count(id)?int(id):-1; return; }
	if(type==0xe0) { ch.bend=(b<<7)|a; flags=frequency_update; }
	else if(type==0xb0) {
		if(a==114) { ch.bank=b; return; }
		if(a==112) { ch.protection=b; return; }
		if(a==1) { ch.modulation=b; flags=0x80; }
		else if(a==7) {
			if(serpent) { serpent_raw_volume[channel]=b; ch.volume=std::min(127u,b*serpent_relative/100); }
			else ch.volume=b;
			flags=amplitude;
		}
		else if(a==11) { ch.expression=b; flags=amplitude; }
		else if(a==10) flags=amplitude;
		else if(a==64 || a==121) {
			if(a==64) ch.sustain=b;
			if(a==121 && si_new_ail) ch.sustain=0;
			if(ch.sustain<64) for(unsigned i=0;i<16;++i) if(slots[i].status && slots[i].channel==channel && slots[i].sustained) ailOff(channel,slots[i].note);
			if(a==121) { ch.modulation=0; ch.expression=127; ch.bend=8192; flags=0xc1; }
		} else if(a==123) {
			for(unsigned i=0;i<16;++i) if(slots[i].status==1 && slots[i].channel==channel) ailOff(channel,slots[i].note);
		}
	}
	for(unsigned i=0;i<16;++i) if(slots[i].status && slots[i].channel==channel) { slots[i].update|=flags; ailUpdate(i); }
}
void OriginalOpl2MidiDriver::endingTick() {
	volume_update^=amplitude;
	for(unsigned i=0;i<16;++i) {
		auto& s=slots[i]; if(!s.status || !s.type) continue;
		auto step=[&](unsigned p,uint8 flag,bool level=false) {
			if(s.step[p]) {
				uint16 old=s.value[p],now=uint16(old+s.step[p]); s.value[p]=now;
				if(level && s.status==2 && ((now^old)&0x8000) && !((now^s.step[p])&0x8000)) s.value[p]=0;
				s.update|=flag;
			}
			if(--s.count[p]==0) { ailFetch(i,p); s.update|=level?amplitude:flag; }
		};
		step(0,1); step(4,8); step(5,0x80); step(6,0x80);
		step(1,volume_update,true); step(2,volume_update,true); step(7,0x10);
		s.value[3]+=s.step[3]; if(--s.count[3]==0) ailFetch(i,3);
		if(s.update&all) ailUpdate(i);
		if(s.status!=2) { if(--s.duration==0) { s.status=2; ailPhase(i); } }
		else if(s.value[1]<1024 && s.value[2]<1024) { ailRelease(i); s.status=0; ailSwitch(); }
	}
	priority_clock+=5;
	if(priority_clock>=60) { priority_clock-=60; ailPriority(); }
}
#endif
