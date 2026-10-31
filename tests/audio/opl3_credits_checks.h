// Test-only member access by explicit instantiation preserves MSVC's private
// symbol names and avoids production accessors or a running game/SDL device.
template<class Tag,typename Tag::type Member> struct CreditsTestAccess {
    friend typename Tag::type creditsMember(Tag){return Member;}
};
struct CreditsGame {using type=Exult_Game*;friend type creditsMember(CreditsGame);};
template struct CreditsTestAccess<CreditsGame,&Game::game_type>;
struct CreditsDriver {using type=std::shared_ptr<MidiDriver> MyMidiPlayer::*;friend type creditsMember(CreditsDriver);};
template struct CreditsTestAccess<CreditsDriver,&MyMidiPlayer::midi_driver>;
struct CreditsInitialized {using type=bool MyMidiPlayer::*;friend type creditsMember(CreditsInitialized);};
template struct CreditsTestAccess<CreditsInitialized,&MyMidiPlayer::initialized>;
struct CreditsFilename {using type=std::string MyMidiPlayer::*;friend type creditsMember(CreditsFilename);};
template struct CreditsTestAccess<CreditsFilename,&MyMidiPlayer::timbre_lib_filename>;
// Audio already grants this test/helper name access; no new production API.
class Tired_of_compiler_warnings {
public:static void disposeAudio(){delete Audio::self;Audio::self=nullptr;}
};
// The real MyMidiPlayer route, original external assets, and existing classic
// driver oracle protect credits/menu integration. No recovered data is embedded.
class Opl3CreditsRegression {
    static int owner(Opl3StereoMidiDriver& d,uint64 id) {
        for(unsigned v=0;v<d.VoiceCount;++v)if(d.voice_identity[v]==id)return int(v);return -1;
    }
    static std::vector<uint8> record(const std::vector<uint8>& bank,unsigned program) {
        for(unsigned i=0;i<bank[0];++i)if(bank[1+i*48+47]==program)return {bank.begin()+1+i*48,bank.begin()+1+(i+1)*48};
        return {bank.begin()+1,bank.begin()+49}; // Original FindTimbre fallback.
    }
    static std::shared_ptr<Opl3StereoMidiDriver> synth() {
        auto d=std::make_shared<Opl3StereoMidiDriver>();d->setSfxSynchronous(true);
        require(d->initMidiDriver(48000,true)==0,"credits synchronous synth");return d;
    }
    static unsigned be32(const std::vector<uint8>& b,unsigned p) {
        return (unsigned(b[p])<<24)|(unsigned(b[p+1])<<16)|(unsigned(b[p+2])<<8)|b[p+3];
    }
    static std::vector<uint8> passage(const std::vector<uint8>& score,const char* label) {
        std::vector<uint8> result(score.begin(),score.begin()+14);result[10]=0;result[11]=2;
        unsigned end=22+be32(score,18);result.insert(result.end(),score.begin()+14,score.begin()+end);
        for(unsigned p=end;p+8<=score.size();) {
            end=p+8+be32(score,p+4);std::string bytes(score.begin()+p,score.begin()+end);
            if(bytes.find(label)!=std::string::npos){result.insert(result.end(),score.begin()+p,score.begin()+end);return result;}p=end;
        }
        throw std::runtime_error("original credits named passage missing");
    }
    static std::vector<EventLog> trace(XMidiEventList* list,ClockHandler& h) {
        list->incrementCounter();{XMidiSequence playback(&h,0,list,false,255,-1);pump(playback,h,list->getLength()*50+1000);playback.finish();}
        auto result=h.log;std::map<uint64,uint64> identities;uint64 next=0;
        for(auto& e:result)if(e.id){if(!identities.count(e.id))identities[e.id]=++next;e.id=identities[e.id];}return result;
    }
    static void checkScore(const std::vector<uint8>& score,const char* label,unsigned expectedNotes,unsigned expectedMapped,
                           const std::vector<uint8>& bank,const std::vector<uint8>& exe) {
        IBufferDataView a(score.data(),score.size()),b(score.data(),score.size());
        XMidiFile fm(&a,XMIDIFILE_CONVERT_NOCONVERSION,"OPL3Stereo"),baseline(&b,XMIDIFILE_CONVERT_NOCONVERSION,"OPL3Stereo");
        auto d=synth();d->setClassicContext(false,true);IBufferDataView input(bank.data(),bank.size());d->loadTimbreLibrary(&input,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
        ClockHandler h(true),plain(true);unsigned notes=0,mapped=0,ticks=0,p8=0,p47=0,p117=0;std::set<unsigned> pcmParts;
        const unsigned offsets[]={59199,59208,59216,59225,59233,59242,59251,59260,59269,59278,59287,59296,59305,59314,59323,59332,59341,59350,59359,59368,59377};
        std::map<uint64,int> balance;
        h.observe=[&](uint32 message,uint64 id) {
            while(ticks<h.now/100){++ticks;d->tickBgVoices();}
            unsigned type=message&240,c=message&15,key=(message>>8)&127,velocity=(message>>16)&127;
            if(id)balance[id]+=(type==144&&velocity)?1:-1;
            d->sendNote(message,id);
            if(type!=144 || !velocity)return;
            ++notes;int v=owner(*d,id);
            if(c==9 && (key>=88 || exe[111619+key]<10 || exe[111619+key]>30)) {
                require(v<0,"original unmapped credits trigger remains ignored");return;
            }
            require(v>=0,"credits source occurrence receives physical voice");
            require(d->opl->core.channel[v].slotz[0]->key!=0,"credits actual hardware key-on");
            unsigned expectedProgram=d->ch[c].inum;
            if(c==9) {
                require(key<88 && exe[111619+key]>=10 && exe[111619+key]<=30,"original credits mapped trigger");
                unsigned part=exe[111619+key]-10;expectedProgram=exe[offsets[part]];++mapped;
                require(d->chp[v].note==exe[111707+key],"credits original mapped pitch");
                require(d->bg_voice[v].intro_part==int(part),"credits retains original mapped part");
            }
            auto expected=record(bank,expectedProgram);const auto& ins=d->voice_timbre[v];
            require(ins.bg[47]==expected[47],"credits original program or original fallback");
            require(ins.mod_avekm==expected[0] && ins.car_avekm==expected[5] && ins.mod_ad==expected[2] && ins.car_ad==expected[7],"credits original operator/envelope definition");
            require(d->adlib_data[0x20+d->adlib_opadd[v]]==expected[0] && d->adlib_data[0x23+d->adlib_opadd[v]]==expected[5],"credits operators programmed on physical voice");
            require((d->adlib_data[0xc0+d->channelRegister(v)]&15)==(expected[10]&15),"credits original connection/feedback");
            if(c==2 && expectedProgram==8)++p8;if(c==3 && expectedProgram==47)++p47;
            if(c==1 && expectedProgram==117)++p117;
            // Render a short isolated note per encountered instrument, without
            // changing gain or the input events in the complete score trace.
            if(pcmParts.insert(expectedProgram).second) {
                // Independently execute the accepted local original-driver oracle.
                originalOpl2Reset(const_cast<uint8*>(bank.data()));
                for(unsigned part=0;part<21;++part)originalOpl2Event(part+9,0xc0,exe[offsets[part]],0);
                unsigned refChannel=c==9?exe[111619+key]-1:c,refKey=c==9?exe[111707+key]:key;
                if(c!=9)originalOpl2Event(refChannel,0xc0,expectedProgram,0);
                originalOpl2Event(refChannel,0x90,refKey,velocity);
                if(!expected[11]) {
                    auto* regs=originalOpl2Registers();int physical=-1;
                    for(unsigned v=0;v<9;++v)if(regs[0xb0+v]&32){physical=int(v);break;}
                    require(physical>=0,"original driver keys on restored melodic part");
                    unsigned op=(unsigned(physical)/3)*8+unsigned(physical)%3;
                    require(regs[0x20+op]==expected[0] && regs[0x23+op]==expected[5] && regs[0x60+op]==expected[2] && regs[0x63+op]==expected[7],"restored instrument agrees with executed original driver");
                }
                auto isolated=synth();isolated->setClassicContext(false,true);IBufferDataView patches(bank.data(),bank.size());isolated->loadTimbreLibrary(&patches,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
                isolated->ch[c]=d->ch[c];isolated->sendNote(message,900000);
                std::vector<sint16> pcm(19200);isolated->lowLevelProduceSamples(pcm.data(),9600);
                require(std::any_of(pcm.begin(),pcm.end(),[](auto x){return x!=0;}),"credits restored part produces nonzero PCM");
            }
        };
        auto received=trace(fm.GetEventList(0),h),expected=trace(baseline.GetEventList(0),plain);
        require(received.size()==expected.size(),"credits no added or removed events");
        for(unsigned i=0;i<received.size();++i)require(received[i].time==expected[i].time && received[i].message==expected[i].message && received[i].id==expected[i].id,"credits timing/velocity/pan/controllers/identity/deadlines unchanged");
        require(notes==expectedNotes && mapped==expectedMapped,"credits exact source note counts");
        for(auto[id,n]:balance)require(n==0,"credits independent mapped release pairing");
        require(std::none_of(std::begin(d->chp),std::end(d->chp),[](auto& v){return v.channel>=0;}),"credits stop releases every voice");
        if(std::string(label)=="JAMMIN")require(p8==99 && p47==99,"JAMMIN two original 99-note parts");
        if(std::string(label)=="EMPS")require(p117==28,"EMPS program117 28 notes unchanged");
        std::cout<<"PASS credits "<<label<<" notes="<<notes<<" mapped="<<mapped<<" p8="<<p8<<" p47="<<p47<<" p117="<<p117<<" original instruments/PCM/key-ons/unchanged event trace/releases/stop\n";
    }
public:
    static void run() {
        Configuration memory;auto oldConfig=config;config=&memory;
        auto oldGame=*creditsMember(CreditsGame{});*creditsMember(CreditsGame{})=BLACK_GATE;
        bool had=is_system_path_defined("<STATIC>");auto oldPath=get_system_path("<STATIC>");add_system_path("<STATIC>","C:/GOG Games/Ultima 7/STATIC");
        auto bank=scoreEntry(scoreBytes(get_system_path("<STATIC>/U7INTRO.TIM")),1),gameBank=scoreEntry(scoreBytes(get_system_path("<STATIC>/U7VOICE.FLX")),1);
        auto exe=scoreBytes(get_system_path("<STATIC>/../INTRO.EXE"));auto score=scoreEntry(scoreBytes(get_system_path("<STATIC>/INTROADM.DAT")),4);
        // The complete resource is tested, plus each named passage independently.
        unsigned mapped=0;{IBufferDataView input(score.data(),score.size());XMidiFile file(&input,XMIDIFILE_CONVERT_NOCONVERSION,"OPL3Stereo");
        for(auto* e=file.GetEventList(0)->events;e;e=e->next)if(e->getStatusType()==MidiStatus::NoteOn && e->data[1] && e->getChannel()==9 && e->data[0]<88 && exe[111619+e->data[0]]>=10 && exe[111619+e->data[0]]<=30)++mapped;}
        checkScore(score,"record4",2884,mapped,bank,exe);
        checkScore(passage(score,"JAMMIN"),"JAMMIN",459,191,bank,exe);
        checkScore(passage(score,"EMPS"),"EMPS",503,475,bank,exe);
        require(record(bank,117)==record(gameBank,117),"EMPS program117 unchanged bank bytes");
        require(record(bank,65)==record(bank,0),"program65 retains original first-record fallback");
        {
            auto d=synth();d->setClassicContext(false,true);IBufferDataView input(bank.data(),bank.size());d->loadTimbreLibrary(&input,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
            auto on=[](unsigned key){return 0x99|(key<<8)|(100<<16);};auto off=[](unsigned key){return 0x99|(key<<8);};
            d->send(0xb9|(10<<8)|(20<<16));d->sendNote(on(74),100);d->sendNote(on(74),101);int first=owner(*d,100),second=owner(*d,101);
            require(first>=0 && second>=0 && first!=second,"credits overlapping mapped identities");
            d->sendNote(off(74),101);require(owner(*d,100)==first && owner(*d,101)<0,"credits independent mapped release");
            d->send(0xb9|(10<<8)|(110<<16));require(d->voice_pan[first]==20,"credits pan captured at note-on");
            d->send(0xc0|(50<<8));for(unsigned i=0;i<18;++i)d->sendNote(0x90|((40+i)<<8)|(100<<16),200+i);
            require(owner(*d,100)<0,"credits uses unchanged voice stealing");int replacement=owner(*d,217);
            d->sendNote(off(74),100);require(owner(*d,217)==replacement,"credits delayed off cannot terminate replacement");
            d->send(0xb0|(123<<8));d->setClassicContext(false,false);IBufferDataView game(gameBank.data(),gameBank.size());d->loadTimbreLibrary(&game,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
            d->sendNote(on(74),500);require(owner(*d,500)<0,"menu mapping cannot leak into gameplay");
        }
        // Exercise actual MyMidiPlayer methods with no SDL audio device, window,
        // gameplay or saves. A disabled Audio singleton permits construction;
        // an existing synchronous synth then supplies the normal music driver.
        auto oldAudio=Audio::get_ptr();require(!oldAudio,"routing test requires unused Audio singleton");
        {memory.set("config/audio/enabled","no",false);Audio::Init();
        {MyMidiPlayer player;auto d=synth();player.*creditsMember(CreditsDriver{})=d;player.*creditsMember(CreditsInitialized{})=true;
            player.set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_ENDGAME);require(!d->ending_context && !d->ending_bank,"cinematic preload happens before activation");
            require(player.start_music(ENDSCORE_XMI,1,false,MyMidiPlayer::Force_Midi),"actual ending first-section route");
            require(d->ending_context && d->ending_bank,"actual preload-to-activation refresh");
            for(unsigned p:{53u,54u,58u,60u,61u,62u,63u})require(!d->ending_patches[p].empty() && d->xmidibanks[2]->insbank[p].tvfx_program==int(p),"all TVFX records ready before score dispatch");
            auto* cached=d->ending_patches[60].data();std::vector<sint16> pcm(1920);
            for(unsigned i=0;i<100;++i)d->produceSamples(pcm.data(),uint32(pcm.size()*sizeof(sint16)));
            player.stop_music();require(std::none_of(d->ending_voices.begin(),d->ending_voices.end(),[](auto& s){return s.status;}),"cinematic stop/skip cleans TVFX");
            require(player.start_music(ENDSCORE_XMI,2,false,MyMidiPlayer::Force_Midi),"actual ending second-section route");
            require(d->ending_bank && cached==d->ending_patches[60].data(),"unchanged ending context retains normal bank cache");
            player.stop_music();require(player.start_music(4,false,MyMidiPlayer::Force_Midi,INTROMUS),"actual credits route");
            require(d->original_menu && !d->original_intro && !d->ending_context && !d->ending_bank,"credits context separate from Guardian and TVFX");
            require((player.*creditsMember(CreditsFilename{}))==INTRO_TIM && d->bg_timbres[8].native_bg && d->bg_timbres[47].native_bg,"actual credits selects U7INTRO.TIM");
            player.stop_music();player.set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_INTRO);require(d->original_intro && !d->original_menu,"same-bank Guardian context transition reloads mapping");
            player.set_timbre_lib(MyMidiPlayer::TIMBRE_LIB_GAME);require(!d->original_intro && !d->original_menu && (player.*creditsMember(CreditsFilename{}))==U7VOICE_FLX,"gameplay bank/context restored");
        }Tired_of_compiler_warnings::disposeAudio();}
        if(had)add_system_path("<STATIC>",oldPath);else clear_system_path("<STATIC>");*creditsMember(CreditsGame{})=oldGame;config=oldConfig;
        std::cout<<"PASS actual cinematic preload/context/FM sequence3+4/cache/stop/credits/Guardian/gameplay routing; mapped overlap/steal/stale-off/pan\n";
    }
};
