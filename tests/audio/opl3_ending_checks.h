// BG ending-only TVFX regression: original assets stay external, and expected
// behavior comes from the independently executed DOS AIL source oracle above.
class Opl3EndingRegression {
    static inline unsigned checks=0;
    static inline std::array<uint8,256> reference_before{};
    static std::unique_ptr<Opl3StereoMidiDriver> driver(std::vector<uint8>& bank,std::vector<uint8>& score,unsigned sequence=3) {
        auto d=std::make_unique<Opl3StereoMidiDriver>();d->setSfxSynchronous(true);
        require(d->initMidiDriver(48000,true)==0,"ending OPL3 initialization");
        IBufferDataView music(score.data(),score.size());
        require(d->setEndingContext(true,&music,sequence),"validated ending score context");
        IBufferDataView patches(bank.data(),bank.size());d->loadTimbreLibrary(&patches,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
        require(d->ending_bank && d->si_bank,"validated original ending bank and pitch table");return d;
    }
    static std::vector<uint8> definition(const std::vector<uint8>& bank,unsigned program) {
        for(unsigned at=0;at+6<=bank.size();at+=6) {
            if(bank[at]==255 || bank[at+1]==255)break;
            if(bank[at]!=program || bank[at+1]!=2)continue;
            unsigned off=bank[at+2]|(unsigned(bank[at+3])<<8)|(unsigned(bank[at+4])<<16)|(unsigned(bank[at+5])<<24);
            unsigned n=bank[off]|(unsigned(bank[off+1])<<8);return {bank.begin()+off,bank.begin()+off+n};
        }
        throw std::runtime_error("missing original TVFX definition");
    }
    static std::vector<unsigned> registers(unsigned voice) {
        unsigned m=Opl3StereoMidiDriver::adlib_opadd[voice],r=Opl3StereoMidiDriver::channelRegister(voice);
        return {0x20+m,0x23+m,0x40+m,0x43+m,0x60+m,0x63+m,0x80+m,0x83+m,0xe0+m,0xe3+m,0xc0+r,0xa0+r,0xb0+r};
    }
    static void clearTrace(){std::copy_n(serpentRefRegisters(),256,reference_before.begin());ending_opl3_writes.clear();serpentRefClearTrace();}
    static void compare(Opl3StereoMidiDriver& d,unsigned voice=0,bool ordered=true) {
        auto& s=d.ending_voices[voice];auto ref=serpentRefRegisters();
        auto expected=registers(serpentRefSlot(0,1)==255?0:serpentRefSlot(0,1));auto actual=registers(voice);
        for(unsigned i=0;i<actual.size();++i)if(d.adlib_data[actual[i]]!=ref[expected[i]]) {
            std::cerr<<"TVFX program="<<s.program<<" register="<<std::hex<<expected[i]<<" actual="<<unsigned(d.adlib_data[actual[i]])<<" expected="<<unsigned(ref[expected[i]])<<std::dec<<'\n';
            throw std::runtime_error("TVFX original register mismatch");
        }
        require(s.status==serpentRefSlot(0,0),"original TVFX status/release termination");
        if(s.status) {
            require(s.duration==serpentRefSlot(0,5),"original TVFX independent duration");
            require(s.type==serpentRefSlot(0,4),"original TVFX instrument type");
        }
        // Chip initialization differs; compare effective parameter writes from the
        // same reference shadow, excluding redundant writes of unchanged values.
        std::vector<unsigned> writes,reference;auto shadow=reference_before,reference_shadow=reference_before;
        for(auto w:ending_opl3_writes) {
            auto at=std::find(actual.begin(),actual.end(),w&65535);
            if(at!=actual.end()) {
                unsigned index=unsigned(at-actual.begin()),value=w>>16;
                if(shadow[expected[index]]!=value)writes.push_back(index|(value<<8));
                shadow[expected[index]]=uint8(value);
            }
        }
        for(unsigned i=0;i<serpentRefTraceCount();++i) {
            auto w=serpentRefTraceAt(i);auto at=std::find(expected.begin(),expected.end(),w&255);
            if(at!=expected.end()) {
                unsigned reg=w&255,value=w>>8;
                if(reference_shadow[reg]!=value)reference.push_back(unsigned(at-expected.begin())|(value<<8));
                reference_shadow[reg]=uint8(value);
            }
        }
        if(ordered && writes!=reference) {
            std::cerr<<"TVFX program="<<s.program<<" status="<<unsigned(s.status)<<" duration="<<s.duration<<" ordering actual:";
            for(auto w:writes)std::cerr<<' '<<std::hex<<w;
            std::cerr<<" expected:";for(auto w:reference)std::cerr<<' '<<w;std::cerr<<std::dec<<'\n';
            throw std::runtime_error("TVFX original register update ordering");
        }++checks;
    }
    static int owner(Opl3StereoMidiDriver& d,uint64 identity) {
        for(unsigned i=0;i<d.VoiceCount;++i)if(d.voice_identity[i]==identity)return int(i);return -1;
    }
    static void select(Opl3StereoMidiDriver& d,unsigned program,unsigned channel=1) {
        d.send(0xb0|channel|(114<<8)|(2<<16));d.send(0xc0|channel|(program<<8));
    }
public:
    static void run() {
        auto had=is_system_path_defined("<STATIC>");auto old=get_system_path("<STATIC>");
        add_system_path("<STATIC>","C:/GOG Games/Ultima 7/STATIC");
        auto bank=scoreBytes(get_system_path("<STATIC>/XMIDI.AD")),score=scoreBytes(get_system_path("<STATIC>/ENDSCORE.XMI"));
        auto adv=scoreBytes(get_system_path("<STATIC>/ADLIB.ADV"));
        for(unsigned program:{53u,54u,58u,60u,61u,62u,63u}) {
            auto d=driver(bank,score);auto bytes=definition(bank,program);
            require(d->ending_patches[program]==bytes,"correct original TVFX record");
            serpentRefReset(true,adv.data());serpentRefInstall(2,program,bytes.data());
            select(*d,program);serpentRefEvent(0xb1|(114<<8)|(2<<16));serpentRefEvent(0xc1|(program<<8));
            clearTrace();d->sendNote(0x91|(60<<8)|(100<<16),100);serpentRefEvent(0x91|(60<<8)|(100<<16));compare(*d,0,false);
            for(unsigned tick=0;tick<1080;++tick) {
                if(tick==180) {clearTrace();d->sendNote(0x91|(60<<8),100);serpentRefEvent(0x81|(60<<8));compare(*d);}
                // Exercise pitch/dynamics/automation interaction with original controllers.
                if(tick==20 || tick==40 || tick==60 || tick==80) {
                    unsigned message=tick==20?(0xb1|(7<<8)|(66<<16)):tick==40?(0xb1|(11<<8)|(90<<16)):tick==60?(0xe1|(100<<16)):(0xb1|(1<<8)|(100<<16));
                    clearTrace();d->send(message);serpentRefEvent(message);compare(*d);
                }
                clearTrace();d->tickEndingVoices();serpentRefTick();compare(*d);
            }
            require(owner(*d,100)<0,"TVFX release completes without stale ownership");
            std::cout<<"PASS BG ending TVFX bank2 program="<<program<<" original AIL registers/write-order/60Hz/duration/release\n";
        }
        {
            auto d=driver(bank,score);d->send(0xc0|(50<<8));
            for(unsigned i=0;i<9;++i)d->sendNote(0x90|((40+i)<<8)|(100<<16),1000+i);
            auto bytes=definition(bank,61);serpentRefReset(true,adv.data());serpentRefInstall(2,61,bytes.data());
            select(*d,61);serpentRefEvent(0xb1|(114<<8)|(2<<16));serpentRefEvent(0xc1|(61<<8));
            clearTrace();d->sendNote(0x91|(48<<8)|(100<<16),1100);serpentRefEvent(0x91|(48<<8)|(100<<16));
            require(owner(*d,1100)==9,"TVFX uses normal high-bank allocation");compare(*d,9,false);
            for(unsigned tick=0;tick<300;++tick) {
                if(tick==120){clearTrace();d->sendNote(0x91|(48<<8),1100);serpentRefEvent(0x81|(48<<8));compare(*d,9);}
                clearTrace();d->tickEndingVoices();serpentRefTick();compare(*d,9);
            }
        }
        {
            auto d=driver(bank,score);select(*d,60);d->sendNote(0x91|(60<<8)|(100<<16),200);d->sendNote(0x91|(60<<8)|(100<<16),201);
            int first=owner(*d,200),second=owner(*d,201);require(first>=0&&second>=0&&first!=second,"overlapping TVFX occurrences");
            d->sendNote(0x91|(60<<8),201);d->tickEndingVoices();
            require(d->ending_voices[first].status==1&&d->ending_voices[second].status==2,"release targets exact original trigger identity");
            // Fill both register banks, then steal the oldest TVFX voice normally.
            d->send(0xc0|(50<<8));for(unsigned i=0;i<17;++i)d->sendNote(0x90|((40+i)<<8)|(100<<16),300+i);
            require(owner(*d,200)<0&&owner(*d,316)==first,"unchanged oldest-active stealing policy");
            auto before=std::vector<uint8>(std::begin(d->adlib_data),std::end(d->adlib_data));
            d->sendNote(0x91|(60<<8),200);for(unsigned i=0;i<120;++i)d->tickEndingVoices();
            for(auto r:registers(first))require(before[r]==d->adlib_data[r],"stale TVFX timer/off cannot change replacement");
            // Existing stop endpoint must also clear a TVFX tail after the sequencer has ended.
            select(*d,53);d->sendNote(0x91|(55<<8)|(100<<16),500);d->finishSequence(0);
            require(std::none_of(d->ending_voices.begin(),d->ending_voices.end(),[](auto& s){return s.status;}),"stop/skip clears TVFX tails");
            d->setEndingContext(false);require(!d->ending_bank,"ending state cannot leak to next score");
            d->destroyMidiDriver();require(!d->opl,"ending destruction clears chip");
            require(d->initMidiDriver(48000,true)==0,"ending reset/reopen");require(!d->ending_context,"reset restores ordinary context");
        }
        {
            auto a=driver(bank,score),b=driver(bank,score);select(*a,61);select(*b,61);
            a->sendNote(0x91|(48<<8)|(100<<16),600);b->sendNote(0x91|(48<<8)|(100<<16),600);
            std::vector<sint16> whole(9600),pieces(9600);a->lowLevelProduceSamples(whole.data(),4800);
            for(unsigned at=0;at<4800;){unsigned n=std::min(13u,4800-at);b->lowLevelProduceSamples(pieces.data()+at*2,n);at+=n;}
            require(whole==pieces,"TVFX 60Hz cadence independent of render partitions");
            require(std::any_of(whole.begin(),whole.end(),[](auto x){return x!=0;}),"TVFX produces PCM");
            static_cast<MidiDriver&>(*a).setGlobalVolume(0);a->yield();a->lowLevelProduceSamples(whole.data(),4800);
            require(std::all_of(whole.begin(),whole.end(),[](auto x){return x==0;}),"TVFX master mute");
        }
        {
            auto d=driver(bank,score);auto changed=score;changed.back()^=1;IBufferDataView bad(changed.data(),changed.size());
            require(d->setEndingContext(true,&bad,3)&&!d->ending_context,"unrecognized score safely rejects TVFX activation");
            IBufferDataView good(score.data(),score.size());require(!d->setEndingContext(true,&good,1),"Roland sequence cannot activate ending TVFX");
            d->setEndingContext(true,&good,3);auto badBank=bank;badBank.back()^=1;IBufferDataView input(badBank.data(),badBank.size());
            d->loadTimbreLibrary(&input,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);require(!d->ending_bank,"unrecognized bank does not activate TVFX");
        }
        {
            auto a=driver(bank,score);auto b=std::make_unique<Opl3StereoMidiDriver>();b->setSfxSynchronous(true);
            require(b->initMidiDriver(48000,true)==0,"ordinary ending baseline");
            IBufferDataView input(bank.data(),bank.size());b->loadTimbreLibrary(&input,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
            for(unsigned bankId=0;bankId<128;++bankId)if(a->xmidibanks[bankId])for(unsigned program=0;program<128;++program) {
                const auto& ins=a->xmidibanks[bankId]->insbank[program];
                if(!(ins.perc_voice&128) || ins.tvfx_program>=0)continue;
                require(ins==b->xmidibanks[bankId]->insbank[program],"ordinary ending instrument decoding unchanged");
            }
            for(unsigned message:{0xb1u|(114<<8),0xc1u|(50<<8),0xb1u|(10<<8)|(40<<16),0x91u|(60<<8)|(100<<16),0xb1u|(7<<8)|(66<<16),0xe1u|(100<<16),0x91u|(60<<8)}) {
                a->sendNote(message,900);b->sendNote(message,900);
                require(std::equal(std::begin(a->adlib_data),std::end(a->adlib_data),std::begin(b->adlib_data)),"ordinary ending notes/controllers/pan unchanged");
            }
        }
        Configuration memory;auto previous=config;config=&memory;
        for(unsigned sequence:{3u,4u}) {
            auto d=driver(bank,score,sequence);IBufferDataView input(score.data(),score.size());XMidiFile file(&input,XMIDIFILE_CONVERT_NOCONVERSION,"OPL3Stereo");
            auto list=file.GetEventList(sequence);require(list,"actual ending FM sequence");
            ClockHandler h(true);unsigned ticks=0,triggers=0,total=0;std::set<int> programs;
            h.observe=[&](uint32 m,uint64 id){
                while(ticks<h.now/100){++ticks;d->tickEndingVoices();}
                auto type=m&240,c=m&15;
                int program=d->ch[c].ins.tvfx_program;
                d->sendNote(m,id);
                if(type==0x90 && (m>>16)) {
                    ++total;if(program>=0){++triggers;programs.insert(program);int v=owner(*d,id);
                    require(v>=0&&d->ending_voices[v].program==program&&d->ending_voices[v].identity==id,"actual ending trigger uses correct TVFX/identity");}
                }
            };
            list->incrementCounter();{XMidiSequence playback(&h,0,list,false,255,-1);pump(playback,h,list->getLength()*50+30000);playback.finish();}
            if(triggers!=(sequence==3?13:3)) std::cerr<<"Ending sequence="<<sequence<<" TVFX triggers="<<triggers<<" total notes="<<total<<'\n';
            require(triggers==(sequence==3?13:3),"all affected ending note-ons interpreted as TVFX");
            require(total==(sequence==3?366:180),"unaffected score note count preserved");
            require(programs==(sequence==3?std::set<int>{53,54,58,60,61,62}:std::set<int>{63}),"actual ending uses seven expected definitions");
            require(std::none_of(d->ending_voices.begin(),d->ending_voices.end(),[](auto& s){return s.status;}),"sequence finish leaves no automation");
            std::cout<<"PASS OPL3 actual BG ending sequence="<<sequence<<" total notes="<<total<<" TVFX="<<triggers<<" correct programs/identities/finish\n";
        }
        config=previous;
        if(had)add_system_path("<STATIC>",old);else clear_system_path("<STATIC>");
        std::cout<<"PASS ending TVFX comparisons="<<checks<<" overlap/steal/stale-off/stop/reset/PCM partition/mute/context guards\n";
    }
};
