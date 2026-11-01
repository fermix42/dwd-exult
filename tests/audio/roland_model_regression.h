// Retained MT32Emu, profile compatibility and independent synthesized SFX checks.
#include "MT32EmuMidiDriver.h"
#include "Configuration.h"
#include "OriginalSfx.h"
#include "U7fileman.h"
#include <filesystem>
#include <fstream>
class RolandModelRegression {
    using Driver=MT32EmuMidiDriver;
    static double energy(const std::vector<sint16>& pcm) {
        double sum=0;for(auto v:pcm)sum+=double(v)*v;return sum;
    }
public:
    static void run() {
        std::cout << std::unitbuf;
        unsigned roland=0;
        for(unsigned i=0;i<MidiDriver::getDriverCount();++i) {
            auto name=MidiDriver::getDriverName(i),label=MidiDriver::getDriverDisplayName(i);
            require(name!="MT32EmuEarly" && name!="MT32EmuLater" && name!="MT32EmuCM32L","obsolete profiles absent");
            if(name=="MT32Emu"){++roland;require(label=="MT32EMU","retained Roland label");}
            if(name=="OriginalOPL2")require(label=="OPL2 (SB/ADLIB)","exact OPL2 label/stable ID");
            if(name=="OPL3Stereo")require(label=="Enhanced OPL3 Stereo","OPL3 label/stable ID");
        }
        require(roland==1,"one emulated Roland choice");
        for(const char* old:{"MT32EmuEarly","MT32EmuLater","MT32EmuCM32L","mt32emuearly","MT32EMULATER"}) {
            Configuration cfg;cfg.set("config/audio/midi/driver",old,false);cfg.set("config/audio/effects/driver","FMOpl",false);
            std::string name;cfg.value("config/audio/midi/driver",name,"");
            cfg.set("config/audio/midi/driver",MidiDriver::canonicalDriverName(name),false);
            Configuration restart;require(restart.read_config_string(cfg.dump()),"config restart");
            restart.value("config/audio/midi/driver",name,"");require(name=="MT32Emu","obsolete config maps and persists");
            restart.value("config/audio/effects/driver",name,"");require(name=="FMOpl","SFX selection independent");
        }
        for(const char* retained:{"OriginalOPL2","OPL3Stereo","MT32Emu","Windows","FluidSynth","default","disabled","custom"})
            require(MidiDriver::canonicalDriverName(retained)==retained,"unrelated IDs unchanged");
        std::cout<<"PASS profile registry, exact labels, case-insensitive aliases, persisted restart and separate SFX configuration\n";
        namespace fs=std::filesystem;
        const char* output=std::getenv("EXULT_AUDIO_TEST_OUTPUT");require(output,"disposable audio output required");
        fs::path scratch=fs::u8path(output)/"mt32-rom-fixture";require(!fs::exists(scratch),"fresh ROM fixture");fs::create_directories(scratch/"data");
        std::vector<std::pair<std::string,std::string>> paths;
        for(const char* key:{"<DATA>","<BUNDLE>","<SAVEHOME>","<STATIC>"})paths.emplace_back(key,is_system_path_defined(key)?get_system_path(key):"");
        auto restore=[&]{for(const auto& p:paths){if(p.second.empty())clear_system_path(p.first);else add_system_path(p.first,p.second);}};
        try {
            for(const char* key:{"<DATA>","<BUNDLE>","<SAVEHOME>"})add_system_path(key,scratch.u8string());
            {Driver invalid;invalid.setSfxSynchronous(true);require(invalid.initMidiDriver(32000,true)!=0,"missing ROM fails safely");invalid.destroyMidiDriver();}
            const char* romroot=std::getenv("EXULT_ROLAND_ROM_DIR");
            if(romroot) {
            for(auto pair:{std::pair<const char*,const char*>{"mt32_ctrl_1_07.rom","mt32_pcm.rom"},{"mt32_ctrl_2_07.rom","mt32_pcm.rom"},{"cm32l_ctrl_1_02.rom","cm32l_pcm.rom"}}) {
                fs::copy_file(fs::u8path(romroot)/pair.first,scratch/"MT32_CONTROL.ROM",fs::copy_options::overwrite_existing);
                fs::copy_file(fs::u8path(romroot)/pair.second,scratch/"MT32_PCM.ROM",fs::copy_options::overwrite_existing);
                for(int pass=0;pass<2;++pass) {
                    Driver d;d.setSfxSynchronous(true);require(d.initMidiDriver(32000,true)==0,"compatible named ROM pair initializes");
                    d.startSequence(0,events({event(0,0xc1,0,0),event(0,0xb1,7,100),event(0,0x91,60,100,12),event(24,0xff,0x2f,0)}),false,255,-1);
                    d.yield();
                    std::vector<sint16> pcm(32000);d.produceSamples(pcm.data(),unsigned(pcm.size()*sizeof(sint16)));
                    require(energy(pcm)>0,"retained MT32EMU produces PCM");
                    d.send(0x81|(60<<8));d.destroyMidiDriver();
                }
                std::cout<<"PASS retained MT32EMU ROM playback and restart "<<pair.first<<"\n";
            }
            fs::remove(scratch/"MT32_CONTROL.ROM");fs::remove(scratch/"MT32_PCM.ROM");
            fs::copy_file(fs::u8path(romroot)/"mt32_ctrl_1_07_a.rom",scratch/"MT32A.BIN");
            fs::copy_file(fs::u8path(romroot)/"mt32_ctrl_1_07_b.rom",scratch/"MT32B.BIN");
            fs::copy_file(fs::u8path(romroot)/"mt32_pcm.rom",scratch/"MT32_PCM.ROM");
            {Driver d;d.setSfxSynchronous(true);require(d.initMidiDriver(32000,true)==0,"upstream interleaved control ROM loader preserved");d.destroyMidiDriver();}
            } else std::cout<<"NOT TESTED retained MT32EMU PCM/split-ROM playback: EXULT_ROLAND_ROM_DIR not supplied\n";
            for(bool si:{false,true}) {
                if(U7FileManager::get_ptr())U7FileManager::get_ptr()->reset();
                add_system_path("<STATIC>",si?"C:/GOG Games/Ultima 7 - Serpent Isle/static":"C:/GOG Games/Ultima 7/static");
                double sum=0;
                for(int id=0;id<8;++id) {
                    std::unique_ptr<OriginalSfx::Engine> engine;
                    if(si)engine=std::make_unique<OriginalSfx::SiXmidi>(true,id,32000);
                    else engine=std::make_unique<OriginalSfx::BgAdlib>(OriginalSfx::record("<STATIC>/adlibsfx.dat",id),32000);
                    std::vector<sint16> pcm(4096);
                    for(int frame=0;frame<20;++frame){if(!engine->render(pcm.data(),2048))break;sum+=energy(pcm);}
                }
                require(sum>0,"independent OPL2 SFX produces PCM");
                std::cout<<"PASS independent OPL2 SFX PCM "<<(si?"SI":"BG")<<"\n";
            }
        }catch(...){restore();throw;}restore();
        if(U7FileManager::get_ptr())U7FileManager::get_ptr()->reset();
        // No fixture or proprietary ROM is packaged; retained locally as ignored test evidence.
    }
};
