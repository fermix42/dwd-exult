/* Copyright (C) 2026 The Exult Team
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef ORIGINALOPL2MIDIDRIVER_H
#define ORIGINALOPL2MIDIDRIVER_H
#ifdef USE_FMOPL_MIDI
#include "LowLevelMidiDriver.h"
#include "fmopl.h"
#include "FMOplMidiDriver.h"
#include <array>
#include <deque>
#include <map>
#include <vector>

// Black Gate's native driver policy. No enhanced OPL3 state is shared here.
class OriginalOpl2MidiDriver : public LowLevelMidiDriver {
public:
	OriginalOpl2MidiDriver() : LowLevelMidiDriver(std::string(desc.name)) {}
	~OriginalOpl2MidiDriver() override { destroyMidiDriver(); }
	static const MidiDriverDesc* getDesc() { return &desc; }
	void setContext(bool black_gate, bool introduction) { bg=black_gate; intro=introduction; }
protected:
	int open() override;
	void close() override;
	void send(uint32 message) override;
	void lowLevelProduceSamples(sint16* samples, uint32 frames) override;
	void loadTimbreLibrary(IDataSource*, TimbreLibraryType) override;
	bool isSampleProducer() override { return true; }
	bool isFMSynth() override { return true; }
	bool usesNoteInstances() const override { return bg; }
	int getSequenceGlobalVolume() override { return bg?100:LowLevelMidiDriver::getGlobalVolume(); }
private:
	friend class OriginalOpl2Regression;
	static const MidiDriverDesc desc;
	static std::shared_ptr<MidiDriver> createInstance() { return std::make_shared<OriginalOpl2MidiDriver>(); }
	FMOpl_Pentagram::FM_OPL* chip=nullptr;
	bool bg=true, intro=false, ready=false;
	// Explicit transitional SI fallback: the existing FMOpl implementation.
	class Legacy : public FMOplMidiDriver {
	public:
		Legacy(uint32 rate,bool stereo_output) { sample_rate=rate; stereo=stereo_output; open(); }
		~Legacy() override { close(); }
		using FMOplMidiDriver::send;
		using FMOplMidiDriver::lowLevelProduceSamples;
		using FMOplMidiDriver::loadTimbreLibrary;
	};
	std::unique_ptr<Legacy> legacy;
	unsigned clock=0, melodic=9;
	uint8 rhythm=0;
	std::array<uint8,256> registers{};
	std::vector<std::array<uint8,48>> bank;
	std::array<uint16,13> frequencies{};
	// Low notes interpolate through preceding, defined driver data because
	// the original uses a signed remainder for its upper-table index.
	std::array<uint16,24> frequency_window{};
	std::array<uint8,88> percussion_channel{}, percussion_key{};
	std::array<int,32> programs{}, bends{}, volumes{}, vibrato{};
	struct Voice {
		int timbre=-1, channel=-1, key=0, stage=1, pitch=0, slide=0, vibrato=0;
		uint8 phase=0;
	};
	std::array<Voice,11> voices{};
	std::deque<unsigned> free, busy;
	void write(unsigned reg, uint8 value);
	void program(unsigned channel, unsigned number);
	void control(unsigned channel, unsigned number, unsigned value);
	void note(unsigned channel, unsigned key, unsigned velocity);
	void frequency(unsigned voice);
	void tick();
	void reset();
	void loadClassicBank(IDataSource*);
	void loadTimbre(unsigned voice, unsigned timbre);
	unsigned modulator(unsigned voice) const;
	unsigned carrier(unsigned voice) const;
	// The 1991 AIL policy used by BG's ending, on the same nine-channel chip.
	bool ending=false;
	struct AilSlot {
		int patch=-1, voice=-1;
		uint8 status=0, type=0, channel=0, key=0, note=0, transpose=0, velocity=0;
		bool sustained=false;
		uint8 update=0, block=0, key_shadow=0, feedback=0, scale=2;
		std::array<uint8,2> ksl{}, avekm{}, ad{}, sr{};
		std::array<uint16,8> value{}, position{}, count{}, step{};
		uint16 duration=0;
	};
	struct AilChannel {
		int patch=-1;
		uint8 bank=0, program=255, volume=127, expression=127, modulation=0, sustain=0, protection=0;
		unsigned bend=8192;
	};
	std::array<AilSlot,16> slots{};
	std::array<AilChannel,16> ail_channels{};
	std::array<int,9> owners{};
	std::map<unsigned,std::vector<uint8>> ail_patches;
	std::array<uint16,192> ail_frequencies{};
	unsigned priority_clock=0;
	uint8 volume_update=0;
	void loadEndingBank(IDataSource*);
	void endingSend(uint32);
	void endingTick();
	void ailOn(unsigned,unsigned,unsigned);
	void ailOff(unsigned,unsigned);
	void ailRelease(unsigned);
	void ailAssign(unsigned);
	void ailPriority();
	void ailSwitch();
	void ailUpdate(unsigned);
	void ailPhase(unsigned);
	void ailFetch(unsigned,unsigned);
	unsigned ailByte(const AilSlot&,unsigned) const;
	uint16 ailWord(const AilSlot&,unsigned) const;
};
#endif
#endif
