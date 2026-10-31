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
#	include "OriginalRolandMusic.h"
#	include "databuf.h"
#	include "sha1/sha1.h"
#	include "XMidiEventList.h"
#	include "XMidiFile.h"

#	include <cstring>
#	include <algorithm>
#	include <filesystem>
#	include <set>
#	include <vector>
#	include <stdexcept>
#	include <functional>

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
    music.reset();music_ready=false;music_wait=0;music_frames=0;music_sysex.clear();
}

void MT32EmuMidiDriver::send(uint32 b) {
    if(original_music){
        auto lock=LockMutex();
        if(music && music_ready){
            size_t first=music->output.size();music->event(b);
            // In AIL's service, XmidiWait defers the stall until the complete
            // channel event returns (e.g. patch write AND program resend).
            // Main-thread bank initialization retains waits between blocks.
            if(!music_classic && music->output.size()>first){
                unsigned wait=0;for(size_t i=first;i<music->output.size();++i){wait+=music->output[i].retraces;music->output[i].retraces=0;}
                music->output.back().retraces=wait;
            }
        }
        return;
    }
	if(mt32) mt32->playMsg(b);
}

void MT32EmuMidiDriver::send_sysex(uint8 status, const uint8* msg, uint16 length) {
    if(original_music) {
        auto lock=LockMutex();
        if(!score_sysex || !music || !music_ready || !msg || !length)return;
        if(status==0xf0)music_sysex={0xf0};
        if(status!=0xf0 && status!=0xf7)return;
        if(music_sysex.empty()) { if(msg[0]!=0xf0)return; }
        music_sysex.insert(music_sysex.end(),msg,msg+length);
        if(music_sysex.size()>65536){music_sysex.clear();std::cerr<<"Original Roland: oversized SysEx ignored\n";return;}
        if(music_sysex.back()==0xf7){music->output.push_back({std::move(music_sysex),0});music_sysex.clear();}
        return;
    }
	if (!msg || !length) {
		return;
	}

	if (status != 0xF0 && msg[length - 1] != 0xF7) {
		return;
	}

	if(mt32) mt32->playSysexWithoutFraming(msg, length - 1);
}

void MT32EmuMidiDriver::renderSynth(sint16* samples, uint32 num_samples) {
	if (mt32src) {
		mt32src->getOutputSamples(samples, num_samples);
	} else if(mt32) {
		mt32->render(samples, num_samples);
	} else {
		std::fill_n(samples,num_samples*(stereo?2:1),0);
	}
}

