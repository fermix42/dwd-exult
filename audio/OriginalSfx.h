/* Independent original-resource SFX playback. GPL-2.0-or-later.
 * Game data is read at runtime, never compiled into this implementation.
 */
#ifndef EXULT_ORIGINAL_SFX_H
#define EXULT_ORIGINAL_SFX_H

#include "AudioSample.h"
#include "databuf.h"
#include "sha1/sha1.h"
#include "midi_drivers/FMOplMidiDriver.h"
#include "midi_drivers/MT32EmuMidiDriver.h"
#include "midi_drivers/XMidiFile.h"
#include "midi_drivers/XMidiEventList.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace OriginalSfx {
using Bytes = std::vector<uint8>;

inline Bytes readFile(const char* path) {
	IFileDataSource src(path);
	if (!src.good() || src.getSize() > 16 * 1024 * 1024) throw std::runtime_error(std::string("missing or oversized resource: ") + path);
	Bytes result(src.getSize());
	src.read(result.data(), result.size());
	if (!src.good()) throw std::runtime_error("incomplete resource");
	return result;
}

inline Bytes record(const char* path, int id) {
	IExultDataSource src(path, id);
	if (!src.good() || !src.getSize() || src.getSize() > 1024 * 1024) throw std::runtime_error("missing SFX record");
	Bytes result(src.getSize());
	src.read(result.data(), result.size());
	return result;
}

inline uint16 word(const Bytes& b, size_t at) {
	if (at >= b.size() || b.size() - at < 2) throw std::runtime_error("invalid SFX offset");
	return b[at] | (uint16(b[at + 1]) << 8);
}

// Only this exact local BG+FoV executable has an established layout.
// Offsets and hashes are identifiers; no recovered note data is embedded.
inline Bytes loadBgNotes() {
	Bytes exe = readFile("<STATIC>/../u7.exe");
	if (exe.size() != 689248 || exe[0] != 'M' || exe[1] != 'Z'
		|| std::string(sha1::toHexString(sha1::calc(exe.data(), int(exe.size()))).data())
			!= "5f2887c84474cbbd0752634925d8dd5d6425e981") {
		throw std::runtime_error("unsupported U7.EXE (expected BG+FoV SHA1 5f2887c84474cbbd0752634925d8dd5d6425e981)");
	}
	constexpr size_t offset = 299577, count = 115, stride = 8;
	Bytes notes(exe.begin() + offset, exe.begin() + offset + count * stride);
	for (size_t i = 0; i < count; ++i) {
		size_t p = i * stride;
		if ((notes[p] & ~15) || notes[p + 1] || notes[p + 2] > 127 || notes[p + 3] > 127
			|| notes[p + 6] > 127 || ((notes[p] & 1) && i + notes[p + 7] + 1 >= count))
			throw std::runtime_error("unsupported U7.EXE SFX note layout");
	}
	return notes;
}

struct Engine {
	virtual ~Engine() = default;
	virtual void restart() = 0;
	virtual bool render(sint16* out, unsigned frames) = 0;
};

