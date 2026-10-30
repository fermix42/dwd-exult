// Source-backed score recovery checks; no original music data is embedded in the tests.
#include "Opl3MusicPan.h"
#include "XMidiFile.h"
#include "Configuration.h"
#include <fstream>
#include <set>
#include <tuple>
using ScoreNote=std::tuple<int,int,int,int,int>; // time, channel, pitch, velocity, duration
static std::map<ScoreNote,int> scoreNotes(XMidiEventList* l,int offset=0) {
    std::map<ScoreNote,int> result;
    for(auto*e=l->events;e;e=e->next)if(e->getStatusType()==MidiStatus::NoteOn&&e->data[1])
        ++result[{e->time-offset,e->getChannel(),e->data[0],e->data[1],e->ex.note_on.duration}];
    return result;
}
static int noteCount(XMidiEventList* l) {int n=0;for(auto[k,c]:scoreNotes(l))n+=c;return n;}
static int firstNote(XMidiEventList*l) {for(auto*e=l->events;e;e=e->next)if(e->getStatusType()==MidiStatus::NoteOn&&e->data[1])return e->time;return 0;}
static std::vector<uint8> scoreBytes(const std::string&name) {
    std::ifstream f(name,std::ios::binary);require(bool(f),"original score archive unavailable");
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
static uint32 le32(const std::vector<uint8>&b,size_t at) {require(at+4<=b.size(),"archive bounds");return uint32(b[at])|(uint32(b[at+1])<<8)|(uint32(b[at+2])<<16)|(uint32(b[at+3])<<24);}
static std::vector<uint8> scoreEntry(const std::vector<uint8>&b,int id) {
    auto off=le32(b,128+id*8),size=le32(b,132+id*8);require(off<=b.size()&&size<=b.size()-off,"entry bounds");
    return {b.begin()+off,b.begin()+off+size};
}

void Opl3VoiceRegression::playRecoveredScore(XMidiEventList* list,const std::vector<bool>& recovered,IDataSource& bank) {
    auto d=driver();d->loadTimbreLibrary(&bank,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
    ClockHandler h(true);size_t index=0;unsigned keyed=0;
    h.observe=[&](uint32 message,uint64 id) {
        if(id)d->sendNote(message,id);else d->send(message);
        if((message&0xf0)==0x90&&(message>>16)) {
            require(index<recovered.size(),"unexpected extra note-on");
            if(recovered[index]) {
                int v=voice(*d,id);require(v>=0,"recovered note did not acquire a voice");
                require(d->opl->core.channel[v].slotz[0]->key!=0,"recovered note did not key on hardware");++keyed;
            }
            ++index;
        }
    };
    list->incrementCounter();{XMidiSequence sequence(&h,0,list,false,255,-1);pump(sequence,h,list->getLength()*50+30000);sequence.finish();}
    require(index==recovered.size()&&keyed==128,"recovered hardware note count");
    require(active(*d)==0,"recovered score leaves hanging voices after stop");
    std::cout<<"PASS track29 original-FM-bank hardware key-ons="<<keyed<<"; stop clears voices\n";
}
static void recoveryGuards() {
    for(int mode=0;mode<6;++mode) {
        auto* fm=events({event(0,0xc4,49,0),event(0,0x94,60,100,3),event(10,0x94,64,100,2)});
        auto* play=events({event(0,0xc4,49,0),event(0,0x94,60,100,3),event(10,0x94,64,100,2)});
        auto* mt=events({event(0,0xc4,mode==1?50:49,0),event(0,0x94,60,100,9),event(5,0x94,62,100,3),event(10,0x94,64,100,2)});
        if(mode==2){auto*e=event(0,0xe4,0,80);e->next=mt->events;mt->events=e;}
        if(mode==3){auto*e=event(0,0xb4,64,127);e->next=mt->events;mt->events=e;}
        if(mode==4){for(auto*l:{fm,mt,play}){auto*e=event(0,0xb4,116,2);e->next=l->events;l->events=e;}}
        if(mode==5){auto*e=event(5,0x94,62,100,3);e->next=mt->events->next->next;mt->events->next->next=e;}
        auto before=scoreNotes(play);auto added=recoverOpl3MusicNotes(play,fm,mt);
        require(added==(mode==0?1u:0u),"correspondence guard");
        for(auto[key,count]:before)require(scoreNotes(play)[key]>=count,"existing gate duration changed");
        if(!mode)require(recoverOpl3MusicNotes(play,fm,mt)==0,"duplicate recovery guard");
        play->decrementCounter();fm->decrementCounter();mt->decrementCounter();
    }
    std::cout<<"PASS recovery guards: instrument/bend/sustain/loop/unison; preserve FM duration\n";
}
static void scoreRecovery() {
    Configuration memoryConfig;memoryConfig.read_config_string("<config><audio><midi><volume_curve>1</volume_curve></midi></audio></config>");
    struct RestoreConfig {Configuration* old=config;~RestoreConfig(){config=old;}} restore;
    config=&memoryConfig;
    recoveryGuards();
    for(int game=0;game<2;++game) {
        const char* overridePath=std::getenv(game?"EXULT_OPL3_SI_STATIC":"EXULT_OPL3_BG_STATIC");
        const std::string dir=overridePath?overridePath:game?"C:/GOG Games/Ultima 7 - Serpent Isle/STATIC":"C:/GOG Games/Ultima 7/STATIC";
        auto fmBank=scoreBytes(dir+"/ADLIBMUS.DAT"),mtBank=scoreBytes(dir+"/MT32MUS.DAT");
        int checked=0,total=0,changed=0;
        for(uint32 id=0;id<le32(fmBank,84);++id) {
            auto f=scoreEntry(fmBank,id);if(f.empty())continue;
            auto m=scoreEntry(mtBank,id);if(m.empty())continue;
            IBufferDataView fs(f.data(),f.size()),ms(m.data(),m.size()),ps(f.data(),f.size());
            XMidiFile fm(&fs,XMIDIFILE_CONVERT_NOCONVERSION,"OPL3Stereo",true),mt(&ms,XMIDIFILE_CONVERT_NOCONVERSION,"OPL3Stereo",true),play(&ps,XMIDIFILE_CONVERT_NOCONVERSION,"OPL3Stereo");
            require(fm.number_of_tracks()&&mt.number_of_tracks()&&play.number_of_tracks(),"parse original scores");
            auto*l=play.GetEventList(0);auto*fl=fm.GetEventList(0);auto*ml=mt.GetEventList(0);
            const int offset=firstNote(l)-firstNote(fl);auto before=scoreNotes(l,offset),source=scoreNotes(ml);
            const int original=noteCount(l);
            transferOpl3MusicPan(l,fl,ml);
            const unsigned recovered=recoverOpl3MusicNotes(l,fl,ml);
            auto after=scoreNotes(l,offset);
            for(auto[key,count]:before)require(after[key]>=count,"original FM notes changed");
            for(auto[key,count]:after)if(count>before[key])require(source[key]>=count-before[key],"recovered note lacks exact source event");
            require(noteCount(l)==original+int(recovered),"recovery count mismatch");
            require(recoverOpl3MusicNotes(l,fl,ml)==0,"repeated recovery duplicates notes");
            int previous=-1;for(auto*e=l->events;e;e=e->next){require(e->time>=previous,"event timing reordered");previous=e->time;}
            if(!game&&id==29){std::cout<<"TRACK29 FM="<<original<<" MT32="<<noteCount(ml)<<" recovered="<<recovered<<" final="<<noteCount(l)<<"\n";require(original==587&&recovered==128&&noteCount(l)==715,"track 29 acceptance");
                std::vector<bool> flags;auto originals=before;
                for(auto*e=l->events;e;e=e->next)if(e->getStatusType()==MidiStatus::NoteOn&&e->data[1]) {
                    ScoreNote key{e->time-offset,e->getChannel(),e->data[0],e->data[1],e->ex.note_on.duration};
                    bool extra=originals[key]==0;if(!extra)--originals[key];flags.push_back(extra);
                }
                auto bankFile=scoreBytes(dir+"/U7VOICE.FLX");auto bankEntry=scoreEntry(bankFile,1);IBufferDataView bank(bankEntry.data(),bankEntry.size());
                Opl3VoiceRegression::playRecoveredScore(l,flags,bank);
                ClockHandler h(true);l->incrementCounter();{XMidiSequence seq(&h,0,l,false,255,-1);pump(seq,h,l->getLength()*50+30000);auto n=notes(h);std::map<uint64,int> balance;int ons=0;for(auto&e:n){bool on=e.message>>16;balance[e.id]+=on?1:-1;if(on)++ons;}require(ons==715,"recovered notes not dispatched");for(auto[id,b]:balance)require(id&&b==0,"recovered identity release mismatch");}
            }
            if(recovered)++changed;total+=recovered;++checked;
        }
        require(checked==(game?67:54),"soundtrack coverage count");
        std::cout<<"PASS "<<(game?"SI":"BG")<<" score coverage="<<checked<<" recovered="<<total<<" changed tracks="<<changed<<"\n";
    }
}