namespace {
std::vector<uint8> rolandBytes(IDataSource* source) {
    if(!source || !source->good() || source->getSize()>16*1024*1024)throw std::runtime_error("missing or oversized original music resource");
    std::vector<uint8> bytes(source->getSize());source->seek(0);source->read(bytes.data(),unsigned(bytes.size()));source->seek(0);return bytes;
}
std::vector<uint8> rolandValidated(IDataSource* source,unsigned size,const char* hash) {
    auto data=rolandBytes(source);
    if(data.size()!=size || std::string(sha1::toHexString(sha1::calc(data.data(),int(data.size()))).data())!=hash)
        throw std::runtime_error("unrecognized original Roland driver/data version");
    return data;
}
}
bool MT32EmuMidiDriver::setMusicContext(bool bg,bool menu,bool intro,bool ending) {
    auto lock=LockMutex();
    bool old_original=original_music,old_classic=music_classic,old_ail=music_new_ail;
    original_music=requested_model!=Model::LegacyAuto;
    music_classic=(bg && !ending) || (!bg && menu);
    music_new_ail=!bg && !menu && !intro && !ending;
    return old_original!=original_music || old_classic!=music_classic || old_ail!=music_new_ail;
}
void MT32EmuMidiDriver::loadTimbreLibrary(IDataSource* source,TimbreLibraryType type) {
    if(!original_music){LowLevelMidiDriver::loadTimbreLibrary(source,type);return;}
    for(int i=0;i<maxSequences();++i)finishSequence(i);
    if(!waitTillNoComMessages())return;
    auto lock=LockMutex();music_ready=false;music.reset();music_frames=0;music_wait=0;music_sysex.clear();
    try {
        auto policy=std::make_unique<OriginalRolandMusic>(music_classic);
        auto bank=rolandBytes(source);
        if(music_classic) {
            if(type!=TIMBRE_LIBRARY_U7VOICE_MT)throw std::runtime_error("unsupported classic Roland bank");
            policy->initializeClassic(bank);
        } else {
            if(type!=TIMBRE_LIBRARY_XMIDI_MT)throw std::runtime_error("unsupported AIL Roland bank");
            std::vector<uint8> driver;
            if(music_new_ail){
                IExultDataSource data("<STATIC>/snddrvrs.dat",1);driver=rolandValidated(&data,10707,"cea64dbdc3c655f5677f892d7e5b33173903d4f4");
                IFileDataSource executable("<STATIC>/../si.exe");auto bytes=rolandValidated(&executable,754240,"aae950c362e041d175c244815f1c74609e7dc5a8");
                std::copy_n(bytes.begin()+306240,71,music_volumes.begin());
            } else {IFileDataSource data("<STATIC>/mt32mpu.adv");driver=rolandValidated(&data,10424,"e71da3eefe6c4806495fd334a554ad41dbf7d19a");}
            policy->loadAilBank(bank);policy->initializeAil(driver,music_new_ail);
            policy->relativeVolume=90;
        }
        music=std::move(policy);music_ready=true;
        if(music_new_ail){
            IExultDataSource initialization("<STATIC>/mt32mus.dat",70);
            prepareMusicTrack(&initialization,0,-1);
            XMidiFile score(&initialization,XMIDIFILE_CONVERT_NOCONVERSION,getName(),true,true);
            auto* events=score.GetEventList(0);if(!events)throw std::runtime_error("missing SI Roland initialization sequence 70");
            uploading_timbres=false;
            LowLevelMidiDriver::startSequence(3,events,false,255);
            uploading_timbres=true;
        }
    } catch(const std::exception& e){music_ready=false;std::cerr<<"Original Roland music unavailable: "<<e.what()<<"; no substitute music profile activated\n";}
}
void MT32EmuMidiDriver::prepareMusicTrack(IDataSource* source,int sequence,int track) {
    if(!original_music || !music || !music_ready)return;
    if(!waitTillNoComMessages()){music_ready=false;std::cerr<<"Original Roland: previous sequence did not stop; refusing replacement\n";return;}
    auto lock=LockMutex();
    if(music_new_ail)music->relativeVolume=track>=0 && unsigned(track)<music_volumes.size()?music_volumes[track]:90;
    if(music_classic)return;
    // Read only the existing IFF TIMB directory; event parsing remains XMidiFile's job.
    auto bytes=rolandBytes(source);
    auto be=[&](unsigned at){return (unsigned(bytes[at])<<24)|(unsigned(bytes[at+1])<<16)|(unsigned(bytes[at+2])<<8)|bytes[at+3];};
    int form=-1;
    std::function<void(unsigned,unsigned)> walk=[&](unsigned begin,unsigned end){
        for(unsigned at=begin;at+8<=end;){
            unsigned size=be(at+4);if(size>end-at-8)return;
            bool container=!std::memcmp(bytes.data()+at,"FORM",4)||!std::memcmp(bytes.data()+at,"CAT ",4);
            if(container && size>=4){if(!std::memcmp(bytes.data()+at,"FORM",4)&&!std::memcmp(bytes.data()+at+8,"XMID",4))++form;walk(at+12,at+8+size);}
            else if(form==sequence && !std::memcmp(bytes.data()+at,"TIMB",4) && size>=2){
                unsigned count=bytes[at+8]|(unsigned(bytes[at+9])<<8);
                if(count>(size-2)/2)throw std::runtime_error("invalid Roland TIMB count");
                for(unsigned i=0;i<count;++i)music->install(bytes[at+11+i*2],bytes[at+10+i*2]);
            }
            at+=8+size+(size&1);
        }
    };
    walk(0,unsigned(bytes.size()));
}
void MT32EmuMidiDriver::startSequence(int index,XMidiEventList* list,bool repeat,int volume,int branch) {
    if(original_music && !music_ready){std::cerr<<"Original Roland: music start rejected because original bank initialization failed\n";return;}
    LowLevelMidiDriver::startSequence(index,list,repeat,volume,branch);
}
bool MT32EmuMidiDriver::isRolandWaiting() const {
    if(!original_music)return false;
    auto lock=const_cast<MT32EmuMidiDriver*>(this)->LockMutex();
    return music_wait || (music && std::any_of(music->output.begin(),music->output.end(),[](const auto& c){return c.retraces!=0;}));
}
uint32 MT32EmuMidiDriver::getTickCount(uint16 sequence) {return original_music?uint32(music_frames*6000/sample_rate):LowLevelMidiDriver::getTickCount(sequence);}
void MT32EmuMidiDriver::sequenceSendSysEx(uint16 sequence,uint8 status,const uint8* bytes,uint16 size) {
    if(original_music){if(status!=0xff){score_sysex=true;send_sysex(status,bytes,size);score_sysex=false;}}
    else LowLevelMidiDriver::sequenceSendSysEx(sequence,status,bytes,size);
}
void MT32EmuMidiDriver::lowLevelProduceSamples(sint16* samples,uint32 frames) {
    if(!original_music){renderSynth(samples,frames);return;}
    auto lock=LockMutex();
    if(!music || !music_ready){std::fill_n(samples,frames*2,0);return;}
    unsigned remaining=frames;
    while(remaining){
        while(!music_wait && !music->output.empty()){
            auto command=std::move(music->output.front());music->output.pop_front();
            auto& b=command.bytes;
            if(mt32 && b.size()>=2){
                if(b[0]==0xf0 && b.back()==0xf7)mt32->playSysexWithoutFraming(b.data()+1,unsigned(b.size()-2));
                else mt32->playMsg(unsigned(b[0])|(unsigned(b[1])<<8)|(b.size()>2?unsigned(b[2])<<16:0));
            }
            // Original AIL retrace waits are 14,286 microseconds, not arbitrary host sleeps.
            music_wait=(uint64(command.retraces)*14286*sample_rate+999999)/1000000;
        }
        unsigned count=music_wait?unsigned(std::min<uint64>(remaining,music_wait)):remaining;
        renderSynth(samples,count);
        if(music_wait)music_wait-=count;else music_frames+=count;
        int gain=LowLevelMidiDriver::getGlobalVolume();
        if(gain!=100)for(unsigned i=0;i<count*2;++i)samples[i]=sint16(int(samples[i])*gain/100);
        samples+=count*2;remaining-=count;
    }
}

#endif    // USE_MT32EMU_MIDI