#ifdef USE_FMOPL_MIDI
// A native envelope interpreter, not a MIDI approximation. One effect owns
// its chip; Exult's mixer supplies instance volume, repetition and position.
class BgAdlib final : public Engine {
	struct Envelope { uint16 value{}, next{}, ticks{}, step{}; };
	Bytes data;
	FMOpl_Pentagram::FM_OPL* chip = nullptr;
	std::array<Envelope, 8> env{};
	uint16 hold{}, key{}, mult{}, ksl{}, connection{};
	unsigned rate, phase = 0;
	bool released = false, done = false;
	// Header fields: pitch, two levels, feedback, priority, multipliers, wave.
	static constexpr std::array<unsigned, 8> fields{4, 10, 16, 28, 22, 34, 40, 46};
	void reg(unsigned r, unsigned v) { FMOpl_Pentagram::OPLWriteReg(chip, r, v & 255); }
	void advance(unsigned n) {
		auto& e = env[n];
		for (unsigned guard = 0; guard < 10; ++guard) {
			uint16 command = word(data, e.next), arg = word(data, size_t(e.next) + 2);
			e.ticks = command; e.step = arg;
			e.next += 4;
			if (!command) { e.next = uint16(e.next + arg - 4); continue; }
			if (command == 0xffff) { e.value = arg; continue; }
			if (command == 0xfffe) {
				switch (n) {
				case 0: key = arg; break;
				case 1: ksl = (ksl & 255) | ((arg & 255) << 8); break;
				case 2: ksl = (ksl & 0xff00) | (arg & 255); break;
				case 3: connection = arg; break;
				case 5: mult = (mult & 255) | ((arg & 255) << 8); break;
				case 6: mult = (mult & 0xff00) | (arg & 255); break;
				default: return;
				}
				continue;
			}
			e.ticks = command; e.step = arg; return;
		}
		e.ticks = 0xffff; e.step = 0;
	}
	void step(unsigned n) {
		auto& e = env[n]; e.value += e.step;
		if (!--e.ticks) advance(n);
	}
	void stage(bool release) {
		mult = 0x2020; key = 0x2820; ksl = connection = 0;
		for (unsigned n = 0; n < env.size(); ++n) {
			if (!release) env[n].value = word(data, fields[n]);
			env[n].next = word(data, fields[n] + (release ? 4 : 2)); advance(n);
		}
		uint16 a = 0xff0f, b = 0xff0f;
		if (word(data, 6) != 52) { a = word(data, release ? 56 : 52); b = word(data, release ? 58 : 54); }
		if (!release) { reg(0xb0, 0); reg(0x40, 63); reg(0x43, 63); }
		reg(0x60, a >> 8); reg(0x80, a); reg(0x63, b >> 8); reg(0x83, b);
	}
	void tick() {
		reg(0x40, (uint16(~env[1].value) >> 10) | (ksl >> 8));
		reg(0x43, (63 - (env[2].value >> 10)) | (ksl & 255));
		reg(0xc0, ((env[3].value >> 12) & 14) | (connection >> 8));
		reg(0x20, (env[5].value >> 12) | (mult >> 8));
		reg(0x23, (env[6].value >> 12) | (mult & 255));
		reg(0xe0, env[7].value >> 8); reg(0xe3, env[7].value);
		unsigned pitch = env[0].value >> 6;
		reg(0xa0, pitch); reg(0xb0, (key >> 8) | (pitch >> 8));
		for (unsigned n : {0u, 3u, 4u, 5u, 6u, 7u}) step(n);
		if (!released) {
			step(1); step(2);
			if (!--hold) { released = true; stage(true); }
		} else {
			unsigned ended = 0;
			for (unsigned n : {1u, 2u}) {
				auto& e = env[n];
				if (!e.value || (sint16(e.value) > 0 && e.value < 1024)
					|| (sint16(e.step) < 0 && uint32(e.value) + e.step < 65536)) {
					e.value = e.step = 0; e.ticks = 0xffff; ++ended;
				} else step(n);
			}
			if (ended == 2) { reg(0xb0, 0); done = true; }
		}
	}
public:
	BgAdlib(Bytes bytes, unsigned hz) : data(std::move(bytes)), rate(hz) {
		if (data.size() < 52 || word(data, 0) > data.size()) throw std::runtime_error("invalid AdLib SFX header");
		chip = FMOpl_Pentagram::makeAdLibOPL(rate);
		if (!chip) throw std::runtime_error("cannot initialize SFX OPL chip");
		try { restart(); } catch (...) { FMOpl_Pentagram::OPLDestroy(chip); chip = nullptr; throw; }
	}
	~BgAdlib() override { FMOpl_Pentagram::OPLDestroy(chip); }
	void restart() override {
		FMOpl_Pentagram::OPLResetChip(chip); reg(1, 32); phase = rate;
		done = released = false; hold = word(data, 2); stage(false);
	}
	bool render(sint16* out, unsigned frames) override {
		if (done) return false;
		for (unsigned i = 0; i < frames; ++i) {
			if (phase >= rate) { phase -= rate; tick(); }
			FMOpl_Pentagram::YM3812UpdateOne_Stereo(chip, out + i * 2, 1);
			phase += 60;
		}
		return true;
	}
};
#endif

