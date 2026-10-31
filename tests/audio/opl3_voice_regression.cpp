// Focused regression checks against the actual OPL3 driver, emulator and XMIDI sequencer.
// Run run-opl3-regression.ps1 after building the fork; no game assets or audio device required.
#include "pent_include.h"
#include "Opl3StereoMidiDriver.h"
#include "XMidiSequence.h"
// Inspect actual chip registers without introducing a production debug API.
#include "../../audio/midi_drivers/NukedOpl3.cpp"
#include <map>
#include <stdexcept>
#include <vector>
#include "OriginalOpl2MidiDriver.h"
// Test-only interception forwards every real YM3812 write unchanged. Including
// the production TU permits ordered-write comparison without a debug API.
static std::vector<uint32> authentic_opl2_writes;
namespace FMOpl_Pentagram {
static void observedOriginalWrite(FM_OPL* chip,int port,int value) {
    static unsigned address=0;
    if(port==0) address=value;
    else authentic_opl2_writes.push_back(address|(unsigned(value)<<8));
    OPLWrite(chip,port,value);
}
}
#define OPLWrite observedOriginalWrite
#include "../../audio/midi_drivers/OriginalOpl2MidiDriver.cpp"
#undef OPLWrite

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
class Opl3VoiceRegression {
    static uint32 on(int pitch=60, int channel=0) { return 0x90 | channel | (pitch<<8) | (100<<16); }
    static uint32 off(int pitch=60, int channel=0) { return 0x90 | channel | (pitch<<8); }
    static int active(Opl3StereoMidiDriver& d) {
        return std::count_if(std::begin(d.chp),std::end(d.chp),[](const auto& v){ return v.channel>=0; });
    }
    static int voice(Opl3StereoMidiDriver& d,uint64 id) {
        for(int i=0;i<d.VoiceCount;i++) if(d.voice_identity[i]==id) return i;
        return -1;
    }
    static std::unique_ptr<Opl3StereoMidiDriver> driver() {
        auto d=std::make_unique<Opl3StereoMidiDriver>();
        d->setSfxSynchronous(true); // Existing synchronous command service; no callback thread.
        require(d->initMidiDriver(48000,true)==0,"driver initialization");
        d->send(0xc0); d->send(0xc1);
        return d;
    }
public:
    static void fidelity();
    static void guardian();
    static void serpentFidelity();
    static void playRecoveredScore(XMidiEventList*, const std::vector<bool>&, IDataSource&);
    static void run() {
        {
            auto d=driver();
            for(int i=0;i<18;i++) d->sendNote(on(40+i),i+1);
            require(active(*d)==18,"all 18 voices available");
            for(int i=0;i<18;i++) {
                require(voice(*d,i+1)==i,"deterministic physical allocation");
                require(d->opl->core.channel[i].slotz[0]->key!=0,"key-on reaches each hardware channel");
                require(d->opl->core.channel[i].f_num!=0,"frequency reaches both register banks");
                require(d->opl->core.channel[i].cha&&d->opl->core.channel[i].chb,"both stereo buses enabled");
            }
            require(!d->opl->core.rhy,"hardware rhythm mode remains disabled");
            for(auto& c:d->opl->core.channel) require(c.chtype==0,"no four-operator reservation");
            d->sendNote(on(80),19);
            require(voice(*d,1)<0&&voice(*d,19)==0,"oldest active note stolen");
            d->sendNote(off(40),1);
            require(voice(*d,19)==0&&active(*d)==18,"late release cannot stop replacement");
            d->sendNote(off(50),11); d->sendNote(off(42),3);
            d->sendNote(on(81),20); d->sendNote(on(82),21);
            require(voice(*d,20)==10&&voice(*d,21)==2,"oldest released voice reused first");
            std::vector<sint16> pcm(4096);d->lowLevelProduceSamples(pcm.data(),2048);
            require(std::any_of(pcm.begin(),pcm.end(),[](auto x){return x!=0;}),"18-voice PCM output");
            d->send(0xb0 | (123<<8));require(active(*d)==0,"stop releases both banks");
            d->destroyMidiDriver();require(!d->opl&&active(*d)==0,"shutdown clears hardware and ownership");
            d->destroyMidiDriver();
        }
        std::cout<<"PASS capacity/banks/exhaustion/late-off/release-order/shutdown\n";
        {
            auto d=driver();d->sendNote(on(),100);d->sendNote(on(),101);
            require(active(*d)==2,"identical pitch occurrences overlap");
            d->sendNote(off(),101);require(voice(*d,100)>=0&&voice(*d,101)<0,"release exact instance");
            d->sendNote(off(),100);require(active(*d)==0,"independent instance release");
            d->send(0xb0|(10<<8)|(20<<16));d->sendNote(on(),102);
            int v=voice(*d,102);auto pan=d->opl->core.channel[v].leftpan;
            d->send(0xb0|(10<<8)|(110<<16));d->sendNote(on(64),103);
            require(d->voice_pan[v]==20&&d->opl->core.channel[v].leftpan==pan,"pan captured at note-on");
            auto frequency=d->opl->core.channel[v].f_num;d->send(0xe0|(100<<16));
            require(d->opl->core.channel[v].f_num!=frequency,"pitch updates active voices");
            d->send(0xb0|(7<<8)|(40<<16));require(d->ch[0].vol==40,"volume controller retained");
            d->sendNote(on(67,1),104);d->send(0xb0|(123<<8));
            require(active(*d)==1&&voice(*d,104)>=0,"channel-scoped stop preserves other channel");
        }
        std::cout<<"PASS overlaps/pitch/volume/pan/channel-stop/destruction\n";
        {
            auto d=driver();
            for(int i=0;i<9;i++)d->sendNote(on(40+i),i+200);
            // Exercise high-bank operator programming and controller updates explicitly.
            d->send(0xc1|(40<<8));d->send(0xb1|(10<<8)|(110<<16));d->sendNote(on(70,1),209);
            require(voice(*d,209)==9,"high-bank note selected");
            require(d->opl->core.channel[9].slotz[0]->reg_mult==(d->ch[1].ins.mod_avekm&15),"high-bank instrument preserved");
            auto old=d->opl->core.channel[9].f_num;d->send(0xe1|(100<<16));
            require(d->opl->core.channel[9].f_num!=old,"high-bank pitch update");
            auto level=d->opl->core.channel[9].slotz[1]->reg_tl;d->send(0xb1|(7<<8)|(20<<16));
            require(d->opl->core.channel[9].slotz[1]->reg_tl!=level,"high-bank volume update");
        }
        std::cout<<"PASS high-bank instrument/pitch/volume\n";
    }
};

