// Focused comparisons against executed, read-only original-driver sources.
#include "OriginalRolandMusic.h"
extern "C" {
bool rolandRefHasTimbre(unsigned,unsigned);void rolandRefClear();unsigned rolandRefCount();unsigned rolandRefSize(unsigned);unsigned rolandRefByte(unsigned,unsigned);unsigned rolandRefWait(unsigned);
void rolandRefClassic(void*);void rolandRefAil(const uint8_t*,bool);void rolandRefEvent(uint32_t,bool);void rolandRefInstall(unsigned,unsigned,const uint8_t*);
}
class RolandMusicRegression {
    using Driver=MT32EmuMidiDriver;
    static void compare(OriginalRolandMusic& policy,const char* label) {
        unsigned expected=0,actual=0;
        for(unsigned i=0;i<rolandRefCount();++i){
            // LCD text is not musical state and is deliberately not copied from game code.
            if(rolandRefSize(i)>8 && rolandRefByte(i,0)==0xf0 && rolandRefByte(i,5)==0x20)continue;
            require(actual<policy.output.size(),label);const auto& c=policy.output[actual++];
            require(c.bytes.size()==rolandRefSize(i),label);
            for(unsigned j=0;j<c.bytes.size();++j)if(c.bytes[j]!=rolandRefByte(i,j)){std::cerr<<label<<" message="<<i<<" byte="<<j<<" actual="<<unsigned(c.bytes[j])<<" reference="<<rolandRefByte(i,j)<<"\n";require(false,label);}
            require(c.retraces==rolandRefWait(i),"original-driver wait mismatch");++expected;
        }
        require(expected==policy.output.size(),"extra Roland output command");
        for(const auto& c:policy.output)if(c.bytes[0]==0xf0){
            unsigned sum=0;for(unsigned i=5;i<c.bytes.size()-1;++i)sum+=c.bytes[i];
            require((sum&127)==0,"Roland checksum");
        }
        policy.output.clear();rolandRefClear();
    }
    static void checkScore(const std::vector<uint8>& bytes,OriginalRolandMusic& policy,bool classic,const char* label,int track=0) {
        IBufferDataView input(bytes.data(),bytes.size());XMidiFile score(&input,XMIDIFILE_CONVERT_NOCONVERSION,"MT32EmuEarly",true,true);
        auto* list=score.GetEventList(track);require(list,"Roland original score parses");unsigned events=0;std::array<unsigned,16> banks{};
        for(auto* e=list->events;e;e=e->next){
            unsigned type=e->status&240;
            if(type>=0x80 && type<=0xe0 && (!classic || type==0x80 || type==0x90 || type==0xb0 || type==0xc0 || type==0xe0)){
                uint32 m=e->status|(unsigned(e->data[0])<<8)|(unsigned(e->data[1])<<16);
                if(!classic && type==0xb0 && e->data[0]==114)banks[e->status&15]=e->data[1];
                if(!classic && type==0xc0 && banks[e->status&15]){
                    unsigned group=banks[e->status&15],program=e->data[0];auto t=policy.timbres.find((group<<8)|program);
                    if(t!=policy.timbres.end()&&!rolandRefHasTimbre(group,program)){
                        std::vector<uint8> bytes(248);bytes[0]=248;std::copy(t->second.begin(),t->second.end(),bytes.begin()+2);
                        policy.install(group,program);rolandRefInstall(group,program,bytes.data());compare(policy,"source score required custom timbre");
                    }
                }
                policy.event(m);rolandRefEvent(m,classic);compare(policy,label);++events;
            }
        }
        std::cout<<"PASS Roland full source event translation "<<label<<" events="<<events<<"\n";
    }
public:
    static void run() {
        Configuration memory;auto oldConfig=config;config=&memory;
        struct RestoreConfig {Configuration* old;~RestoreConfig(){config=old;}} restoreConfig{oldConfig};
        struct RolandClock:ClockHandler {
            bool waiting=false;RolandClock():ClockHandler(true){}
            bool usesOriginalRolandTiming()const override{return true;}
            bool isRolandWaiting()const override{return waiting;}
        } clock;
        clock.observe=[&](uint32 m,uint64){if((m&65535)==(0xb1|(61<<8)))clock.waiting=true;};
        {
            XMidiSequence sequence(&clock,0,events({event(0,0xb1,61,1),event(0,0x91,60,100,1),event(0,0x91,60,90,3),event(5,0xff,0x2f,0)}),false,255,-1);
            for(int i=0;i<10;++i)sequence.playEvent();
            require(notes(clock).empty(),"Roland wait blocks same-tick notes");
            clock.waiting=false;pump(sequence,clock,250);
            auto played=notes(clock);require(played.size()==4,"Roland overlapping source notes keep independent releases");
            require(played[0].time==0 && played[1].time==0 && played[2].time==50 && played[3].time==150,"Roland preserves same-time ordering and full source gate lengths");
            sequence.finish();
        }
        std::cout<<"PASS Roland opt-in sequencer wait/same-tick order/overlap/release timing\n";
        const std::string bg="C:/GOG Games/Ultima 7/STATIC/",si="C:/GOG Games/Ultima 7 - Serpent Isle/STATIC/";
        auto classicBank=scoreEntry(scoreBytes(bg+"U7VOICE.FLX"),0);
        OriginalRolandMusic classic(true);classic.initializeClassic(classicBank);rolandRefClassic(classicBank.data());compare(classic,"classic initialization/bank mismatch");
        auto introBank=scoreEntry(scoreBytes(bg+"U7INTRO.TIM"),0);
        OriginalRolandMusic introduction(true);introduction.initializeClassic(introBank);rolandRefClassic(introBank.data());compare(introduction,"Guardian original timbre bank mismatch");
        auto driver=scoreBytes(si+"MT32MPU.ADV");
        OriginalRolandMusic ail(false);ail.initializeAil(driver,false);rolandRefAil(driver.data(),false);compare(ail,"AIL cinematic initialization mismatch");
        auto gameplay=scoreEntry(scoreBytes(si+"SNDDRVRS.DAT"),1);
        OriginalRolandMusic newer(false);newer.initializeAil(gameplay,true);rolandRefAil(gameplay.data(),true);compare(newer,"AIL gameplay initialization mismatch");
        unsigned cases=0;
        for(unsigned channel=0;channel<16;++channel){
            uint32 program=0xc0|channel|(40<<8);newer.event(program);rolandRefEvent(program,false);compare(newer,"AIL program mismatch");
            for(unsigned control=0;control<128;++control)for(unsigned value:{0u,31u,64u,127u}) {
                if(control==58 && value<24)continue;
                if(control>=35 && control<=46 && (control-32)%5>=3){
                    unsigned base=32+(control-32)/5*5;
                    for(unsigned op=0;op<3;++op){uint32 setup=0xb0|channel|((base+op)<<8)|((op==0?8u:op==2?112u:0u)<<16);newer.event(setup);rolandRefEvent(setup,false);compare(newer,"queue address setup");}
                }
                uint32 m=0xb0|channel|(control<<8)|(value<<16);
                newer.event(m);rolandRefEvent(m,false);compare(newer,"AIL controller translation mismatch");++cases;
            }
            for(unsigned bend:{0u,1u,8191u,8192u,16383u}){
                uint32 m=0xe0|channel|((bend&127)<<8)|((bend>>7)<<16);newer.event(m);rolandRefEvent(m,false);compare(newer,"AIL pitch data mismatch");++cases;
            }
        }
        ail.loadAilBank(scoreBytes(si+"XMIDI.MT"));
        // Install actual original timbres, in source-bank order, before score delivery.
        unsigned installed=0;
        for(const auto& item:ail.timbres){
            unsigned group=item.first>>8,program=item.first&255;
            if(group==0 || group==127)continue;
            std::vector<uint8> bytes(248);bytes[0]=248;std::copy(item.second.begin(),item.second.end(),bytes.begin()+2);
            ail.install(group,program);rolandRefInstall(group,program,bytes.data());compare(ail,"AIL five-part upload/cache/patch mismatch");++installed;
        }
        checkScore(scoreEntry(scoreBytes(bg+"MT32MUS.DAT"),32),classic,true,"BG gameplay/Music Player track32");
        checkScore(scoreEntry(scoreBytes(bg+"INTRORDM.DAT"),2),introduction,true,"BG Guardian");
        checkScore(scoreEntry(scoreBytes(bg+"INTRORDM.DAT"),0),introduction,true,"BG opening intro");
        checkScore(scoreEntry(scoreBytes(bg+"INTRORDM.DAT"),3),classic,true,"BG menu");
        auto menuBank=scoreEntry(scoreBytes(si+"MAINMENU.TIM"),0);OriginalRolandMusic menu(true);menu.initializeClassic(menuBank);rolandRefClassic(menuBank.data());compare(menu,"SI menu initialization");
        checkScore(scoreEntry(scoreBytes(si+"MAINSHP.FLX"),28),menu,true,"SI menu");
        checkScore(scoreEntry(scoreBytes(si+"MT32MUS.DAT"),70),ail,false,"SI initialization record70");
        checkScore(scoreEntry(scoreBytes(si+"MT32MUS.DAT"),13),ail,false,"SI gameplay track13");
        checkScore(scoreBytes(si+"R_SINTRO.XMI"),ail,false,"SI original intro");
        checkScore(scoreBytes("data/si/ext_intro_r.xmi"),ail,false,"SI extended intro");
        checkScore(scoreBytes(si+"R_SEND.XMI"),ail,false,"SI ending");
        OriginalRolandMusic endingPolicy(false);endingPolicy.loadAilBank(scoreBytes(bg+"XMIDI.MT"));endingPolicy.initializeAil(driver,false);rolandRefAil(driver.data(),false);compare(endingPolicy,"BG ending initialization");
        checkScore(scoreBytes(bg+"ENDSCORE.XMI"),endingPolicy,false,"BG ending part1",1);
        checkScore(scoreBytes(bg+"ENDSCORE.XMI"),endingPolicy,false,"BG ending part2",2);
        std::cout<<"PASS Roland original references: controller/pitch cases="<<cases<<" original timbres="<<installed<<"\n";
        // Actual backend lifecycle and sample-time waits, without the application or audio device.
        const char* roms=std::getenv("EXULT_ROLAND_ROM_DIR");require(roms,"set EXULT_ROLAND_ROM_DIR for stock synthesis");
        const bool hadStatic=is_system_path_defined("<STATIC>");auto oldStatic=get_system_path("<STATIC>");
        add_system_path("<DATA>",roms);add_system_path("<SAVEHOME>",roms);
        for(auto model:{Driver::Model::Early,Driver::Model::Later,Driver::Model::CM32L}) {
            Driver d(model);d.setSfxSynchronous(true);require(d.initMidiDriver(32000,true)==0,"Roland model initialization");
            for(int context=0;context<9;++context){
                bool isBg=context<4;bool menu=context==2 || context==8;bool intro=context==1 || context==5 || context==6;bool ending=context==3 || context==7;
                add_system_path("<STATIC>",isBg?bg:si);d.setMusicContext(isBg,menu,intro,ending);
                auto bank=isBg?(ending?scoreBytes(bg+"XMIDI.MT"):intro?introBank:classicBank):menu?scoreEntry(scoreBytes(si+"MAINMENU.TIM"),0):scoreBytes(si+"XMIDI.MT");
                IBufferDataView source(bank.data(),bank.size());d.loadTimbreLibrary(&source,((isBg&&!ending)||menu)?MidiDriver::TIMBRE_LIBRARY_U7VOICE_MT:MidiDriver::TIMBRE_LIBRARY_XMIDI_MT);
                require(d.music_ready,"original bank initialized");
                std::vector<sint16> pcm(640);unsigned cycles=0;
                while(d.isRolandWaiting() && cycles++<1000)d.lowLevelProduceSamples(pcm.data(),320);
                require(!d.isRolandWaiting(),"bounded original initialization waits");
                require(d.music_frames<640,"hardware waits freeze musical clock");
                // Exercise the same bank + prepare + sequencer entry points as game/Music Player routing.
                int sequence=0;
                auto realScore=isBg?(ending?scoreBytes(bg+"ENDSCORE.XMI"):intro?scoreEntry(scoreBytes(bg+"INTRORDM.DAT"),2):menu?scoreEntry(scoreBytes(bg+"INTRORDM.DAT"),3):scoreEntry(scoreBytes(bg+"MT32MUS.DAT"),32)):
                    menu?scoreEntry(scoreBytes(si+"MAINSHP.FLX"),28):ending?scoreBytes(si+"R_SEND.XMI"):context==6?scoreBytes("data/si/ext_intro_r.xmi"):intro?scoreBytes(si+"R_SINTRO.XMI"):scoreEntry(scoreBytes(si+"MT32MUS.DAT"),13);
                if(isBg&&ending)sequence=1;
                IBufferDataView scoreInput(realScore.data(),realScore.size());d.prepareMusicTrack(&scoreInput,sequence,context==4?13:-1);
                XMidiFile musicFile(&scoreInput,XMIDIFILE_CONVERT_NOCONVERSION,Driver::identifier(model),true,true);
                d.startSequence(0,musicFile.GetEventList(sequence),false,255);
                bool scoreAudible=false;
                for(int i=0;i<600;++i){std::fill(pcm.begin(),pcm.end(),0);d.produceSamples(pcm.data(),unsigned(pcm.size()*sizeof(sint16)));scoreAudible|=std::any_of(pcm.begin(),pcm.end(),[](auto v){return v!=0;});}
                require(scoreAudible,"actual original score reaches Munt through sequencer");
                if(context==4){
                    uint8 timbre=255;d.mt32->readMemory(0xc090+(80-24)*4,1,&timbre);
                    require(timbre==0,"SI record70 assigns its first custom timbre to rhythm key80 in actual Munt memory");
                }
                d.finishSequence(0);for(int i=0;i<240;++i)d.yield();
                require(!d.isSequencePlaying(0),"score stop/skip acknowledges");
                for(int i=0;i<1200;++i)d.lowLevelProduceSamples(pcm.data(),320);
                require(!d.mt32->isActive(),"score stop releases original instruments");
                d.send(0xc1);d.send(0xb1|(7<<8)|(100<<16));d.send(0x91|(60<<8)|(100<<16));
                bool audible=false;for(int i=0;i<30;++i){d.lowLevelProduceSamples(pcm.data(),320);audible|=std::any_of(pcm.begin(),pcm.end(),[](auto v){return v!=0;});}
                require(audible,"original profile renders actual Munt PCM");
                d.send(0x81|(60<<8));for(int i=0;i<1200;++i)d.lowLevelProduceSamples(pcm.data(),320);
                require(!d.mt32->isActive(),"native profile clean note termination");
                static_cast<MidiDriver&>(d).setGlobalVolume(0);for(int i=0;i<240 && d.getGlobalVolume()!=0;++i)d.yield();
                d.send(0x91|(62<<8)|(100<<16));for(int i=0;i<30;++i){d.lowLevelProduceSamples(pcm.data(),320);require(std::all_of(pcm.begin(),pcm.end(),[](auto v){return v==0;}),"native PCM master mute");}
                d.send(0x81|(62<<8));static_cast<MidiDriver&>(d).setGlobalVolume(100);for(int i=0;i<240 && d.getGlobalVolume()!=100;++i)d.yield();
            }
            d.destroyMidiDriver();require(!d.mt32&&!d.music,"native shutdown releases synthesis and policy");
            std::cout<<"PASS original Roland ROM-backed contexts/model "<<Driver::identifier(model)<<"\n";
        }
        if(hadStatic)add_system_path("<STATIC>",oldStatic);else clear_system_path("<STATIC>");
    }
};