#ifdef USE_MT32EMU_MIDI
class EffectMt32 final : public MT32EmuMidiDriver {
public:
	using MT32EmuMidiDriver::send;
	using MT32EmuMidiDriver::send_sysex;
	using MT32EmuMidiDriver::lowLevelProduceSamples;
	void memory(unsigned address, const Bytes& bytes) {
		Bytes message{0x41, 0x10, 0x16, 0x12, uint8((address >> 14) & 127),
			uint8((address >> 7) & 127), uint8(address & 127)};
		unsigned checksum = message[4] + message[5] + message[6];
		for (auto b : bytes) { if (b > 127) throw std::runtime_error("invalid MT-32 timbre byte"); message.push_back(b); checksum += b; }
		message.push_back((128 - (checksum & 127)) & 127); message.push_back(0xf7);
		send_sysex(0xf0, message.data(), uint16(message.size()));
	}
};

class BgMt32 final : public Engine {
	std::shared_ptr<EffectMt32> driver;
	Bytes notes;
	std::vector<unsigned> chain;
	unsigned first, current, pitch{}, remaining{}, rate, phase{}, silent{};
	bool finished = false;
	void note(unsigned velocity) { driver->send(0x91 | (pitch << 8) | (velocity << 16)); }
	void begin() {
		const auto p = current * 8;
		pitch = notes[p + 2]; remaining = word(notes, p + 4);
		auto it = std::find(chain.begin(), chain.end(), current);
		if (it == chain.end()) throw std::runtime_error("invalid MT-32 SFX chain");
		driver->send(0xc1 | (unsigned(it - chain.begin()) << 8));
		note(notes[p + 3]);
	}
	void tick() {
		if (finished) return;
		auto p = current * 8;
		unsigned flags = notes[p];
		if ((flags & 4) || --remaining) return;
		note(0);
		bool retrigger = false;
		if (flags & 2) {
			if (pitch < notes[p + 6]) { ++pitch; retrigger = true; }
			else if (pitch > notes[p + 6]) { --pitch; retrigger = true; }
			else pitch = notes[p + 2];
		}
		if ((flags & 8) || retrigger) { note(notes[p + 3]); remaining = word(notes, p + 4); }
		else if (flags & 1) { current += notes[p + 7] + 1; begin(); }
		else finished = true;
	}
public:
	BgMt32(unsigned id, unsigned hz) : notes(loadBgNotes()), first(id), current(id), rate(hz) {
		if (id >= 115) throw std::runtime_error("BG SFX number out of range");
		for (unsigned n = id;;) {
			chain.push_back(n);
			if (!(notes[n * 8] & 1)) break;
			n += notes[n * 8 + 7] + 1;
			if (n >= 115 || chain.size() >= 64) throw std::runtime_error("invalid BG SFX chain");
		}
		driver = std::make_shared<EffectMt32>();
		if (driver->initMidiDriver(rate, true)) throw std::runtime_error("MT32Emu SFX initialization failed (check ROMs)");
		driver->setSfxSynchronous(true);
		try {
			for (unsigned slot = 0; slot < chain.size(); ++slot) {
				Bytes timbre = record("<STATIC>/mt32sfx.dat", chain[slot]);
				if (timbre.size() != 246) throw std::runtime_error("unsupported BG MT-32 timbre size");
				// Roland addresses are represented here as contiguous 7-bit bytes.
				driver->memory((8u << 14) + slot * 256, timbre);
				driver->memory((5u << 14) + slot * 8, Bytes{2, uint8(slot), 24, 50, 24, 0, 0, 0});
			}
			restart();
		} catch (...) { driver->destroyMidiDriver(); throw; }
	}
	~BgMt32() override { driver->destroyMidiDriver(); }
	void restart() override {
		driver->send(0xb1 | (123 << 8));
		driver->send(0xb1 | (7 << 8) | (127 << 16));
		driver->send(0xb1 | (10 << 8) | (64 << 16));
		current = first; phase = silent = 0; finished = false; begin();
	}
	bool render(sint16* out, unsigned frames) override {
		if (finished && silent >= rate / 4) return false;
		for (unsigned i = 0; i < frames; ++i) {
			if (phase >= rate) { phase -= rate; tick(); }
			driver->lowLevelProduceSamples(out + i * 2, 1); phase += 60;
			if (finished && !out[i * 2] && !out[i * 2 + 1]) ++silent; else silent = 0;
		}
		return true;
	}
};
#endif