struct EventLog { uint32 time,message;uint64 id; };
struct ClockHandler:XMidiSequenceHandler {
    uint32 now=0;bool instances;std::vector<EventLog> log;
    std::function<void(uint32,uint64)> observe;
    explicit ClockHandler(bool enabled):instances(enabled){}
    bool usesNoteInstances() const override{return instances;}
    void sequenceSendEvent(uint16,uint32 m)override{log.push_back({now,m,0});if(observe)observe(m,0);}
    void sequenceSendNote(uint16,uint32 m,uint64 id)override{log.push_back({now,m,id});if(observe)observe(m,id);}
    void sequenceSendSysEx(uint16,uint8,const uint8*,uint16)override{}
    uint32 getTickCount(uint16)override{return now;}
    void handleCallbackTrigger(uint16,uint8)override{}
    int getGlobalVolume()override{return 100;}
};
static XMidiEvent* event(int tick,int status,int a,int b,int duration=0) {
    auto*e=XMidiEvent::Create();e->time=tick;e->status=status;e->data[0]=a;e->data[1]=b;e->ex.note_on.duration=duration;return e;
}
static XMidiEventList* events(std::initializer_list<XMidiEvent*> entries) {
    auto*l=XMidiEventList::Create();auto**at=&l->events;for(auto*e:entries){*at=e;at=&e->next;}return l;
}
static void pump(XMidiSequence&s,ClockHandler&h,uint32 until) {
    for(;h.now<=until;++h.now)for(int n=0;n<8;++n)if(s.playEvent()<0)break;
}
static std::vector<EventLog> notes(ClockHandler&h) {
    std::vector<EventLog> result;for(auto&e:h.log)if((e.message&0xf0)==0x90)result.push_back(e);return result;
}
static void sequencing() {
    {ClockHandler h(true);{XMidiSequence s(&h,0,events({event(0,0x90,60,100,20),event(2,0x90,60,90,5)}),false,255,-1);
    pump(s,h,1000);auto n=notes(h);require(n.size()==4,"overlap event count");
    require(n[0].id!=n[1].id&&n[2].id==n[1].id&&n[2].time==300&&n[3].id==n[0].id&&n[3].time==950,"independent deadlines");}}
    {ClockHandler h(true);{XMidiSequence s(&h,0,events({event(0,0xb0,116,2),event(0,0x90,60,100,20),event(1,0xb0,117,127)}),false,255,-1);
    pump(s,h,1300);std::map<uint64,int> balance;for(auto&e:notes(h))balance[e.id]+=(e.message>>16)?1:-1;
    require(balance.size()>=2,"loop instances");for(auto[id,b]:balance)require(id&&b==0,"loop releases");}}
    {ClockHandler h(true);{XMidiSequence s(&h,0,events({event(0,0x90,60,100,20)}),false,255,-1);
    pump(s,h,99);h.now=100;s.pause();pump(s,h,499);h.now=500;s.unpause();pump(s,h,1400);auto n=notes(h);
    require(n.size()==4&&n[0].id==n[2].id&&n[0].id==n[3].id&&n[3].time==1350,"pause identity and deadline");}}
    {ClockHandler h(true);{auto*b=event(3,0xb0,120,5);auto*l=events({event(0,0x90,50,100,5),b,event(4,0x90,60,100,10)});l->branches=b;
    XMidiSequence s(&h,0,l,false,255,5);pump(s,h,1000);auto n=notes(h);require(n.size()==2&&((n[0].message>>8)&127)==60,"branch entry");}}
    {ClockHandler h(true);{XMidiSequence s(&h,0,events({event(0,0xb0,10,20),event(0,0x90,60,100,20),event(1,0xb0,10,100)}),false,255,-1);
    pump(s,h,99);s.loseChannel(0);pump(s,h,199);s.gainChannel(0);auto n=notes(h);require(n.size()==3&&n[0].id==n[2].id,"channel restoration identity");
    bool pan=false;for(size_t i=1;i<h.log.size();i++)if(h.log[i].id==n[0].id&&(h.log[i].message>>16))pan=((h.log[i-1].message>>16)&127)==20;
    require(pan,"channel restoration captured pan");s.finish();auto count=notes(h).size();pump(s,h,1500);require(notes(h).size()==count,"finish clears deadlines");}}
    {ClockHandler h(true);{XMidiSequence s(&h,0,events({event(0,0x90,60,100,20)}),false,255,-1);pump(s,h,99);}
    auto n=notes(h);require(n.size()==2&&n[0].id==n[1].id,"destructor releases");}
    std::cout<<"PASS sequencer overlap/deadlines/loops/branch/pause/channel-restoration/finish/destructor\n";
}
static void legacyTrace() {
    ClockHandler h(false);{XMidiSequence s(&h,0,events({event(0,0x90,60,100,20),event(2,0x90,60,90,5),event(5,0x90,62,80,10)}),false,255,-1);
    pump(s,h,150);h.now=200;s.pause();pump(s,h,300);s.unpause();pump(s,h,1400);s.finish();}
    for(auto&e:h.log)std::cout<<"LEGACY "<<e.time<<" "<<e.message<<" "<<e.id<<"\n";
}
#include "opl3_score_recovery_checks.h"

