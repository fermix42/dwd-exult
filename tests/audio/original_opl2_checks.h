// Compare the new production driver to execution of the separate local
// recovered DOS driver. No game instruments or recovered tables are embedded.
#include "OriginalOpl2MidiDriver.h"
extern "C" void originalOpl2Reset(void*);
extern "C" void originalOpl2Event(unsigned,unsigned,unsigned,unsigned);
extern "C" void originalOpl2Tick();
extern "C" const uint8_t* originalOpl2Registers();
extern "C" int originalOpl2State(unsigned,unsigned);
class OriginalOpl2Regression {
	static void compare(OriginalOpl2MidiDriver& d,const char* operation) {
		auto ref=originalOpl2Registers();
		for(unsigned r=0;r<256;++r) if(d.registers[r]!=ref[r]) {
			// Program changes invalidate released voices' channel/timbre. The
			// DOS timer can then index its code as channel data (channel FF).
			// Production deliberately bounds this invalid-memory behavior.
			if(((r>=0xa0 && r<=0xa8)||(r>=0xb0 && r<=0xb8)) && d.voices[r&15].channel<0) continue;
			std::cerr<<operation<<" register "<<std::hex<<r<<" Exult="<<unsigned(d.registers[r])<<" reference="<<unsigned(ref[r])<<std::dec<<'\n';
			for(unsigned v=0;v<9;++v) if(d.voices[v].timbre>=0) {
				auto& s=d.voices[v]; std::cerr<<"voice="<<v<<" channel="<<s.channel<<" program="<<unsigned(d.bank[s.timbre][47])<<" stage="<<s.stage<<" pitch="<<s.pitch<<"/"<<originalOpl2State(0x42,v)<<" slide="<<s.slide<<"/"<<originalOpl2State(0xd1,v)<<" vibrato="<<s.vibrato<<"/"<<originalOpl2State(0x127,v)<<'\n';
			}
			throw std::runtime_error("Original OPL2 register mismatch");
		}
	}
	static std::unique_ptr<OriginalOpl2MidiDriver> driver(std::vector<uint8>& bytes,bool intro=false) {
		auto d=std::make_unique<OriginalOpl2MidiDriver>(); d->setSfxSynchronous(true);
		require(d->initMidiDriver(48000,true)==0,"original OPL2 initialization"); d->setContext(true,intro);
		IBufferDataView bank(bytes.data(),bytes.size()); d->loadTimbreLibrary(&bank,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
		require(d->ready,"original OPL2 validated bank");
		originalOpl2Reset(bytes.data());
		for(unsigned c=9;c<30;++c) originalOpl2Event(c,0xc0,d->bank[d->programs[c]][47],0);
		compare(*d,"bank/init/percussion programs"); return d;
	}
	static void event(OriginalOpl2MidiDriver& d,unsigned type,unsigned channel,unsigned a,unsigned b=0) {
		d.send(type|channel|(a<<8)|(b<<16));
		if(type==0xc0 && channel==9) return;
		if((type==0x90 || type==0x80) && channel==9) {
			if(a<88 && d.percussion_channel[a]) originalOpl2Event(d.percussion_channel[a]-1,type,d.percussion_key[a],b);
		} else originalOpl2Event(channel,type,a,b);
		compare(d,"MIDI event");
	}
public:
	static void run() {
		const bool had=is_system_path_defined("<STATIC>"); auto old=get_system_path("<STATIC>");
		add_system_path("<STATIC>","C:/GOG Games/Ultima 7/STATIC");
		auto data=scoreEntry(scoreBytes(get_system_path("<STATIC>/U7VOICE.FLX")),1);
		unsigned cases=0;
		{
			auto d=driver(data);
			for(unsigned p=0;p<128;++p) {
				event(*d,0xc0,0,p);
				for(unsigned velocity:{1u,32u,63u,64u,100u,127u}) {
					event(*d,0xb0,0,7,80); event(*d,0x90,0,60,velocity);
					for(unsigned bend:{0u,8192u,16383u}) { event(*d,0xe0,0,bend&127,bend>>7); ++cases; }
					for(unsigned tick=0;tick<4;++tick) { d->tick(); originalOpl2Tick(); compare(*d,"60Hz pitch automation"); }
					event(*d,0xb0,0,1,90); event(*d,0xb0,0,64,127); event(*d,0xb0,0,11,30);
					event(*d,0x80,0,60); event(*d,0xb0,0,121); event(*d,0xb0,0,123);
				}
			}
		}
		{
			auto d=driver(data); event(*d,0xc0,0,0);
			for(unsigned k=40;k<60;++k) event(*d,0x90,0,k,100);
			require(d->busy.size()==9 && d->free.empty(),"original nine-voice capacity");
			for(unsigned v=0;v<9;++v) require(d->voices[v].key==int(49+v+(v<2?9:0)),"original FIFO active stealing");
			event(*d,0xb0,0,123); require(d->busy.empty(),"original channel stop");
			event(*d,0x90,0,60,90); event(*d,0x90,0,60,90); event(*d,0x80,0,60);
			require(d->busy.size()==1,"original first matching physical occurrence release"); event(*d,0x80,0,60);
			event(*d,0xc0,0,36);
			for(unsigned k=50;k<60;++k) event(*d,0x90,0,k,100);
			require(d->busy.size()<=9,"secondary layers cannot expand original hardware");
			event(*d,0xb0,0,123);
			for(unsigned key=28;key<88;++key) { event(*d,0x90,9,key,100); event(*d,0x80,9,key); }
		}
		std::cout<<"PASS Original OPL2 executed DOS-driver register comparison: "<<cases<<" program/velocity/bend cases; envelopes/controllers/FIFO/stealing/partials/percussion/stop\n";
		{
			auto intro=scoreEntry(scoreBytes(get_system_path("<STATIC>/U7INTRO.TIM")),1);
			auto score=scoreEntry(scoreBytes(get_system_path("<STATIC>/INTROADM.DAT")),2);
			Configuration cfg; auto previous=config; config=&cfg;
			IBufferDataView input(score.data(),score.size()); XMidiFile file(&input,0,"OriginalOPL2",true);
			auto list=file.GetEventList(0); require(list,"original Guardian FM score"); auto d=driver(intro,true);
			ClockHandler h(true); unsigned triggers=0,mapped=0,remapped=0,ignored=0,ticks=0; std::set<int> programs;
			h.observe=[&](uint32 m,uint64) {
				while(ticks<h.now/100) { ++ticks; d->tick(); originalOpl2Tick(); compare(*d,"Guardian 60Hz automation"); }
				unsigned type=m&240,ch=m&15,a=(m>>8)&127,b=(m>>16)&127;
				if(type==0x90 && ch==9 && b) {
					++triggers; if(a<88 && d->percussion_channel[a]) { ++mapped; remapped+=d->percussion_key[a]!=a;
						programs.insert(d->bank[d->programs[d->percussion_channel[a]-1]][47]);
					} else ++ignored;
				}
				event(*d,type,ch,a,b);
			};
			list->incrementCounter(); { XMidiSequence sequence(&h,0,list,false,255,-1); pump(sequence,h,list->getLength()*50+30000); sequence.finish(); }
			require(triggers==401 && mapped==400 && remapped==376 && ignored==1,"original Guardian mapping counts");
			require(programs==std::set<int>{138,139,141,143},"original Guardian timbres");
			config=previous;
			std::cout<<"PASS Original OPL2 Guardian 401/400/376/1, four original timbres, executed reference-register comparison, no added notes\n";
		}
		{
			auto d=driver(data); auto bank=scoreBytes(get_system_path("<STATIC>/XMIDI.AD"));
			IBufferDataView input(bank.data(),bank.size()); d->loadTimbreLibrary(&input,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
			require(d->ready && d->ending,"ending AIL bank and TVFX");
			for(unsigned patch:{53u,54u,58u,60u,61u,62u,63u}) {
				d->endingSend(0xb1|(114<<8)|(2<<16)); d->endingSend(0xc1|(patch<<8)); d->endingSend(0x91|(60<<8)|(100<<16));
				require(std::any_of(d->slots.begin(),d->slots.end(),[&](auto& s){return s.patch==int(512+patch) && s.status && s.voice>=0;}),"ending source TVFX acquires hardware voice");
				for(unsigned tick=0;tick<180;++tick) d->endingTick(); d->endingSend(0x81|(60<<8));
				for(unsigned tick=0;tick<300;++tick) d->endingTick();
			}
			auto score=scoreBytes(get_system_path("<STATIC>/ENDSCORE.XMI"));
			Configuration memory; auto previous=config; config=&memory;
			for(unsigned sequence:{3u,4u}) {
				IBufferDataView music(score.data(),score.size()); XMidiFile file(&music,0,"OriginalOPL2",true);
				auto list=file.GetEventList(sequence); require(list,"ending FM sequence routing");
				IBufferDataView reload(bank.data(),bank.size()); d->loadTimbreLibrary(&reload,MidiDriver::TIMBRE_LIBRARY_XMIDI_AD);
				ClockHandler h(true); unsigned ticks=0,notes=0,voiced=0;
				h.observe=[&](uint32 m,uint64) {
					while(ticks<h.now/100) { ++ticks; d->endingTick(); }
					d->send(m);
					if((m&240)==144 && (m>>16)) {
						++notes;
						if(std::any_of(d->slots.begin(),d->slots.end(),[&](auto& s){return s.status && s.voice>=0 && s.channel==(m&15) && s.key==((m>>8)&127);})) ++voiced;
					}
					unsigned physical=std::count_if(d->owners.begin(),d->owners.end(),[](int s){return s>=0;});
					require(physical<=9,"ending nine physical voices");
				};
				list->incrementCounter(); { XMidiSequence playback(&h,0,list,false,255,-1); pump(playback,h,list->getLength()*50+30000); playback.finish(); }
				require(notes && voiced,"ending source score produces voiced notes");
				std::cout<<"PASS Original OPL2 ending FM sequence="<<sequence<<" notes="<<notes<<" voiced="<<voiced<<" (original hardware may drop notes)\n";
			}
			config=previous;
			d->reset(); require(std::all_of(d->owners.begin(),d->owners.end(),[](int n){return n<0;}),"ending reset ownership");
			d->destroyMidiDriver(); require(!d->chip,"original chip shutdown");
			std::cout<<"PASS Original OPL2 ending TVFX original bank loading/voices/timing/reset/shutdown (DOS AIL trace comparison pending)\n";
		}
		{
			auto a=driver(data),b=driver(data); a->send(0xc0); b->send(0xc0);
			a->send(0x90|(60<<8)|(100<<16)); b->send(0x90|(60<<8)|(100<<16));
			std::vector<sint16> whole(9600),pieces(9600);
			a->lowLevelProduceSamples(whole.data(),4800);
			for(unsigned at=0;at<4800;) { unsigned n=std::min(13u,4800-at); b->lowLevelProduceSamples(pieces.data()+at*2,n); at+=n; }
			require(whole==pieces,"original 60Hz automation render partition continuity");
			require(std::any_of(whole.begin(),whole.end(),[](auto v){return v!=0;}),"original OPL2 produces PCM");
			for(unsigned i=0;i<4800;++i) require(whole[i*2]==whole[i*2+1],"original OPL2 mono output");
			static_cast<MidiDriver&>(*a).setGlobalVolume(0); a->yield(); a->lowLevelProduceSamples(whole.data(),4800);
			require(std::all_of(whole.begin(),whole.end(),[](auto v){return v==0;}),"original OPL2 master mute");
			std::cout<<"PASS Original OPL2 PCM continuity/mono/master mute\n";
		}
		{
			Configuration cfg,loaded;
			cfg.read_config_string("<config><audio><midi><driver>OriginalOPL2</driver><volume_OriginalOPL2>65</volume_OriginalOPL2><use_oggs>no</use_oggs></midi></audio></config>");
			require(loaded.read_config_string(cfg.dump()),"CFG serialization roundtrip");
			std::string name; int volume=0; loaded.value("config/audio/midi/driver",name,"");
			loaded.value("config/audio/midi/volume_OriginalOPL2",volume,0);
			require(name=="OriginalOPL2" && volume==65,"CFG driver and independent volume persistence");
			std::set<std::string> available; for(int i=0;i<MidiDriver::getDriverCount();++i) available.insert(MidiDriver::getDriverName(i));
			require(available.count(name) && available.count("FMOpl") && available.count("OPL3Stereo"),"menu/CFG driver registry preserves existing choices");
			std::cout<<"PASS Original OPL2 CFG roundtrip/volume/menu driver registry\n";
		}
		if(had) add_system_path("<STATIC>",old); else clear_system_path("<STATIC>");
	}
};