#ifdef USE_FMOPL_MIDI
// SI's effect banks contain Origin TVFX records, not fixed BNK instruments.
// Keep their envelopes on the isolated SFX synth; the existing sequencer
// still owns XMIDI timing, channel locks, note durations and loops.
class SiFmDriver final : public FMOplMidiDriver {
	using Chip = FMOpl_Pentagram::FM_OPL;
	struct Voice {
		struct Ramp { uint16 value{}, pos{}, left{1}, delta{}; };
		const Bytes* data = nullptr;
		std::array<Ramp, 8> ramps{};
		unsigned channel{}, note{}, velocity{}, duration{};
		uint8 key{0x28}, flags[2]{0x20,0x20}, ksl[2]{}, connection{}, scale{2};
		bool release = false;
		unsigned dirty = 255;
	};
	Chip* effects = nullptr;
	std::map<unsigned, Bytes> timbres;
	std::array<Voice, 9> voices{};
	std::array<unsigned,16> banks{}, programs{}, volumes{}, expressions{}, pans{}, sustain{};
	unsigned clock = 0;
	bool level_refresh = false;
	static unsigned scaleVolume(unsigned x, unsigned y) {
		unsigned v = x*y/128; return v ? v+1 : 0;
	}
	void write(unsigned voice, unsigned reg, unsigned value, int op = -1) {
		unsigned operatorOffset = (voice/3)*8 + voice%3;
		unsigned address = reg + (op < 0 ? voice : operatorOffset + unsigned(op)*3);
		FMOpl_Pentagram::OPLWriteReg(effects, address, value & 255);
	}
	void phase(Voice& v) {
		v.key=0x28; v.flags[0]=v.flags[1]=0x20; v.ksl[0]=v.ksl[1]=0;
		v.connection=0; v.scale=2; v.dirty=255;
		for(unsigned p=0;p<8;++p) {
			unsigned field=6+p*6;
			if(!v.release) v.ramps[p].value=word(*v.data,field);
			v.ramps[p].pos=uint16(word(*v.data,field+(v.release?4:2))+2);
			v.ramps[p].left=1; v.ramps[p].delta=0;
		}
		if(!v.release) v.duration=uint16(word(*v.data,4)+1);
	}
	void fetch(Voice& v,unsigned p) {
		auto& r=v.ramps[p];
		for(unsigned guard=0;guard<10;++guard) {
			unsigned op=word(*v.data,r.pos), arg=word(*v.data,size_t(r.pos)+2);
			if(!op) { r.pos=uint16(r.pos+arg); continue; }
			r.pos+=4;
			if(op==65535) { r.value=uint16(arg); continue; }
			if(op!=65534) { r.left=uint16(op); r.delta=uint16(arg); return; }
			switch(p) {
			case 0: v.key=uint8(arg>>8); break;
			case 1: v.ksl[0]=uint8(arg); break;
			case 2: v.ksl[1]=uint8(arg); break;
			case 4: v.connection=uint8(arg>>8); v.scale=uint8((arg>>8)|2); break;
			case 5: v.flags[0]=uint8(arg); break;
			case 6: v.flags[1]=uint8(arg); break;
			default: break;
			}
		}
		r.left=65535; r.delta=0;
	}
	void update(unsigned index) {
		auto& v=voices[index]; auto& r=v.ramps;
		if(v.dirty & 1) {
			unsigned f=r[0].value>>6; write(index,0xa0,f); write(index,0xb0,(f>>8)|v.key);
		}
		if(v.dirty & 128) for(unsigned op=0;op<2;++op) write(index,0x20,(r[5+op].value>>12)|v.flags[op],op);
		if(v.dirty & 64) {
			unsigned gain=scaleVolume(scaleVolume(volumes[v.channel],expressions[v.channel]),v.velocity);
			for(unsigned op=0;op<2;++op) {
				unsigned level=r[1+op].value>>10;
				if(v.scale & (1u<<op)) level=level*gain/127;
				write(index,0x40,((~level)&63)|v.ksl[op],op);
			}
		}
		if(v.dirty & 32) {
			uint16 a=0xff0f,b=0xff0f;
			if(uint16(word(*v.data,8)+2)!=54) {
				a=word(*v.data,v.release?58:54); b=word(*v.data,v.release?60:56);
			}
			write(index,0x60,b>>8,0); write(index,0x80,b,0);
			write(index,0x60,a>>8,1); write(index,0x80,a,1);
		}
		if(v.dirty & 16) { write(index,0xe0,r[7].value>>8,0); write(index,0xe0,r[7].value,1); }
		if(v.dirty & 8) write(index,0xc0,((r[4].value>>12)&14)|(v.connection&1));
		effects->P_CH[index].PAN=uint8(pans[v.channel]); v.dirty=0;
	}
	void tick() {
		level_refresh=!level_refresh;
		constexpr unsigned masks[8]={1,64,64,0,8,128,128,16};
		for(unsigned i=0;i<voices.size();++i) {
			auto& v=voices[i]; if(!v.data) continue;
			for(unsigned p=0;p<8;++p) {
				auto& r=v.ramps[p];
				if(r.delta) {
					uint16 old=r.value, next=uint16(old+r.delta);
					if(v.release && (p==1 || p==2) && ((next^old)&0x8000) && !((next^r.delta)&0x8000)) next=0;
					r.value=next;
					if((p!=1 && p!=2) || level_refresh) v.dirty|=masks[p];
				}
				if(!uint16(--r.left)) { fetch(v,p); v.dirty|=masks[p]; }
			}
			update(i);
			if(!v.release) {
				if(!uint16(--v.duration)) { v.release=true; phase(v); }
			} else if(v.ramps[1].value<1024 && v.ramps[2].value<1024) {
				write(i,0xb0,0); v.data=nullptr;
			}
		}
	}
protected:
	int open() override {
		int result=FMOplMidiDriver::open(); if(result) return result;
		effects=FMOpl_Pentagram::makeAdLibOPL(sample_rate);
		if(!effects) { FMOplMidiDriver::close(); return 1; }
		FMOpl_Pentagram::OPLWriteReg(effects,1,32);
		volumes.fill(127); expressions.fill(127); pans.fill(64); return 0;
	}
	void close() override {
		if(effects) { FMOpl_Pentagram::OPLDestroy(effects); effects=nullptr; }
		FMOplMidiDriver::close();
	}
	void loadTimbreLibrary(IDataSource* source,TimbreLibraryType type) override {
		if(type!=TIMBRE_LIBRARY_XMIDI_AD) throw std::runtime_error("unsupported SI FM library");
		source->seek(0); Bytes raw(source->getSize()); source->read(raw.data(),raw.size());
		std::vector<std::pair<unsigned,Bytes>> fixed;
		for(size_t at=0;at+6<=raw.size();at+=6) {
			unsigned patch=raw[at],bank=raw[at+1]; if(patch==255 || bank==255) break;
			uint32 offset=uint32(raw[at+2])|(uint32(raw[at+3])<<8)|(uint32(raw[at+4])<<16)|(uint32(raw[at+5])<<24);
			unsigned size=word(raw,offset);
			if(size<14 || offset>raw.size() || size>raw.size()-offset) throw std::runtime_error("invalid SI FM timbre");
			Bytes bytes(raw.begin()+offset,raw.begin()+offset+size);
			if(size==14) fixed.emplace_back((bank<<7)|patch,std::move(bytes));
			else {
				if(size<54 || bytes[3]!=2) throw std::runtime_error("unsupported SI TVFX timbre type");
				timbres.emplace((bank<<7)|patch,std::move(bytes));
			}
		}
		Bytes compatible((fixed.size()+1)*6,255);
		for(unsigned i=0;i<fixed.size();++i) {
			compatible[i*6]=uint8(fixed[i].first&127); compatible[i*6+1]=uint8(fixed[i].first>>7);
			uint32 offset=uint32(compatible.size());
			for(unsigned b=0;b<4;++b) compatible[i*6+2+b]=uint8(offset>>(8*b));
			compatible.insert(compatible.end(),fixed[i].second.begin(),fixed[i].second.end());
		}
		IBufferDataView bnk(compatible.data(),compatible.size()); FMOplMidiDriver::loadTimbreLibrary(&bnk,type);
	}
	void send(uint32 message) override {
		unsigned channel=message&15,kind=message&240,a=(message>>8)&127,b=(message>>16)&127;
		if(kind==0xb0) {
			if(a==114) banks[channel]=b;
			if(a==7) volumes[channel]=b;
			if(a==11) expressions[channel]=b;
			if(a==10) pans[channel]=b;
			if(a==64) sustain[channel]=b;
			for(auto& v:voices) if(v.data && v.channel==channel) {
				v.dirty|=64;
				if(a==123 || a==121) v.duration=1;
			}
		}
		if(kind==0xc0) programs[channel]=a;
		auto timbre=timbres.find((banks[channel]<<7)|programs[channel]);
		if(kind==0x80 || (kind==0x90 && !b)) {
			bool found=false;
			for(auto& v:voices) if(v.data && v.channel==channel && v.note==a) { if(sustain[channel]<64) v.duration=1; found=true; }
			if(found) return;
		}
		if(timbre!=timbres.end()) {
			if(kind==0xc0) return;
			if(kind==0x90 && b) {
				unsigned index=0; while(index<voices.size() && voices[index].data) ++index;
				if(index==voices.size()) index=0;
				write(index,0xb0,0); auto& v=voices[index]; v=Voice{};
				v.data=&timbre->second; v.channel=channel; v.note=a; v.velocity=82+3*(b>>3);
				phase(v); update(index); return;
			}
		}
		FMOplMidiDriver::send(message);
	}
	void lowLevelProduceSamples(sint16* output,uint32 frames) override {
		FMOplMidiDriver::lowLevelProduceSamples(output,frames);
		std::array<sint16,2> pcm{};
		for(unsigned i=0;i<frames;++i) {
			clock+=60; if(clock>=sample_rate) { clock-=sample_rate; tick(); }
			FMOpl_Pentagram::YM3812UpdateOne_Stereo(effects,pcm.data(),1);
			for(unsigned c=0;c<2;++c) output[i*2+c]=sint16(std::clamp(int(output[i*2+c])+pcm[c],-32768,32767));
		}
	}
};
#endif

