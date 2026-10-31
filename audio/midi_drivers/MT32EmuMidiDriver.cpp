/*
Copyright (C) 2003  The Pentagram Team
Copyright (C) 2010-2022  The Exult Team

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

#include "pent_include.h"

#ifdef USE_MT32EMU_MIDI
#	include "MT32EmuMidiDriver.h"

#	include "utils.h"

#	include <cstring>
#	include <algorithm>
#	include <filesystem>
#	include <set>
#	include <vector>
#	include <stdexcept>

using namespace MT32Emu;

const MidiDriver::MidiDriverDesc MT32EmuMidiDriver::desc = MidiDriver::MidiDriverDesc("MT32Emu", createInstance);
const MidiDriver::MidiDriverDesc MT32EmuMidiDriver::early_desc("MT32EmuEarly",createEarly);
const MidiDriver::MidiDriverDesc MT32EmuMidiDriver::later_desc("MT32EmuLater",createLater);
const MidiDriver::MidiDriverDesc MT32EmuMidiDriver::cm32l_desc("MT32EmuCM32L",createCM32L);

struct MT32EmuMidiDriver::ROMSet {
    struct FreeImage { void operator()(const ROMImage* image) const { ROMImage::freeROMImage(image); } };
    using Image=std::unique_ptr<const ROMImage,FreeImage>;
    // Files must outlive their borrowed images; members are destroyed in reverse order.
    std::vector<std::unique_ptr<FileStream>> files;
    std::vector<Image> images;
    const ROMImage* control=nullptr;
    const ROMImage* pcm=nullptr;
    const MachineConfiguration* machine=nullptr;
    unsigned unrecognized=0;

    void discover() {
        namespace fs=std::filesystem;
        std::vector<std::string> roots;
        for(const char* key:{"<BUNDLE>","<DATA>","<SAVEHOME>"}) if(is_system_path_defined(key)) {
            std::string root=get_system_path(key);
            if(std::string_view(key)=="<SAVEHOME>") root+="/data";
            if(std::find(roots.begin(),roots.end(),root)==roots.end()) roots.push_back(root);
        }
        std::set<size_t> sizes;
        for(auto known=ROMInfo::getAllROMInfos();*known;++known) sizes.insert((*known)->fileSize);
        for(const auto& root:roots) {
            for(const char* suffix:{"","mt32-early","mt32-later","cm32l"}) {
                fs::path directory=fs::u8path(root); if(*suffix) directory/=suffix;
                std::error_code error;
                std::vector<fs::path> paths;
                for(fs::directory_iterator it(directory,fs::directory_options::skip_permission_denied,error),end;!error && it!=end;it.increment(error)) {
                    std::error_code status_error;
                    if(it->is_regular_file(status_error) && !status_error) paths.push_back(it->path());
                }
                std::sort(paths.begin(),paths.end());
                for(const auto& path:paths) {
                    std::error_code size_error; auto size=fs::file_size(path,size_error);
                    std::string extension=path.extension().u8string();
                    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::tolower(c));});
                    bool named_rom=extension==".rom" || extension==".bin";
                    if(size_error || !sizes.count(size)) { if(named_rom) ++unrecognized; continue; }
                    auto file=std::make_unique<FileStream>();
                    if(!file->open(path.u8string().c_str())) { if(named_rom) ++unrecognized; continue; }
                    Image image(ROMImage::makeROMImage(file.get()));
                    if(!image || !image->getROMInfo()) { if(named_rom) ++unrecognized; continue; }
                    files.push_back(std::move(file)); images.push_back(std::move(image));
                }
            }
        }
        // Let Munt validate known split/multiplexed pairs and merge in memory.
        // This also handles its MT-32 PCM / CM-32L lower-half alias correctly.
        for(size_t i=0;i<files.size();++i) for(size_t j=i+1;j<files.size();++j) {
            Image merged(ROMImage::makeROMImage(files[i].get(),files[j].get()));
            if(merged && merged->getROMInfo()) images.push_back(std::move(merged));
        }
    }
    std::string summary() const {
        std::set<std::string> names;
        for(const auto& image:images) if(image->getROMInfo()) names.insert(image->getROMInfo()->shortName);
        std::string result;
        for(const auto& name:names) { if(!result.empty()) result+=", "; result+=name; }
        return result.empty()?"none recognized":result;
    }
};

const char* MT32EmuMidiDriver::identifier(Model model) {
    switch(model) {
    case Model::Early:return "MT32EmuEarly";
    case Model::Later:return "MT32EmuLater";
    case Model::CM32L:return "MT32EmuCM32L";
    default:return "MT32Emu";
    }
}
const char* MT32EmuMidiDriver::modelLabel(Model model) {
    switch(model) {
    case Model::Early:return "Original MT-32 - Early";
    case Model::Later:return "Original MT-32 - Later";
    case Model::CM32L:return "Original CM-32L / LAPC-I";
    default:return "MT32Emu (legacy automatic ROM choice)";
    }
}
bool MT32EmuMidiDriver::isExplicitDriver(std::string_view name) {
    std::string value(name);
    for(Model model:{Model::Early,Model::Later,Model::CM32L})
        if(!Pentagram::strcasecmp(value.c_str(),identifier(model))) return true;
    return false;
}
MT32EmuMidiDriver::MT32EmuMidiDriver() : MT32EmuMidiDriver(Model::LegacyAuto) {}
MT32EmuMidiDriver::MT32EmuMidiDriver(Model model)
    : LowLevelMidiDriver(std::string(identifier(model))), requested_model(model), mt32(nullptr), mt32src(nullptr) {}
MT32EmuMidiDriver::~MT32EmuMidiDriver() { destroyMidiDriver(); close(); }

const MachineConfiguration* MT32EmuMidiDriver::compatibleMachine(Model model,const ROMInfo* control,const ROMInfo* pcm) {
    if(!control || !pcm || control->type!=ROMInfo::Control || pcm->type!=ROMInfo::PCM
        || control->pairType!=ROMInfo::Full || pcm->pairType!=ROMInfo::Full) return nullptr;
    for(auto machine=MachineConfiguration::getAllMachineConfigurations();*machine;++machine) {
        std::string_view id=(*machine)->getMachineID();
        bool matches=model==Model::LegacyAuto
            || (model==Model::Early && id.substr(0,7)=="mt32_1_")
            || (model==Model::Later && id.substr(0,7)=="mt32_2_")
            || (model==Model::CM32L && id.substr(0,6)=="cm32l_");
        if(!matches) continue;
        bool found_control=false,found_pcm=false;
        for(auto rom=(*machine)->getCompatibleROMInfos();*rom;++rom) {
            found_control|=*rom==control; found_pcm|=*rom==pcm;
        }
        if(found_control && found_pcm) return *machine;
    }
    return nullptr;
}
int MT32EmuMidiDriver::failure(int code,const std::string& message) {
    initialization_error=std::string(modelLabel(requested_model))+": "+message;
    std::cerr<<initialization_error<<". No requested Roland model is active.\n";
    close(); return code;
}
int MT32EmuMidiDriver::openExplicit() {
    try {
        roms=std::make_unique<ROMSet>(); roms->discover();
        // Full images precede merged images. Within those groups, existing
        // bundle/data/savehome precedence and sorted filenames break ties.
        for(const auto& control:roms->images) {
            for(const auto& pcm:roms->images) {
                auto machine=compatibleMachine(requested_model,control->getROMInfo(),pcm->getROMInfo());
                if(!machine) continue;
                roms->control=control.get(); roms->pcm=pcm.get(); roms->machine=machine;
                return initializeSynth(roms->control,roms->pcm);
            }
        }
        std::string required=requested_model==Model::CM32L?"stock CM-32L control + CM-32L PCM":
            requested_model==Model::Later?"stock MT-32 v2.xx control + MT-32 PCM":"stock MT-32 v1.xx control + MT-32 PCM";
        return failure(2,"no matching ROM pair; requires "+required+". Munt identified: "+roms->summary()
            +"; unrecognized ROM-named files: "+std::to_string(roms->unrecognized));
    } catch(const std::exception&) {
        return failure(2,"ROM discovery or initialization failed; supply a compatible recognized control/PCM pair");
    }
}


/*
 *	This file open proc redirects writes to a writable directory
 *	and looks in this and other directories for the MT32 data.
 */

