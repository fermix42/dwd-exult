/*
Copyright (C) 2003  The Pentagram Team
Copyright (C) 2016-2022  The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef MT32EMUMIDIDRIVER_H_INCLUDED
#define MT32EMUMIDIDRIVER_H_INCLUDED

#ifdef USE_MT32EMU_MIDI

#	include "LowLevelMidiDriver.h"
#	include <array>

#	ifdef __GNUC__
#		pragma GCC diagnostic push
#		pragma GCC diagnostic ignored "-Wundef"
#		pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#		if defined(__llvm__) || defined(__clang__)
#			pragma GCC diagnostic ignored "-Wmacro-redefined"
#		endif
#	endif    // __GNUC__
#	ifdef SDL_PLATFORM_IOS
#		include <mt32emu.h>
#	else
#		include <mt32emu/mt32emu.h>
#	endif
#	ifdef __GNUC__
#		pragma GCC diagnostic pop
#	endif    // __GNUC__

namespace MT32Emu {
	class Synth;
	class SampleRateConverter;
}    // namespace MT32Emu

class OriginalRolandMusic;

class MT32EmuMidiDriver : public LowLevelMidiDriver {
public:
	enum class Model { LegacyAuto, Early, Later, CM32L };
private:
	friend class RolandModelRegression;
	friend class RolandMusicRegression;
	static const MidiDriverDesc desc;
	static const MidiDriverDesc early_desc, later_desc, cm32l_desc;

	static std::shared_ptr<MidiDriver> createInstance() {
		return std::make_shared<MT32EmuMidiDriver>();
	}
	static std::shared_ptr<MidiDriver> createEarly() { return std::make_shared<MT32EmuMidiDriver>(Model::Early); }
	static std::shared_ptr<MidiDriver> createLater() { return std::make_shared<MT32EmuMidiDriver>(Model::Later); }
	static std::shared_ptr<MidiDriver> createCM32L() { return std::make_shared<MT32EmuMidiDriver>(Model::CM32L); }
	struct ROMSet;
	std::unique_ptr<ROMSet> roms;
	Model requested_model;
	std::string active_machine, rom_identification, initialization_error;

    bool score_sysex=false;
    std::atomic_bool original_music{false};
    bool music_classic=false, music_new_ail=false, music_ready=false;
    std::unique_ptr<OriginalRolandMusic> music;
    uint64 music_frames=0, music_wait=0;
    std::vector<uint8> music_sysex;
    std::array<uint8,71> music_volumes{};
    void renderSynth(sint16*,uint32);
	int openExplicit();
	int initializeSynth(const MT32Emu::ROMImage*,const MT32Emu::ROMImage*);
	int failure(int code,const std::string& message);
	static const MT32Emu::MachineConfiguration* compatibleMachine(Model,const MT32Emu::ROMInfo*,const MT32Emu::ROMInfo*);

	MT32Emu::Synth*               mt32;
	MT32Emu::SampleRateConverter* mt32src;

public:
	MT32EmuMidiDriver();
	explicit MT32EmuMidiDriver(Model);
	~MT32EmuMidiDriver() override;
    bool setMusicContext(bool black_gate,bool menu,bool introduction,bool ending);
    bool hasOriginalMusic() const { return original_music; }
    void prepareMusicTrack(IDataSource*,int sequence,int track);
    void startSequence(int,XMidiEventList*,bool,int,int=-1) override;
	static const char* identifier(Model);
	static const char* modelLabel(Model);
	static bool isExplicitDriver(std::string_view);
	const std::string& getActiveMachineID() const { return active_machine; }
	const std::string& getROMIdentification() const { return rom_identification; }
	const std::string& getInitializationError() const { return initialization_error; }
	static const MidiDriverDesc* getEarlyDesc() { return &early_desc; }
	static const MidiDriverDesc* getLaterDesc() { return &later_desc; }
	static const MidiDriverDesc* getCM32LDesc() { return &cm32l_desc; }

	static const MidiDriverDesc* getDesc() {
		return &desc;
	}

protected:
    void loadTimbreLibrary(IDataSource*,TimbreLibraryType) override;
    void sequenceSendSysEx(uint16,uint8,const uint8*,uint16) override;
    bool usesOriginalRolandTiming() const override { return original_music; }
    bool usesNoteInstances() const override { return original_music; }
    bool isRolandWaiting() const override;
    uint32 getTickCount(uint16) override;
    int getSequenceGlobalVolume() override { return original_music?100:LowLevelMidiDriver::getGlobalVolume(); }
	// LowLevelMidiDriver implementation
	int  open() override;
	void close() override;
	void send(uint32 b) override;
	void send_sysex(uint8 status, const uint8* msg, uint16 length) override;
	void lowLevelProduceSamples(sint16* samples, uint32 num_samples) override;

	// MidiDriver overloads
	bool isSampleProducer() override {
		return true;
	}

	bool isMT32() override {
		return true;
	}
};

#endif    // USE_MT32EMU_MIDI

#endif    // MT32EMUMIDIDRIVER_H_INCLUDED
