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

class SiXmidi final : public Engine {
	std::shared_ptr<MidiDriver> driver;
	std::unique_ptr<XMidiFile> sequence;
	unsigned rate, rendered = 0, silent = 0;
public:
	SiXmidi(bool fm, int id, unsigned hz) : rate(hz) {
#ifdef USE_FMOPL_MIDI
		if (fm) driver = FMOplMidiDriver::getDesc()->createInstance();
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