static bool openROMFile(FileStream& file, const std::string& filename, bool writable) {
	std::string basedir;
	if (!writable) {
		// May be in bundle.
		if (is_system_path_defined("<BUNDLE>")) {
			basedir = "<BUNDLE>/" + filename;
			if (file.open(get_system_path(basedir).c_str())) {
				return true;
			}
		}
		// Now try data dir.
		basedir = "<DATA>/" + filename;
		if (file.open(get_system_path(basedir).c_str())) {
			return true;
		}
		// We now fall back to a writable data dir, as the emulator
		// may have written something there.
	}
	basedir = "<SAVEHOME>/data/" + filename;
	return file.open(get_system_path(basedir).c_str());
}

static const ROMImage* getROM(FileStream& file, const std::string& filename) {
	if (openROMFile(file, filename, false)) {
		return ROMImage::makeROMImage(&file);
	}
	return nullptr;
}

int MT32EmuMidiDriver::open() {
	initialization_error.clear(); active_machine.clear(); rom_identification.clear();
	// Must be stereo
	if (!stereo) {
		return failure(1,"audio output must be stereo");
	}

	if(requested_model!=Model::LegacyAuto) return openExplicit();
	roms=std::make_unique<ROMSet>();
	// Make sure dir exists; this is the dir where data will be saved.
	U7mkdir("<SAVEHOME>/data", 0755);

	roms->files.push_back(std::make_unique<FileStream>());
	FileStream& controlROMFile=*roms->files.back();
	const ROMImage* controlROMImage;
	controlROMImage = getROM(controlROMFile, "CM32L_CONTROL.ROM");
	if (!controlROMImage) {
		controlROMImage = getROM(controlROMFile, "MT32_CONTROL.ROM");
	}
	if (!controlROMImage) {
		FileStream part1;
		FileStream part2;
		if (openROMFile(part1, "MT32A.BIN", false) && openROMFile(part2, "MT32B.BIN", false)) {
			auto pOut = U7open_out("<SAVEHOME>/data/MT32_CONTROL.ROM", false);
			if (pOut) {
				auto&        out   = *pOut;
				const Bit8u* data1 = part1.getData();
				const Bit8u* data2 = part2.getData();
				for (size_t ii = 0; ii < std::min(part1.getSize(), part2.getSize()); ii++) {
					out.put(static_cast<char>(data1[ii]));
					out.put(static_cast<char>(data2[ii]));
				}
				controlROMImage = getROM(controlROMFile, "MT32_CONTROL.ROM");
			}
		}
	}
	if (!controlROMImage) {
		return failure(2,"failed to open a legacy control ROM");
	}

	roms->images.emplace_back(controlROMImage);
	roms->files.push_back(std::make_unique<FileStream>());
	FileStream& pcmROMFile=*roms->files.back();
	const ROMImage* pcmROMImage;
	pcmROMImage = getROM(pcmROMFile, "CM32L_PCM.ROM");
	if (!pcmROMImage) {
		pcmROMImage = getROM(pcmROMFile, "MT32_PCM.ROM");
	}
	if (!pcmROMImage) {
		return failure(3,"failed to open a legacy PCM ROM");
	}

	roms->images.emplace_back(pcmROMImage);
	roms->control=controlROMImage; roms->pcm=pcmROMImage;
	roms->machine=compatibleMachine(Model::LegacyAuto,controlROMImage->getROMInfo(),pcmROMImage->getROMInfo());
	return initializeSynth(controlROMImage,pcmROMImage);
}

