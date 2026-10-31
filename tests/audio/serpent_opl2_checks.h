// Independent execution of the local AIL source protects SI's driver policy.
extern "C" void serpentRefReset(bool,const uint8_t*);
extern "C" void serpentRefInstall(unsigned,unsigned,const uint8_t*);
extern "C" void serpentRefEvent(uint32_t);
extern "C" void serpentRefTick();
extern "C" const uint8_t* serpentRefRegisters();
extern "C" void serpentRefClearTrace();
extern "C" unsigned serpentRefTraceCount();
extern "C" uint32_t serpentRefTraceAt(unsigned);
extern "C" unsigned serpentRefSlot(unsigned,unsigned);

class SerpentOpl2Regression {
    static std::vector<uint8> bank_data;
    static unsigned comparisons;
    static void compare(OriginalOpl2MidiDriver& d,const char* operation,bool ordered=true) {
        auto reference=serpentRefRegisters();
        for(unsigned r=0;r<256;++r) if(d.registers[r]!=reference[r]) {
            std::cerr<<operation<<" register="<<std::hex<<r<<" Exult="<<unsigned(d.registers[r])<<" AIL="<<unsigned(reference[r])<<std::dec<<'\n';
            for(unsigned i=0;i<16;++i) if(d.slots[i].status || serpentRefSlot(i,0)) {
                auto& s=d.slots[i]; std::cerr<<"slot="<<i<<" status="<<unsigned(s.status)<<'/'<<serpentRefSlot(i,0)<<" voice="<<s.voice<<'/'<<serpentRefSlot(i,1)<<" channel="<<unsigned(s.channel)<<'/'<<serpentRefSlot(i,2)<<" key="<<unsigned(s.key)<<'/'<<serpentRefSlot(i,3)<<" patch="<<s.patch<<'/'<<serpentRefSlot(i,9)<<'\n';
            }
            throw std::runtime_error("SI reference register mismatch");
        }
        for(unsigned i=0;i<16;++i) {
            const auto& s=d.slots[i];
            require(s.status==serpentRefSlot(i,0),"SI original virtual-slot status");
            if(!s.status) continue;
            require(unsigned(s.voice<0?255:s.voice)==serpentRefSlot(i,1),"SI original physical voice ownership");
            require(s.channel==serpentRefSlot(i,2) && s.key==serpentRefSlot(i,3),"SI original channel/key ownership");
            require(s.type==serpentRefSlot(i,4) && s.duration==serpentRefSlot(i,5),"SI original instrument type/release deadline");
            require(s.note==serpentRefSlot(i,6) && s.transpose==serpentRefSlot(i,7),"SI original transpose/percussion pitch");
        }
        if(ordered) {
            if(authentic_opl2_writes.size()!=serpentRefTraceCount()) {
                std::cerr<<operation<<" writes Exult="<<authentic_opl2_writes.size()<<" AIL="<<serpentRefTraceCount()<<'\n';
                throw std::runtime_error("SI register-write count mismatch");
            }
            for(unsigned i=0;i<authentic_opl2_writes.size();++i)
                require(authentic_opl2_writes[i]==serpentRefTraceAt(i),"SI original register-write ordering");
        }
        ++comparisons;
    }
    static void clear() { authentic_opl2_writes.clear(); serpentRefClearTrace(); }
    static std::unique_ptr<OriginalOpl2MidiDriver> driver(bool cinematic=false) {
        auto d=std::make_unique<OriginalOpl2MidiDriver>(); d->setSfxSynchronous(true);
        require(d->initMidiDriver(48000,true)==0,"SI original OPL2 initialization");
        d->setSerpentContext(false,cinematic);
        IBufferDataView bank(bank_data.data(),bank_data.size()); d->loadTimbreLibrary(&bank,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
        require(d->ready && d->ending && !d->legacy,"SI native AIL replaces legacy fallback");
        serpentRefReset(cinematic,d->serpent_driver.data()); compare(*d,"initialization",false);
        clear(); return d;
    }
    static void install(OriginalOpl2MidiDriver& d,unsigned patch) {
        auto found=d.ail_patches.find(patch); require(found!=d.ail_patches.end(),"original SI timbre exists");
        serpentRefInstall(patch>>8,patch&255,found->second.data());
    }
    static void event(OriginalOpl2MidiDriver& d,unsigned type,unsigned ch,unsigned a,unsigned b=0) {
        clear(); uint32 message=type|ch|(a<<8)|(b<<16); d.send(message);
        if(type==0xb0 && a==7) message=(message&65535)|(std::min(127u,b*d.serpent_relative/100)<<16);
        serpentRefEvent(message);
        try { compare(d,"MIDI event"); }
        catch(...) { std::cerr<<"event="<<std::hex<<message<<std::dec<<'\n'; throw; }
    }
    static void tick(OriginalOpl2MidiDriver& d) {
        clear(); d.endingTick(); serpentRefTick(); compare(d,"60Hz TVFX/5Hz priorities");
    }
    static void select(OriginalOpl2MidiDriver& d,unsigned id,unsigned ch=1) {
        install(d,id); event(d,0xb0,ch,114,id>>8); event(d,0xc0,ch,id&255);
    }
    static unsigned be32(const std::vector<uint8>& bytes,unsigned at) {
        require(at+4<=bytes.size(),"XMI metadata bounds");
        return (unsigned(bytes[at])<<24)|(unsigned(bytes[at+1])<<16)|(unsigned(bytes[at+2])<<8)|bytes[at+3];
    }
    static void timbres(const std::vector<uint8>& bytes,unsigned begin,unsigned end,std::set<unsigned>& result) {
        for(unsigned at=begin;at+8<=end;) {
            unsigned size=be32(bytes,at+4); require(size<=end-at-8,"XMI chunk length");
            std::string tag(reinterpret_cast<const char*>(bytes.data()+at),4);
            if(tag=="TIMB") {
                require(size>=2,"XMI timbre count"); unsigned count=bytes[at+8]|(unsigned(bytes[at+9])<<8);
                require(2+count*2<=size,"XMI timbre entries");
                for(unsigned i=0;i<count;++i) result.insert(bytes[at+10+i*2]|(unsigned(bytes[at+11+i*2])<<8));
            } else if((tag=="FORM" || tag=="CAT " || tag=="LIST") && size>=4) timbres(bytes,at+12,at+8+size,result);
            at+=8+size+(size&1);
        }
    }
    static void score(const std::vector<uint8>& bytes,bool cinematic,int track,const char* label) {
        auto d=driver(cinematic); std::set<unsigned> required; timbres(bytes,0,unsigned(bytes.size()),required);
        unsigned cache=0; for(auto id:required) { install(*d,id); cache+=unsigned(d->ail_patches.at(id).size()); }
        require(cache<=3584,"stock music fits original AIL timbre cache");
        d->setSerpentTrack(track);
        for(unsigned c=0;c<16;++c) serpentRefEvent(0xb0|c|(7<<8)|(std::min(127u,127*d->serpent_relative/100)<<16));
        compare(*d,"track relative volume",false);
        IBufferDataView input(bytes.data(),bytes.size()); XMidiFile file(&input,XMIDIFILE_CONVERT_NOCONVERSION,"OriginalOPL2",true,true);
        auto* list=file.GetEventList(0); require(list,"SI source score");
        ClockHandler h(true); unsigned ticks=0,notes=0;
        h.observe=[&](uint32 m,uint64) {
            while(ticks<h.now/100) { ++ticks; tick(*d); }
            unsigned type=m&240,ch=m&15,a=(m>>8)&127,b=(m>>16)&127;
            if(type==144 && b) ++notes;
            event(*d,type,ch,a,b);
        };
        list->incrementCounter(); { XMidiSequence playback(&h,0,list,false,255,-1); pump(playback,h,list->getLength()*50+30000); playback.finish(); }
        require(notes>0,"SI score dispatches music");
        std::cout<<"PASS SI Original OPL2 "<<label<<" notes="<<notes<<" executed AIL register/slot/write-order trace\n";
    }
public:
    static void run() {
        const bool had=is_system_path_defined("<STATIC>"); auto old=get_system_path("<STATIC>");
        const char* overrideDir=std::getenv("EXULT_OPL3_SI_STATIC");
        add_system_path("<STATIC>",overrideDir?overrideDir:"C:/GOG Games/Ultima 7 - Serpent Isle/STATIC");
        bank_data=scoreBytes(get_system_path("<STATIC>/XMIDI.AD"));
        Configuration cfg; auto previous=config; config=&cfg;
        {
            auto d=driver(); select(*d,15);
            unsigned frequencies=0;
            for(unsigned key=0;key<128;++key) {
                event(*d,0x90,1,key,127);
                for(unsigned wheel=0;wheel<16384;wheel+=32) { event(*d,0xe0,1,wheel&127,wheel>>7); ++frequencies; }
                event(*d,0xe0,1,127,127); ++frequencies; event(*d,0x80,1,key);
            }
            std::cout<<"PASS SI Original OPL2 frequency/bend executed-reference comparisons="<<frequencies<<'\n';
        }
        {
            auto d=driver(); std::vector<unsigned> instruments;
            for(const auto& [id,bytes]:d->ail_patches) if(bytes.size()==14) instruments.push_back(id);
            // A fresh oracle cache for each instrument avoids irrelevant DOS-cache pressure.
            unsigned levels=0;
            for(auto id:instruments) {
                d=driver(); unsigned ch=(id>>8)==127?9:1,key=ch==9?(id&255):60; select(*d,id,ch);
                for(unsigned velocity:{1u,32u,64u,100u,127u}) for(unsigned volume:{0u,64u,127u}) for(unsigned expression:{0u,127u}) {
                    event(*d,0xb0,ch,7,volume); event(*d,0xb0,ch,11,expression); event(*d,0x90,ch,key,velocity);
                    event(*d,0xb0,ch,1,127); event(*d,0xb0,ch,1,0); event(*d,0x80,ch,key); ++levels;
                }
            }
            std::cout<<"PASS SI Original OPL2 instruments="<<instruments.size()<<" velocity/volume/expression cases="<<levels<<"; waveforms/envelopes/percussion/transpose/modulation\n";
        }
        {
            auto d=driver(); select(*d,15);
            event(*d,0x90,1,60,100); event(*d,0x90,1,60,90); event(*d,0xb0,1,64,64); event(*d,0x80,1,60);
            require(std::count_if(d->slots.begin(),d->slots.end(),[](auto& s){return s.status;})==2,"SI overlapping notes held by original sustain");
            event(*d,0xb0,1,121); require(std::none_of(d->slots.begin(),d->slots.end(),[](auto& s){return s.status;}),"SI reset releases both occurrences as original AIL does");
            for(unsigned key=40;key<59;++key) event(*d,0x90,1,key,100);
            require(std::count_if(d->owners.begin(),d->owners.end(),[](int s){return s>=0;})==9,"SI physical limit nine");
            event(*d,0xb0,1,112,127); tick(*d); event(*d,0xb0,1,123);
            std::cout<<"PASS SI Original OPL2 round-robin/priority/stealing/overlap/sustain/reset/channel-stop against AIL\n";
        }
        // These are the four extended timbres required by installed music scores.
        for(unsigned id:{(66u<<8)|25,(63u<<8)|30,(63u<<8)|31,(63u<<8)|32}) {
            auto d=driver(); select(*d,id);
            event(*d,0x90,1,60,100); for(unsigned i=0;i<90;++i) tick(*d);
            event(*d,0x80,1,60); for(unsigned i=0;i<300;++i) tick(*d);
        }
        {
            auto d=driver(); select(*d,(66u<<8)|25);
            for(unsigned key=40;key<57;++key) event(*d,0x90,1,key,100);
            require(std::count_if(d->slots.begin(),d->slots.end(),[](auto& s){return s.status;})==16,"SI sixteen virtual slots");
            require(std::count_if(d->owners.begin(),d->owners.end(),[](int s){return s>=0;})<=9,"SI TVFX nine hardware channels");
            for(unsigned i=0;i<30;++i) tick(*d); event(*d,0xb0,1,123); for(unsigned i=0;i<300;++i) tick(*d);
            std::cout<<"PASS SI Original OPL2 four music TVFX timbres; sixteen-slot/nine-voice contention and deferred releases against AIL\n";
        }
        auto archive=scoreBytes(get_system_path("<STATIC>/ADLIBMUS.DAT"));
        score(scoreEntry(archive,13),false,13,"gameplay track13");
        score(scoreBytes(get_system_path("<STATIC>/A_SINTRO.XMI")),true,-1,"original introduction");
        score(scoreBytes("data/si/ext_intro_a.xmi"),true,-1,"extended introduction");
        score(scoreBytes(get_system_path("<STATIC>/A_SEND.XMI")),true,-1,"ending");
        {
            auto bytes=scoreEntry(archive,13);
            cfg.set("config/audio/midi/volume_curve","1",false);
            IBufferDataView first(bytes.data(),bytes.size()); XMidiFile original(&first,XMIDIFILE_CONVERT_NOCONVERSION,"OriginalOPL2",true,true);
            auto unmodified=scoreNotes(original.GetEventList(0));
            cfg.set("config/audio/midi/volume_curve","2",false);
            IBufferDataView second(bytes.data(),bytes.size()),third(bytes.data(),bytes.size());
            XMidiFile authentic(&second,XMIDIFILE_CONVERT_NOCONVERSION,"OriginalOPL2",true,true);
            XMidiFile legacy(&third,XMIDIFILE_CONVERT_NOCONVERSION,"OPL3Stereo",true);
            require(scoreNotes(authentic.GetEventList(0))==unmodified,"SI original score velocities bypass configurable MIDI curve");
            require(scoreNotes(legacy.GetEventList(0))!=unmodified,"legacy MIDI volume curve behavior remains active");
            cfg.set("config/audio/midi/volume_curve","1",false);
            std::cout<<"PASS SI native dynamics opt-in preserves source events; legacy MIDI volume-curve semantics unchanged\n";
        }
        {
            auto d=driver(); d->setSerpentContext(true,false);
            auto bank=scoreEntry(scoreBytes(get_system_path("<STATIC>/MAINMENU.TIM")),1);
            IBufferDataView data(bank.data(),bank.size()); d->loadTimbreLibrary(&data,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
            require(d->ready && !d->ending && !d->legacy,"SI native classic menu context");
            d->send(0xc1); d->send(0x91|(60<<8)|(100<<16));
            std::vector<sint16> pcm(3200); d->lowLevelProduceSamples(pcm.data(),1600);
            require(std::any_of(pcm.begin(),pcm.end(),[](auto s){return s!=0;}),"SI menu original bank produces audio");
            d->send(0xb1|(123<<8)); require(d->busy.empty(),"SI menu stops cleanly");
            // Exercise the actual FM menu score against the classic-driver
            // oracle: MAINMENU.DRV is identical to the validated BG driver.
            d->destroyMidiDriver();
            d=std::make_unique<OriginalOpl2MidiDriver>(); d->setSfxSynchronous(true);
            require(d->initMidiDriver(48000,true)==0,"fresh SI menu chip"); d->setSerpentContext(true,false);
            d->loadTimbreLibrary(&data,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
            originalOpl2Reset(bank.data());
            for(unsigned c=9;c<30;++c) originalOpl2Event(c,0xc0,d->bank[d->programs[c]][47],0);
            auto menu=scoreEntry(scoreBytes(get_system_path("<STATIC>/MAINSHP.FLX")),27);
            IBufferDataView music(menu.data(),menu.size()); XMidiFile file(&music,XMIDIFILE_CONVERT_NOCONVERSION,"OriginalOPL2",true,true);
            auto list=file.GetEventList(0); require(list,"SI native FM menu score");
            auto compareMenu=[&]() {
                const auto* expected=originalOpl2Registers();
                for(unsigned r=0;r<256;++r) {
                    if(((r>=0xa0 && r<=0xa8)||(r>=0xb0 && r<=0xb8)) && d->voices[r&15].channel<0) continue;
                    require(d->registers[r]==expected[r],"SI menu original-driver registers");
                }
            };
            ClockHandler h(true); unsigned ticks=0,notes=0;
            h.observe=[&](uint32 m,uint64) {
                while(ticks<h.now/100) { ++ticks; d->tick(); originalOpl2Tick(); compareMenu(); }
                unsigned type=m&240,ch=m&15,key=(m>>8)&127,value=(m>>16)&127;
                d->send(m);
                if(type==0x90 || type==0x80) {
                    notes+=(type==0x90 && value);
                    if(ch!=9) originalOpl2Event(ch,type,key,value);
                    else if(key<88 && d->percussion_channel[key]) originalOpl2Event(d->percussion_channel[key]-1,type,d->percussion_key[key],value);
                } else if(type!=0xc0 || ch!=9) originalOpl2Event(ch,type,key,value);
                compareMenu();
            };
            list->incrementCounter(); { XMidiSequence playback(&h,0,list,false,255,-1); pump(playback,h,list->getLength()*50+30000); playback.finish(); }
            require(notes,"SI FM menu has notes");
            std::cout<<"PASS SI native menu score notes="<<notes<<" executed classic-driver reference registers\n";
            require(!d->hasSerpentContext(false,false),"menu/game context change invalidates timbre cache");
            d->setSerpentContext(false,true);
            IBufferDataView cinematic(bank_data.data(),bank_data.size()); d->loadTimbreLibrary(&cinematic,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
            require(d->ready && d->ending && !d->si_new_ail,"menu to native cinematic policy");
            require(!d->hasSerpentContext(false,false),"cinematic/game AIL version change invalidates timbre cache");
            d->setSerpentContext(false,false);
            IBufferDataView gameplay(bank_data.data(),bank_data.size()); d->loadTimbreLibrary(&gameplay,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
            require(d->ready && d->ending && d->si_new_ail,"cinematic to native gameplay policy");
            d->loadTimbreLibrary(nullptr,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
            require(!d->ready,"missing bank rejected without fallback");
            d->destroyMidiDriver(); require(!d->chip && !d->legacy,"SI shutdown releases synthesis state");
        }
        {
            auto a=driver(),b=driver(); install(*a,15); a->send(0xc1|(15<<8)); b->send(0xc1|(15<<8));
            a->send(0x91|(60<<8)|(100<<16)); b->send(0x91|(60<<8)|(100<<16));
            std::vector<sint16> whole(9600),pieces(9600); a->lowLevelProduceSamples(whole.data(),4800);
            for(unsigned at=0;at<4800;) { unsigned n=std::min(13u,4800-at); b->lowLevelProduceSamples(pieces.data()+at*2,n); at+=n; }
            require(whole==pieces,"SI render partition continuity");
            require(std::any_of(whole.begin(),whole.end(),[](auto v){return v!=0;}),"SI native PCM output");
            for(unsigned i=0;i<4800;++i) require(whole[i*2]==whole[i*2+1],"SI mono output");
            static_cast<MidiDriver&>(*a).setGlobalVolume(0); a->yield(); a->lowLevelProduceSamples(whole.data(),4800);
            require(std::all_of(whole.begin(),whole.end(),[](auto v){return !v;}),"SI master mute");
        }
        std::cout<<"PASS SI Original OPL2 source comparisons="<<comparisons<<"; menu/context switching/missing-resource safety/shutdown/mono/mute/continuity\n";
        config=previous; if(had) add_system_path("<STATIC>",old); else clear_system_path("<STATIC>");
    }
};
std::vector<uint8> SerpentOpl2Regression::bank_data;
unsigned SerpentOpl2Regression::comparisons=0;
