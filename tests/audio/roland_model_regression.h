// Scoped Roland model tests; metadata fixtures contain no proprietary ROM bytes.
#include "MT32EmuMidiDriver.h"
#include "Configuration.h"
#include <filesystem>
#include <fstream>
class RolandModelRegression {
    using Driver=MT32EmuMidiDriver;
    using Model=Driver::Model;
    static const MT32Emu::ROMInfo* info(const char* name) {
        for(auto p=MT32Emu::ROMInfo::getAllROMInfos();*p;++p)
            if(std::string_view((*p)->shortName)==name) return *p;
        throw std::runtime_error("missing Munt metadata fixture");
    }
public:
    static void run() {
        using namespace MT32Emu;
        auto pcm=info("pcm_mt32"),cm=info("pcm_cm32l");
        unsigned pairs=0;
        for(auto p=ROMInfo::getFullROMInfos();*p;++p) {
            if((*p)->type!=ROMInfo::Control) continue;
            std::string_view name=(*p)->shortName;
            Model model=name.substr(0,12)=="ctrl_mt32_1_"?Model::Early:
                name.substr(0,12)=="ctrl_mt32_2_"?Model::Later:
                name.substr(0,11)=="ctrl_cm32l_"?Model::CM32L:Model::LegacyAuto;
            for(Model selected:{Model::Early,Model::Later,Model::CM32L}) {
                require(bool(Driver::compatibleMachine(selected,*p,pcm))==(model==selected && selected!=Model::CM32L),"stock MT-32 family policy");
                require(bool(Driver::compatibleMachine(selected,*p,cm))==(model==selected && selected==Model::CM32L),"stock CM-32L family policy");
                require(!Driver::compatibleMachine(selected,*p,nullptr),"missing PCM rejected");
                require(!Driver::compatibleMachine(selected,nullptr,pcm),"missing control rejected");
            }
            if(model!=Model::LegacyAuto) ++pairs;
        }
        require(pairs>=10,"all stock revisions covered by Munt metadata");
        for(auto p=ROMInfo::getPartialROMInfos();*p;++p)
            require(!Driver::compatibleMachine(Model::Early,*p,pcm),"unmerged partial cannot initialize");
        Configuration saved;
        for(Model m:{Model::LegacyAuto,Model::Early,Model::Later,Model::CM32L}) {
            const std::string id=Driver::identifier(m);
            saved.set("config/audio/midi/driver",id,false);
            Configuration loaded;require(loaded.read_config_string(saved.dump()),"CFG reload");
            std::string value;loaded.value("config/audio/midi/driver",value,"");require(value==id,"CFG model persistence");
            bool registered=false;
            for(int i=0;i<MidiDriver::getDriverCount();++i) registered|=MidiDriver::getDriverName(i)==id;
            require(registered,"model registered");
            require(Driver::isExplicitDriver(id)==(m!=Model::LegacyAuto),"legacy ID remains distinct");
        }
        // Disposable invalid data; it is deliberately not claimed to be a real ROM.
        namespace fs=std::filesystem;
        auto scratch=fs::temp_directory_path()/"exult-roland-model-regression";
        require(!fs::exists(scratch),"scratch must be new");fs::create_directory(scratch);
        std::vector<std::pair<std::string,std::string>> paths;
        for(const char* key:{"<DATA>","<BUNDLE>","<SAVEHOME>"}) {
            paths.emplace_back(key,is_system_path_defined(key)?get_system_path(key):"");
            add_system_path(key,scratch.u8string());
        }
        auto restore=[&]{for(const auto& p:paths) {if(p.second.empty()) clear_system_path(p.first);else add_system_path(p.first,p.second);}fs::remove(scratch/"MT32_CONTROL.ROM");fs::remove(scratch);};
        try {
            std::ofstream invalid(scratch/"MT32_CONTROL.ROM",std::ios::binary);std::string zeros(65536,'\0');invalid.write(zeros.data(),zeros.size());invalid.close();
            for(Model m:{Model::Early,Model::Later,Model::CM32L}) {
                Driver d(m);d.setSfxSynchronous(true);require(d.initMidiDriver(32000,true)!=0,"invalid/missing ROM initialization fails");
                require(d.getActiveMachineID().empty()&&!d.getInitializationError().empty(),"failure reports inactive model");
                require(!MidiDriver::createInstance(Driver::identifier(m),32000,true),"explicit failure must not fall back");
                d.destroyMidiDriver();
            }
        } catch(...) {restore();throw;}
        restore();
        std::cout<<"PASS Roland metadata: all stock revisions, mismatches, missing halves, unsupported families; CFG/registry; invalid content and no fallback\n";
        // Optional real user-ROM smoke. No ROM copies or generated proprietary data.
        const char* root=std::getenv("EXULT_ROLAND_ROM_DIR");
        if(root) {
            std::vector<std::pair<std::string,std::string>> old;
            for(const char* key:{"<DATA>","<BUNDLE>","<SAVEHOME>"}) {old.emplace_back(key,is_system_path_defined(key)?get_system_path(key):"");add_system_path(key,root);}
            auto reset=[&]{for(const auto& p:old){if(p.second.empty())clear_system_path(p.first);else add_system_path(p.first,p.second);}};
            try {
                Driver d;d.setSfxSynchronous(true);
                for(int pass=0;pass<2;++pass) {
                    require(d.initMidiDriver(32000,true)==0,"legacy supplied pair initializes");
                    require(!d.getActiveMachineID().empty(),"actual machine reported");
                    require(!d.mt32->isActive(),"fresh synth has no stale notes");
                    static_cast<MidiDriver&>(d).setGlobalVolume(0);for(int i=0;i<240 && d.getGlobalVolume()!=0;++i)d.yield();
                    require(d.getGlobalVolume()==0,"master mute applies through existing command queue");
                    d.startSequence(0,events({event(0,0xc1,0,0),event(0,0xb1,7,100),event(0,0x91,60,100,12),event(24,0xff,0x2f,0)}),false,255,-1);
                    d.yield();std::vector<sint16> muted(32000);d.produceSamples(muted.data(),unsigned(muted.size()*sizeof(sint16)));
                    auto energy=[](const auto& pcm){double result=0;for(auto v:pcm)result+=double(v)*v;return result;};
                    // Munt's analog-output path may retain a DAC noise floor at CC7=0.
                    // Compare identical sequenced passages, not mathematical PCM zero.
                    Driver full;full.setSfxSynchronous(true);require(full.initMidiDriver(32000,true)==0,"volume comparison initializes");
                    full.startSequence(0,events({event(0,0xc1,0,0),event(0,0xb1,7,100),event(0,0x91,60,100,12),event(24,0xff,0x2f,0)}),false,255,-1);
                    full.yield();std::vector<sint16> audible(32000);full.produceSamples(audible.data(),unsigned(audible.size()*sizeof(sint16)));
                    std::cout<<"Roland sequenced energy muted="<<energy(muted)<<" audible="<<energy(audible)<<"\n";
                    require(energy(audible)>0 && energy(muted)*100<energy(audible),"sequenced master mute attenuates output");
                    full.finishSequence(0);full.yield();full.destroyMidiDriver();
                    d.finishSequence(0);d.yield();static_cast<MidiDriver&>(d).setGlobalVolume(100);for(int i=0;i<240 && d.getGlobalVolume()!=100;++i)d.yield();
                    require(d.getGlobalVolume()==100,"master volume restored");
                    d.send(0xc1);d.send(0xb1|(7<<8)|(100<<16));d.send(0x91|(60<<8)|(100<<16));
                    std::vector<sint16> audio(32000);d.lowLevelProduceSamples(audio.data(),16000);
                    require(std::any_of(audio.begin(),audio.end(),[](auto s){return s!=0;}),"real Munt note produces PCM");
                    d.send(0x81|(60<<8));
                    for(int n=0;n<24;++n)d.lowLevelProduceSamples(audio.data(),16000);
                    require(!d.mt32->isActive(),"note/reverb tail terminates");
                    d.destroyMidiDriver();require(!d.mt32&&d.getActiveMachineID().empty(),"shutdown releases actual synth");
                }
                for(Model model:{Model::Early,Model::Later,Model::CM32L}) {
                    Driver explicitDriver(model);explicitDriver.setSfxSynchronous(true);
                    if(explicitDriver.initMidiDriver(32000,true)!=0) {
                        require(explicitDriver.getActiveMachineID().empty(),"unavailable stock model remains inactive");
                        std::cout<<"SKIP stock synthesis (matching ROMs unavailable): "<<Driver::identifier(model)<<"\n";
                    } else {
                        auto identity=explicitDriver.getActiveMachineID();
                        explicitDriver.send(0xc1);explicitDriver.send(0x91|(60<<8)|(100<<16));
                        std::vector<sint16> pcm(32000);explicitDriver.lowLevelProduceSamples(pcm.data(),16000);
                        require(std::any_of(pcm.begin(),pcm.end(),[](auto v){return v!=0;}),"stock model produces PCM");
                        explicitDriver.send(0x81|(60<<8));
                        for(int i=0;i<24;++i)explicitDriver.lowLevelProduceSamples(pcm.data(),16000);
                        require(!explicitDriver.mt32->isActive(),"stock model note terminates");
                        explicitDriver.destroyMidiDriver();
                        require(explicitDriver.initMidiDriver(32000,true)==0 && explicitDriver.getActiveMachineID()==identity,"deterministic stock rediscovery");
                        require(!explicitDriver.mt32->isActive(),"stock restart has no stale notes");
                        explicitDriver.destroyMidiDriver();std::cout<<"PASS stock synthesis "<<identity<<"\n";
                    }
                }
                std::cout<<"PASS available legacy ROM synthesis: note-on/off, tail termination, shutdown/reinitialization\n";
            }catch(...){reset();throw;}reset();
        }
    }
};