class SiXmidi final : public Engine {
	std::shared_ptr<MidiDriver> driver;
	std::unique_ptr<XMidiFile> sequence;
	unsigned rate, rendered = 0, silent = 0;
public:
	SiXmidi(bool fm, int id, unsigned hz) : rate(hz) {
#ifdef USE_FMOPL_MIDI
		if (fm) driver = std::make_shared<SiFmDriver>();
#endif
#ifdef USE_MT32EMU_MIDI
		if (!fm) driver = MT32EmuMidiDriver::getDesc()->createInstance();
#endif
		if (!driver || driver->initMidiDriver(rate, true)) throw std::runtime_error("SFX synthesizer unavailable (MT32Emu requires ROMs)");
		std::static_pointer_cast<LowLevelMidiDriver>(driver)->setSfxSynchronous(true);
		try {
			IFileDataSource timbres(fm ? "<STATIC>/xmidi.ad" : "<STATIC>/xmidi.mt");
			if (!timbres.good()) throw std::runtime_error("missing SI SFX timbre library");
			driver->loadTimbreLibrary(&timbres, fm ? MidiDriver::TIMBRE_LIBRARY_XMIDI_AD : MidiDriver::TIMBRE_LIBRARY_XMIDI_MT);
			Bytes data = record(fm ? "<STATIC>/adlibsfx.dat" : "<STATIC>/mt32sfx.dat", id);
			if (data.size() < 12 || std::memcmp(data.data(), "FORM", 4)) throw std::runtime_error("unsupported SI SFX sequence");
			IBufferDataView src(data.data(), data.size());
			sequence = std::make_unique<XMidiFile>(&src, XMIDIFILE_CONVERT_NOCONVERSION);
			if (!sequence->number_of_tracks()) throw std::runtime_error("empty SI SFX sequence");
			// SI's two indirect controls are instance pan and volume. Supply
			// neutral synthesis values; the existing mixer applies both live.
			std::array<int, 16> indirect;
			indirect.fill(-1);
			for (auto* event = sequence->GetEventList(0)->events; event; event = event->next) {
				if (event->getStatusType() != MidiStatus::Controller) continue;
				auto& pending = indirect[event->getChannel()];
				if (pending >= 0) {
					if (pending > 1) throw std::runtime_error("unsupported SI SFX indirect control index");
					event->data[1] = pending == 0 ? 64 : 127; pending = -1;
				}
				if (event->data[0] == 115) pending = event->data[1];
			}
			restart();
		} catch (...) { driver->destroyMidiDriver(); throw; }
	}
	~SiXmidi() override { driver->destroyMidiDriver(); }
	void restart() override { rendered = silent = 0; driver->startSequence(0, sequence->GetEventList(0), false, 255); }
	bool render(sint16* out, unsigned frames) override {
		if (rendered && !driver->isSequencePlaying(0) && silent >= rate / 4) return false;
		driver->produceSamples(out, frames * 4); rendered += frames;
		if (!driver->isSequencePlaying(0)) {
			for (unsigned i = 0; i < frames; ++i) {
				if (!out[i * 2] && !out[i * 2 + 1]) ++silent; else silent = 0;
			}
		}
		return true;
	}
};

