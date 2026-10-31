// Source-backed Guardian dispatch and recovery checks. Original mapping remains user-provided.
void Opl3VoiceRegression::guardian() {
 Configuration memory;memory.read_config_string("<config><audio><midi><volume_curve>1</volume_curve></midi></audio></config>");
 struct Restore {Configuration* old=config;~Restore(){config=old;}} restore;config=&memory;
 const bool had=is_system_path_defined("<STATIC>");auto old=get_system_path("<STATIC>");
 const char* overrideDir=std::getenv("EXULT_OPL3_BG_STATIC");std::string dir=overrideDir?overrideDir:"C:/GOG Games/Ultima 7/STATIC";
 add_system_path("<STATIC>",dir);
 auto bankFile=scoreBytes(dir+"/U7INTRO.TIM"),bankBytes=scoreEntry(bankFile,1);
 auto exe=scoreBytes(dir+"/../INTRO.EXE"),fmBank=scoreBytes(dir+"/INTROADM.DAT"),mtBank=scoreBytes(dir+"/INTRORDM.DAT");
 auto f=scoreEntry(fmBank,2),m=scoreEntry(mtBank,2);
 IBufferDataView fs(f.data(),f.size()),ms(m.data(),m.size()),ps(f.data(),f.size());
 XMidiFile fm(&fs,0,"OPL3Stereo",true),mt(&ms,0,"OPL3Stereo",true),play(&ps,0,"OPL3Stereo");
 auto* fl=fm.GetEventList(0);auto* ml=mt.GetEventList(0);auto* list=play.GetEventList(0);
 auto before=scoreNotes(list);std::map<XMidiEvent*,std::vector<int>> saved;
 for(auto*e=list->events;e;e=e->next){saved[e]={e->time,e->status,e->data[0],e->data[1]};if(e->getStatusType()==MidiStatus::NoteOn&&e->data[1])saved[e].push_back(e->ex.note_on.duration);}
 require(recoverOpl3GuardianNotes(list,fl,ml)==10,"Guardian exact recovery");
 unsigned added=0;for(auto*e=list->events;e;e=e->next){if(saved.count(e)){std::vector<int> sig{e->time,e->status,e->data[0],e->data[1]};if(e->getStatusType()==MidiStatus::NoteOn&&e->data[1])sig.push_back(e->ex.note_on.duration);require(saved[e]==sig,"Guardian original event unchanged");}else{require(e->getStatusType()==MidiStatus::NoteOn&&e->data[1]&&e->getChannel()==4,"Guardian only channel5 additions");++added;}}
 require(added==10,"Guardian inserted events");auto recovered=scoreNotes(list);require(recoverOpl3GuardianNotes(list,fl,ml)==0&&scoreNotes(list)==recovered,"Guardian no duplicate recovery");
 auto d=driver();d->setOriginalIntro(true);IBufferDataView bank(bankBytes.data(),bankBytes.size());d->loadTimbreLibrary(&bank,MidiDriver::TIMBRE_LIBRARY_U7VOICE_AD);
 ClockHandler h(true);unsigned trigger=0,mapped=0,ignored=0,remapped=0,pan=0;std::set<int> timbres;std::map<uint64,int> balance;
 h.observe=[&](uint32 msg,uint64 id){int ch=msg&15,key=(msg>>8)&127,vel=(msg>>16)&127;bool on=(msg&240)==144&&vel;
 if(id)balance[id]+=on?1:-1;
 if(id)d->sendNote(msg,id);else d->send(msg);
 if(on&&ch==9){++trigger;int v=voice(*d,id),part=key<88?int(exe[111619+key]):0;
 if(!part){++ignored;require(v<0,"Guardian ignored trigger must not sound");}
 else{++mapped;require(v>=0,"Guardian mapped occurrence owns a voice");int target=exe[111707+key];remapped+=target!=key;require(d->chp[v].note==target,"Guardian mapped key");require(d->voice_timbre[v].bg[47]==d->intro_parts[part-10].bg[47],"Guardian mapped timbre");timbres.insert(d->voice_timbre[v].bg[47]);require(d->bg_voice[v].intro_part==part-10,"Guardian mapped part state");require(d->opl->core.channel[v].slotz[0]->key,"Guardian mapped register key-on");const auto& ins=d->voice_timbre[v];
 require(d->adlib_data[0x20+d->adlib_opadd[v]]==ins.mod_avekm&&d->adlib_data[0x23+d->adlib_opadd[v]]==ins.car_avekm,"Guardian operator multipliers/flags");
 require(d->adlib_data[0x60+d->adlib_opadd[v]]==ins.mod_ad&&d->adlib_data[0x63+d->adlib_opadd[v]]==ins.car_ad,"Guardian mapped envelopes");
 require((d->adlib_data[0xc0+d->channelRegister(v)]&15)==(ins.fb_c&15),"Guardian mapped feedback/connection");}}
 if(on&&ch==4)require(d->voice_timbre[voice(*d,id)].bg[47]==0,"Guardian program43 original first-bank fallback");
 if((msg&240)==176&&ch==3&&key==10){++pan;require(d->ch[3].pan==vel,"Guardian source pan reaches driver");}
 };
 list->incrementCounter();{XMidiSequence sequence(&h,0,list,false,255,-1);pump(sequence,h,list->getLength()*50+30000);sequence.finish();}
 require(trigger==401&&mapped==400&&ignored==1&&remapped==376,"Guardian exact percussion counts");
 require(timbres==std::set<int>{138,139,141,143},"Guardian original four timbres");require(pan>=1455,"Guardian pan events retained");
 for(auto[id,n]:balance)require(n==0,"Guardian independent note releases");require(active(*d)==0,"Guardian stop leaves no notes");require(!d->opl->core.rhy,"Guardian rhythm mode remains off");
 // Late releases and channel-10 controller propagation keep the shared identity policy.
 d->sendNote(on(75,9),9000);int v=voice(*d,9000);d->send(0xb9|(7<<8)|(32<<16));d->send(0xb9|(1<<8)|(80<<16));
 require(d->ch[9].vol==32&&d->ch[9].modulation==80,"Guardian mapped controllers");d->sendNote(off(75,9),9000);require(voice(*d,9000)<0,"Guardian trigger release maps back to occurrence");
 d->sendNote(on(75,9),9001);d->sendNote(off(75,9),9000);require(voice(*d,9001)>=0,"Guardian stale off preserves replacement");d->send(0xb9|(123<<8));require(active(*d)==0,"Guardian channel stop");
 if(had)add_system_path("<STATIC>",old);else clear_system_path("<STATIC>");
 std::cout<<"PASS Guardian triggers=401 mapped=400 ignored=1 remapped=376 timbres=138,139,141,143; recovery=10; original events/pan/identities/controllers/stop\n";
}