void Opl3VoiceRegression::fidelity() {
    // Synthetic parameters isolate controller and fixed-point operations; the
    // second section loads the user's unchanged original bank for partials.
    auto setup=[](Opl3StereoMidiDriver& d) {
        auto& ins=d.ch[0].ins; ins.native_bg=true;ins.bg={};ins.bg[12]=7;ins.bg[13]=0;
        ins.bg[14]=16;ins.bg[16]=8;ins.bg[17]=2;
        ins.bg[20]=16;ins.bg[22]=64;ins.bg[32]=8;
        ins.car_ksl_tl=0x80|30;ins.mod_ksl_tl=0x40|20;ins.fb_c=0;
        d.bg_bank=true;
    };
    {
        auto d=driver();d->send(0xb0|(64<<8)|(127<<16));d->sendNote(on(),1);d->sendNote(off(),1);
        require(active(*d)==1,"SI sustain remains active");d->send(0xb0|(64<<8));require(active(*d)==0,"SI pedal release");
        d->send(0xb0|(64<<8)|(63<<16));d->sendNote(on(),10);d->sendNote(off(),10);
        require(active(*d)==0,"SI sustain boundary is 64");
        setup(*d);d->send(0xb0|(64<<8)|(127<<16));d->sendNote(on(),2);int v=voice(*d,2);
        require(d->opl->core.channel[v].slotz[1]->reg_ksl==2,"native carrier KSL retained");
        require(d->opl->core.channel[v].slotz[1]->reg_tl==0,"native raw velocity sensitivity");
        require(d->opl->core.channel[v].slotz[0]->reg_tl==19,"native modulator attenuation");
        auto level=d->opl->core.channel[v].slotz[1]->reg_tl;
        d->send(0xb0|(11<<8));d->send(0xb0|(7<<8));d->send(0xe0|(100<<16));
        require(d->opl->core.channel[v].slotz[1]->reg_tl==level,"BG expression/CC7 do not rewrite held level on bend");
        d->sendNote(off(),2);require(active(*d)==0,"BG ignores sustain");d->tickBgVoices();
        require(!d->opl->core.channel[v].slotz[0]->key,"release automation cannot re-key voice");
        d->sendNote(on(),3);v=voice(*d,3);
        require(d->opl->core.channel[v].slotz[1]->reg_tl>level,"CC7 affects subsequent BG note");
        int slide=d->bg_voice[v].slide;d->tickBgVoices();
        require(d->bg_voice[v].slide==slide+16&&d->bg_voice[v].phase==8,"60Hz pitch envelope and vibrato");
        auto freq=d->opl->core.channel[v].f_num;d->ch[0].ins.bg[14]=0;d->send(0xe0);auto unbent=d->opl->core.channel[v].f_num;
        d->send(0xe0|(127<<16));require(d->opl->core.channel[v].f_num==unbent,"zero instrument bend range");
        require(freq!=unbent,"instrument bend range applied");
    }
    {
        auto d=driver();d->sendNote(on(),1);int v=voice(*d,1);auto ins=d->voice_timbre[v];
        OPL3_WriteReg(&d->opl->core,0x60+d->adlib_opadd[v],0);
        d->midi_fm_instrument(v,ins);require(d->opl->core.channel[v].slotz[0]->reg_ar==0,"unchanged timbre not reprogrammed");
        ins.mod_ad=0xf1;d->midi_fm_instrument(v,ins);
        require(d->opl->core.channel[v].slotz[0]->reg_ar==15,"changed timbre reprogrammed");
        d->send(0xb0|(1<<8)|(127<<16));
        require(d->adlib_data[0x20+d->adlib_opadd[v]]==(ins.mod_avekm|0x40),"SI vibrato preserves modulator bits");
        require(d->adlib_data[0x23+d->adlib_opadd[v]]==(ins.car_avekm|0x40),"SI vibrato reaches carrier");
        d->send(0xb0|(1<<8));require(d->adlib_data[0x20+d->adlib_opadd[v]]==ins.mod_avekm,"vibrato resets original flags");
    }
    {
        auto a=driver(),b=driver();setup(*a);setup(*b);a->sendNote(on(),1);b->sendNote(on(),1);
        std::vector<sint16> whole(9600),pieces(9600);a->lowLevelProduceSamples(whole.data(),4800);
        for(unsigned at=0;at<4800;){unsigned n=std::min(13u,4800-at);b->lowLevelProduceSamples(pieces.data()+at*2,n);at+=n;}
        require(whole==pieces,"60Hz automation independent of render block partition");
        auto muted=driver();setup(*muted);muted->sendNote(on(),1);
        static_cast<MidiDriver&>(*muted).setGlobalVolume(0);std::vector<sint16> silence(3200);
		muted->yield();
        muted->lowLevelProduceSamples(silence.data(),1600);
        require(std::all_of(silence.begin(),silence.end(),[](auto x){return x==0;}),"native BG master mute");
        auto full=driver(),half=driver();setup(*full);setup(*half);
        full->sendNote(on(),1);half->sendNote(on(),1);static_cast<MidiDriver&>(*half).setGlobalVolume(50);
		full->yield();half->yield();
        require(static_cast<MidiDriver&>(*half).getGlobalVolume()==50,"Audio Options reads actual master volume");
        require(half->getSequenceGlobalVolume()==100,"BG native channel levels independent of master gain");
        std::vector<sint16> x(3200),y(3200);full->lowLevelProduceSamples(x.data(),1600);half->lowLevelProduceSamples(y.data(),1600);
        require(std::any_of(x.begin(),x.end(),[](auto v){return v!=0;}),"native test renders audio");
        for(size_t i=0;i<x.size();++i)require(y[i]==x[i]/2,"master gain applied exactly once");
    }
    const char* overrideDir=std::getenv("EXULT_OPL3_BG_STATIC");
    auto bankFile=scoreBytes(std::string(overrideDir?overrideDir:"C:/GOG Games/Ultima 7/STATIC")+"/U7VOICE.FLX");
    auto bytes=scoreEntry(bankFile,1);
    for(int program:{36,40}) {
        auto d=driver();IBufferDataView bank(bytes.data(),bytes.size());d->loadTimbreLibrary(&bank,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
        auto flags=d->xmidibanks[0]->insbank[program].perc_voice;
        for(int p=0;p<128;++p)if(!d->bg_timbres[p].native_bg) {
            d->send(0xc0|(p<<8));require(d->ch[0].ins.bg==d->bg_default.bg,"BG unknown program uses first original timbre");break;
        }
        d->send(0xc0|(program<<8));d->sendNote(on(),700);
        require(active(*d)==2,"original BG secondary partial audible");
        for(int v=0;v<18;++v)if(d->chp[v].channel>=0)require(d->voice_identity[v]==700,"partial shares note identity");
        require(d->xmidibanks[0]->insbank[program].perc_voice==flags,"source bank flags immutable");
        d->sendNote(off(),700);require(active(*d)==0,"one release stops both partials");
        d->send(0xc1);for(int i=0;i<17;++i)d->sendNote(on(40+i,1),800+i);
        d->sendNote(on(),900);require(active(*d)==18,"secondary partial does not steal a full voice");
        for(int i=0;i<17;++i)require(voice(*d,800+i)>=0,"secondary preserves existing occurrences");
    }
    std::cout<<"PASS FM fidelity: BG levels/controllers/partials/envelopes/bend/mute; SI vibrato/sustain; timbre cache; PCM partition continuity\n";
}

void Opl3VoiceRegression::serpentFidelity() {
    const bool hadPath=is_system_path_defined("<STATIC>");
    auto oldPath=get_system_path("<STATIC>");
    const char* overrideDir=std::getenv("EXULT_OPL3_SI_STATIC");
    add_system_path("<STATIC>",overrideDir?overrideDir:"C:/GOG Games/Ultima 7 - Serpent Isle/STATIC");
    auto bytes=scoreBytes(get_system_path("<STATIC>/XMIDI.AD"));
    auto d=driver();IBufferDataView bank(bytes.data(),bytes.size());d->loadTimbreLibrary(&bank,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
    require(d->si_bank,"known original SI driver loads frequency table");d->send(0xc0|(15<<8));
    require(d->ch[0].ins.native_si,"SI simple instrument profile");
    d->sendNote(on(),5000);int v=voice(*d,5000);
    d->chp[v].velocity=127;d->midi_fm_volume(v,0);
    require(d->opl->core.channel[v].slotz[1]->reg_tl==17,"SI program15 carrier before TL0, corrected TL17");
    auto expected=[&](int key,int transpose,int wheel,bool percussion) {
        auto sar=[](int x,int bits){int div=1<<bits;return x<0?-((-x+div-1)/div):x/div;};
        int n=(percussion?transpose:key+transpose)-24;
        do {n+=12;}while(n<0);n+=12;do {n-=12;}while(n>95);
        int p=sar(sar(wheel-8192,5)*12+n*256+8,4)-192;
        do {p+=192;}while(p<0);p+=192;do {p-=192;}while(p>1535);
        int f=d->si_frequencies[(p>>4)%12*16+(p&15)];int block=(p>>4)/12-1;
        if(f&32768)++block;
        if(block<0){++block;f=sar(sint16(f),1);}
        return (f&1023)|(block<<10);
    };
    unsigned compared=0;
    for(int key=0;key<128;++key) {
        d->chp[v].note=key;
        for(int wheel=0;wheel<16384;wheel+=32) {
            d->send(0xe0|((wheel&127)<<8)|((wheel>>7)<<16));
            int word=d->adlib_data[0xa0+d->channelRegister(v)]|((d->adlib_data[0xb0+d->channelRegister(v)]&31)<<8);
            require(word==expected(key,0,wheel,false),"SI exact frequency register comparison");++compared;
        }
        d->send(0xe0|(127<<8)|(127<<16));
        int word=d->adlib_data[0xa0+d->channelRegister(v)]|((d->adlib_data[0xb0+d->channelRegister(v)]&31)<<8);
        require(word==expected(key,0,16383,false),"SI maximum positive wheel");++compared;
    }
    require(voice(*d,5000)==v,"pitch changes retain occurrence ownership");
    d->send(0xb0|(121<<8));require(d->ch[0].pitchbend==8192,"SI pitch reset");
    auto scale=[](int x,int y){int z=x*y/128;return z?z+1:0;};
    unsigned levels=0;
    for(int b:{0,1,127}) {
        if(!d->xmidibanks[b])continue;
        for(int p=0;p<128;++p) {
            auto ins=d->xmidibanks[b]->insbank[p];if(!ins.native_si)continue;
            d->send(0xb0|(114<<8)|(b<<16));d->send(0xc0|(p<<8));d->sendNote(on(),6000);v=voice(*d,6000);
            for(int vel:{1,32,64,100,127}) for(int vol:{0,63,127}) for(int expression:{0,63,127}) {
                d->chp[v].velocity=vel;d->send(0xb0|(7<<8)|(vol<<16));d->send(0xb0|(11<<8)|(expression<<16));
                int gain=scale(scale(vol,expression),82+3*(vel>>3));
                int car=63-(63-(ins.car_ksl_tl&63))*gain/127;
                int mod=(ins.fb_c&1)?63-(63-(ins.mod_ksl_tl&63))*gain/127:ins.mod_ksl_tl&63;
                require(d->opl->core.channel[v].slotz[1]->reg_tl==car,"SI carrier attenuation");
                require(d->opl->core.channel[v].slotz[0]->reg_tl==mod,"SI modulation/additive attenuation");
                require(d->opl->core.channel[v].slotz[1]->reg_ksl==(ins.car_ksl_tl>>6),"SI KSL retained");++levels;
            }
            d->sendNote(off(),6000);
        }
    }

    for(int p=0;p<128;++p)if(d->xmidibanks[127]&&d->xmidibanks[127]->insbank[p].native_si) {
        d->sendNote(on(p,9),7500);int drum=voice(*d,7500);auto ins=d->voice_timbre[drum];
        int word=d->adlib_data[0xa0+d->channelRegister(drum)]|((d->adlib_data[0xb0+d->channelRegister(drum)]&31)<<8);
        require(word==expected(p,ins.transpose,8192,true),"SI percussion uses bank key rather than trigger pitch");
        d->sendNote(off(p,9),7500);break;
    }
    d->send(0xb0|(114<<8));d->send(0xc0|(15<<8));d->send(0xb0|(7<<8)|(127<<16));d->send(0xb0|(11<<8)|(127<<16));
    d->sendNote(on(),7000);v=voice(*d,7000);d->send(0xb0|(64<<8)|(127<<16));d->sendNote(off(),7000);
    require(voice(*d,7000)==v,"SI sustain holds occurrence");d->send(0xe0|(100<<16));d->send(0xb0|(64<<8));require(voice(*d,7000)<0,"SI pedal release ends occurrence");
    d->sendNote(on(),7001);static_cast<MidiDriver&>(*d).setGlobalVolume(0);d->yield();std::vector<sint16> pcm(4096);d->lowLevelProduceSamples(pcm.data(),2048);
    require(std::all_of(pcm.begin(),pcm.end(),[](auto x){return x==0;}),"SI master mute");
    static_cast<MidiDriver&>(*d).setGlobalVolume(50);d->yield();d->lowLevelProduceSamples(pcm.data(),2048);
    require(std::any_of(pcm.begin(),pcm.end(),[](auto x){return x!=0;}),"SI unmute renders audio");
    require(static_cast<MidiDriver&>(*d).getGlobalVolume()==50&&d->getSequenceGlobalVolume()==100,"SI UI volume distinct from channel levels");
    if(hadPath) add_system_path("<STATIC>",oldPath);else clear_system_path("<STATIC>");
    std::cout<<"PASS SI native pitch comparisons="<<compared<<" max deviation=0 cents; operator cases="<<levels<<"; program15 TL0 -> TL17; sustain/master mute/unmute/UI\n";
}

#include "opl3_guardian_checks.h"
#include "original_opl2_checks.h"
#include "serpent_opl2_checks.h"

int wmain(int argc,wchar_t**argv) {
    try {if(argc>1&&std::wstring(argv[1])==L"--legacy-trace"){legacyTrace();return 0;}
    Opl3VoiceRegression::run();Opl3VoiceRegression::fidelity();Opl3VoiceRegression::serpentFidelity();Opl3VoiceRegression::guardian();sequencing();scoreRecovery();OriginalOpl2Regression::run();SerpentOpl2Regression::run();std::cout<<"PASS all OPL3 and Original OPL2 regressions\n";return 0;}
    catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