class Sample final : public Pentagram::AudioSample {
	std::unique_ptr<Engine> engine;
public:
	Sample(std::unique_ptr<Engine> e, unsigned hz) : AudioSample(nullptr, 0), engine(std::move(e)) {
		bits = 16; stereo = true; sample_rate = hz; frame_size = 1024;
		decompressor_size = decompressor_align = 1; length = UINT32_MAX;
	}
	void initDecompressor(void*) const override {}
	void rewind(void*) const override { engine->restart(); }
	uint32 decompressFrame(void*, void* output) const override {
		try {
			std::memset(output, 0, frame_size);
			return engine->render(static_cast<sint16*>(output), frame_size / 4) ? frame_size : 0;
		} catch (const std::exception& e) {
			std::cerr << "Original SFX stopped: " << e.what() << std::endl; return 0;
		}
	}
};

inline Pentagram::AudioSample* create(const std::string& mode, bool si, int id, unsigned rate) {
	if (id < 0 || (!si && id >= 115)) throw std::runtime_error("SFX number outside original game range");
	std::unique_ptr<Engine> engine;
	if (si) engine = std::make_unique<SiXmidi>(mode == "FMOpl", id, rate);
	else if (mode == "FMOpl") {
#ifdef USE_FMOPL_MIDI
		engine = std::make_unique<BgAdlib>(record("<STATIC>/adlibsfx.dat", id), rate);
#endif
	} else {
#ifdef USE_MT32EMU_MIDI
		engine = std::make_unique<BgMt32>(id, rate);
#endif
	}
	if (!engine) throw std::runtime_error("SFX backend not compiled in");
	return new Sample(std::move(engine), rate);
}

} // namespace OriginalSfx
#endif
