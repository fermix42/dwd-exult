/* Copyright (C) 2026 The Exult Team
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SI resource/context policy. The common AIL register interpreter retains
 * explicit 1991 cinematic versus 1992 gameplay behavior on one YM3812.
 */
#include "pent_include.h"
#ifdef USE_FMOPL_MIDI
#include "OriginalOpl2MidiDriver.h"
#include "databuf.h"
#include "sha1/sha1.h"
#include <algorithm>
#include <stdexcept>

namespace {
std::vector<uint8> validated(IDataSource& source,unsigned size,const char* hash,const char* name) {
	if(!source.good() || source.getSize()!=size) throw std::runtime_error(std::string("Original OPL2: unsupported ")+name);
	std::vector<uint8> bytes(size); source.read(bytes.data(),size);
	if(std::string(sha1::toHexString(sha1::calc(bytes.data(),int(size))).data())!=hash)
		throw std::runtime_error(std::string("Original OPL2: unrecognized ")+name);
	return bytes;
}
}
void OriginalOpl2MidiDriver::loadSerpentBank(IDataSource* source,TimbreLibraryType type) {
	serpent_relative=100; serpent_raw_volume.fill(127);
	if(si_menu && type==TIMBRE_LIBRARY_U7VOICE_AD) { loadClassicBank(source); return; }
	if(type!=TIMBRE_LIBRARY_XMIDI_AD) throw std::runtime_error("Original OPL2: unsupported SI music bank");
	if(si_new_ail) {
		IExultDataSource driver("<STATIC>/snddrvrs.dat",0);
		serpent_driver=validated(driver,17031,"14c01ea763f92ed452a6c21800e3f66da8c1af93","SI SNDDRVRS.DAT FM driver");
		IFileDataSource executable("<STATIC>/../si.exe");
		auto bytes=validated(executable,754240,"aae950c362e041d175c244815f1c74609e7dc5a8","SI.EXE music-volume data");
		std::copy_n(bytes.begin()+306311,serpent_track_volumes.size(),serpent_track_volumes.begin());
	} else {
		IFileDataSource driver("<STATIC>/adlib.adv");
		serpent_driver=validated(driver,16144,"3f663b98310b7c938c47f4cec580a0b36359979d","SI cinematic ADLIB.ADV");
	}
	unsigned table=si_new_ail?2755:2720;
	for(unsigned i=0;i<192;++i) ail_frequencies[i]=serpent_driver[table+i*2]|(uint16(serpent_driver[table+i*2+1])<<8);
	loadAilBank(source);
	initializeSerpentAil();
}
void OriginalOpl2MidiDriver::initializeSerpentAil() {
	// ResetSynth's exact register initialization comes from the validated driver.
	unsigned at=si_new_ail?3331:3296;
	for(unsigned reg=1;reg<=245;++reg) write(reg,serpent_driver[at+reg-1]);
	ail_rover=8; priority_clock=0; volume_update=0;
	unsigned programs_at=si_new_ail?10960:10342;
	for(unsigned channel=1;channel<=9;++channel) {
		unsigned patch=serpent_driver[programs_at+channel-1];
		if(patch<128) endingSend(0xc0|channel|(patch<<8));
	}
}
void OriginalOpl2MidiDriver::setSerpentTrack(int track) {
	if(!serpent || !ending) return;
	serpent_relative=si_new_ail && track>=0 && unsigned(track)<serpent_track_volumes.size()?serpent_track_volumes[track]:100;
	for(unsigned channel=0;channel<16;++channel)
		endingSend(0xb0|channel|(7<<8)|(unsigned(serpent_raw_volume[channel])<<16));
}
#endif