int MT32EmuMidiDriver::initializeSynth(const ROMImage* controlROMImage,const ROMImage* pcmROMImage) {
	mt32 = new Synth(nullptr);

	if (!mt32->open(*controlROMImage, *pcmROMImage)) return failure(4,"Munt rejected the control/PCM ROM pair");
	if (mt32->getStereoOutputSampleRate() != sample_rate) {
		if (SampleRateConverter::getSupportedOutputSampleRate(sample_rate) == 0)
			return failure(5,"Munt does not support the requested output sample rate");

		mt32src = new SampleRateConverter(*mt32, sample_rate, SamplerateConversionQuality_GOOD);
	}
	//  Must use DELAY_SHORT_MESSAGES_ONLY or IMMEDIATE to allow immediate
	//  processing of sysex
	mt32->setMIDIDelayMode(MT32Emu::MIDIDelayMode_DELAY_SHORT_MESSAGES_ONLY);

	const auto* control=controlROMImage->getROMInfo(); const auto* pcm=pcmROMImage->getROMInfo();
	rom_identification=std::string(control?control->shortName:"unknown control")+" + "+(pcm?pcm->shortName:"unknown PCM");
	active_machine=roms->machine?roms->machine->getMachineID():"legacy unidentified machine";
	std::cout<<"Roland requested: "<<modelLabel(requested_model)<<"; active Munt machine: "<<active_machine
		<<"; ROMs: "<<rom_identification<<"; initialization successful; partials="<<mt32->getPartialCount()
		<<"; ROM-selected reverb="<<(mt32->isDefaultReverbMT32Compatible()?"old MT-32":"revised LA")<<std::endl;
	return 0;
}

void MT32EmuMidiDriver::close() {
	if (mt32src) {
		delete mt32src;
		mt32src = nullptr;
	}
	if (mt32) {
		mt32->close();
		delete mt32;
		mt32 = nullptr;
	}
	roms.reset(); active_machine.clear(); rom_identification.clear();
}

void MT32EmuMidiDriver::send(uint32 b) {
	if(mt32) mt32->playMsg(b);
}

void MT32EmuMidiDriver::send_sysex(uint8 status, const uint8* msg, uint16 length) {
	if (!msg || !length) {
		return;
	}

	if (status != 0xF0 && msg[length - 1] != 0xF7) {
		return;
	}

	if(mt32) mt32->playSysexWithoutFraming(msg, length - 1);
}

void MT32EmuMidiDriver::lowLevelProduceSamples(sint16* samples, uint32 num_samples) {
	if (mt32src) {
		mt32src->getOutputSamples(samples, num_samples);
	} else if(mt32) {
		mt32->render(samples, num_samples);
	} else {
		std::fill_n(samples,num_samples*(stereo?2:1),0);
	}
}

#endif    // USE_MT32EMU_MIDI
