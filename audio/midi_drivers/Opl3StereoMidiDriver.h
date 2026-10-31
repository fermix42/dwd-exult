/*
Copyright (C) 2003  The Pentagram Team
Copyright (C) 2005-2022  The Exult Team

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

#ifndef OPL3STEREOMIDIDRIVER_H_INCLUDED
#define OPL3STEREOMIDIDRIVER_H_INCLUDED

#ifdef USE_FMOPL_MIDI

#	include "LowLevelMidiDriver.h"
#	include "NukedOpl3.h"
#	include <array>
#	include <tuple>

class IDataSource;

class Opl3StereoMidiDriver : public LowLevelMidiDriver {
	static const MidiDriverDesc desc;

	static std::shared_ptr<MidiDriver> createInstance() {
		return std::make_shared<Opl3StereoMidiDriver>();
	}

public:
	Opl3StereoMidiDriver() : LowLevelMidiDriver(std::string(desc.name)) {}
	~Opl3StereoMidiDriver() override { destroyMidiDriver(); }

	void setOriginalIntro(bool enabled) { original_intro=enabled; }

	static const MidiDriverDesc* getDesc() {
		return &desc;
	}

protected:
	// LowLevelMidiDriver implementation
	int  open() override;
	void close() override;
	void send(uint32 b) override;
	void sendNote(uint32 message, uint64 identity) override;
	bool usesNoteInstances() const override { return true; }
	// BG's CC7 is a timbre parameter with a nonzero floor, not a master gain.
	int getSequenceGlobalVolume() override { return (bg_bank || si_bank) ? 100 : LowLevelMidiDriver::getGlobalVolume(); }
	void lowLevelProduceSamples(sint16* samples, uint32 num_samples) override;

	// MidiDriver overloads
	bool isSampleProducer() override {
		return true;
	}

	bool isFMSynth() override {
		return true;
	}

	void loadTimbreLibrary(IDataSource*, TimbreLibraryType type) override;

private:
	friend class Opl3VoiceRegression;
	static constexpr int VoiceCount = 18;
	static constexpr unsigned channelRegister(unsigned voice) { return (voice % 9) | ((voice / 9) << 8); }
	struct xinstrument {
		unsigned char mod_avekm;
		unsigned char car_avekm;
		unsigned char mod_ksl_tl;
		unsigned char car_ksl_tl;
		unsigned char mod_ad;
		unsigned char car_ad;
		unsigned char mod_sr;
		unsigned char car_sr;
		unsigned char mod_ws;
		unsigned char car_ws;
		unsigned char fb_c;
		unsigned char perc_voice;
		std::array<unsigned char,48> bg{};
		bool native_bg = false;
		bool native_si = false;
		unsigned char transpose = 0;
		bool operator==(const xinstrument& b) const {
			return std::tie(mod_avekm,car_avekm,mod_ksl_tl,car_ksl_tl,mod_ad,car_ad,mod_sr,car_sr,mod_ws,car_ws,fb_c,perc_voice,bg,native_bg,native_si,transpose)
				==std::tie(b.mod_avekm,b.car_avekm,b.mod_ksl_tl,b.car_ksl_tl,b.mod_ad,b.car_ad,b.mod_sr,b.car_sr,b.mod_ws,b.car_ws,b.fb_c,b.perc_voice,b.bg,b.native_bg,b.native_si,b.transpose);
		}
	};

	static const xinstrument   midi_fm_instruments_table[128];
	static const int           my_midi_fm_vol_table[128];
	static int                 lucas_fm_vol_table[128];
	static const unsigned short adlib_opadd[VoiceCount];
	static const int           fnums[12];
	static const double        bend_fine[256];
	static const double        bend_coarse[128];

	struct midi_channel {
		int         inum;
		xinstrument ins;
		bool        xmidi;
		int         xmidi_bank;
		int         vol;
		int         expression;
		int         nshift;
		int         enabled;
		int         pitchbend;
		int         pan;
		int         sustain;
		int         modulation;
	};

	struct xmidibank {
		xinstrument insbank[128];
	};

	struct channel_data {
		int  channel;
		int  note;
		int  counter;
		int  velocity;
		bool sustained;
	};

	enum {
		ADLIB_MELODIC = 0,
		ADLIB_RYTHM   = 1
	};

	void          midi_write_adlib(unsigned int reg, unsigned char val);
	void          midi_fm_instrument(int voice, xinstrument& inst);
	int           midi_calc_volume(int chan, int vel);
	void          midi_update_volume(int chan);
	void          midi_fm_volume(int voice, int volume);
	void          midi_fm_playnote(int voice, int note, int volume, int pitchbend, int pan);
	void          midi_fm_endnote(int voice);
	unsigned char adlib_data[512]{};

	struct BgVoice {
		int channel = -1;
		int intro_part = -1;
		int pitch = 0, slide = 0;
		unsigned phase = 0, stage = 3;
		bool keyed = false;
	};
	std::array<xinstrument,256> bg_timbres{};
	xinstrument bg_default{};
	bool original_intro=false;
	std::array<uint8,88> intro_channels{},intro_keys{};
	std::array<xinstrument,21> intro_parts{};
	void loadIntroMapping();
	BgVoice bg_voice[VoiceCount];
	xinstrument voice_timbre[VoiceCount]{};
	bool timbre_valid[VoiceCount]{};
	bool bg_bank = false;
	bool si_bank = false;
	std::array<uint16,192> si_frequencies{};
	void loadSiFrequencies();
	void updateSiPitch(int voice, int bend);
	uint64 bg_tick_phase = 0;
	void updateBgPitch(int voice);
	void tickBgVoices();
	void applyModulation(int voice);
	channel_data chp[VoiceCount];
	uint64 voice_identity[VoiceCount]{};
	uint64 note_identity = 0;
	uint64 release_order[VoiceCount]{};
	uint64 release_clock = 0;
	int voice_pan[VoiceCount]{};
	xinstrument  myinsbank[128];
	xmidibank*   xmidibanks[128];
	void         loadXMIDITimbres(IDataSource* ds);
	void         loadU7VoiceTimbres(IDataSource* ds);

	NukedStereo::Chip* opl = nullptr;
	midi_channel             ch[16];
};

#endif    // USE_FMOPL_MIDI

#endif    // OPL3STEREODRV_H
