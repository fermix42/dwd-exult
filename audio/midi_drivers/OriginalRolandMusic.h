/* Copyright (C) 2026 The Exult Team
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Roland-only original-driver policy; no music sequencer or ROM data.
 * Instrument and initialization bytes are supplied from validated user assets.
 */
#ifndef ORIGINAL_ROLAND_MUSIC_H
#define ORIGINAL_ROLAND_MUSIC_H
#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <vector>
#include <stdexcept>
class OriginalRolandMusic {
public:
    struct Command { std::vector<uint8> bytes; unsigned retraces=0; };
    std::deque<Command> output;
    bool classic=false;
    unsigned relativeVolume=100;
    std::map<unsigned,std::array<uint8,246>> timbres;
private:
    struct Slot { int key=-1; uint64 age=0; bool protect=false; };
    std::array<Slot,64> slots{};
    std::array<int,16> programs{},channelSlots{};
    std::array<unsigned,16> banks{};
    std::array<unsigned,128> patchBanks{};
    std::array<std::array<uint8,32>,3> queues{};
    std::array<unsigned,3> address{},length{};
    uint64 age=0;
    int find(unsigned key) const {for(unsigned i=0;i<slots.size();++i)if(slots[i].key==int(key))return int(i);return -1;}
    void patch(unsigned program,unsigned offset,const std::vector<uint8>& data,unsigned wait=2) { sysex(0x14000+program*8+offset,data,wait); }
    void assign(unsigned program,unsigned bank) {
        patchBanks[program]=bank;
        int index=bank?find((bank<<8)|program):-1;
        patch(program,0,{uint8(index<0?(program<64?0:1):2),uint8(index<0?(program&63):index)});
    }
public:
    explicit OriginalRolandMusic(bool isClassic):classic(isClassic) {programs.fill(-1);channelSlots.fill(-1);}
    // Addresses are packed as a flat 21-bit Roland address, never binary MIDI bytes.
    void sysex(unsigned at,const std::vector<uint8>& data,unsigned wait=0) {
        if(data.size()>256)throw std::runtime_error("Roland data-set too large");
        Command c{{0xf0,0x41,0x10,0x16,0x12,uint8((at>>14)&127),uint8((at>>7)&127),uint8(at&127)},wait};
        unsigned sum=c.bytes[5]+c.bytes[6]+c.bytes[7];
        for(auto b:data){if(b>127)throw std::runtime_error("Invalid Roland data byte");sum+=b;c.bytes.push_back(b);}
        c.bytes.push_back(uint8((-int(sum))&127));c.bytes.push_back(0xf7);output.push_back(std::move(c));
    }
    void raw(uint32 message) {
        unsigned type=message&240;
        Command c{{uint8(message),uint8((message>>8)&127)},0};
        if(type!=0xc0 && type!=0xd0)c.bytes.push_back(uint8((message>>16)&127));
        output.push_back(std::move(c));
    }
    void install(unsigned bank,unsigned program) {
        if(bank==127 || program>=128)return;
        unsigned key=(bank<<8)|program;
        if(bank && find(key)<0) {
            auto data=timbres.find(key);if(data==timbres.end())return;
            int index=-1;
            for(unsigned i=0;i<slots.size();++i)if(slots[i].key<0){index=int(i);break;}
            if(index<0){uint64 oldest=~uint64(0);for(unsigned i=0;i<slots.size();++i)if(!slots[i].protect && slots[i].age<=oldest){oldest=slots[i].age;index=int(i);}}
            if(index<0)return;
            slots[index]={int(key),age++,false};
            // AIL transmits the common block followed by four partials.
            const unsigned offsets[]={0,14,72,130,188,246};
            for(unsigned i=0;i<5;++i)sysex(0x20000+unsigned(index)*256+offsets[i],std::vector<uint8>(data->second.begin()+offsets[i],data->second.begin()+offsets[i+1]),3);
        }
        assign(program,bank);
    }
    void initializeClassic(const std::vector<uint8>& bank) {
        if(bank.empty() || bank.size()!=1+unsigned(bank[0])*247)throw std::runtime_error("Invalid classic Roland timbre bank");
        sysex(0x1fc07f,{0});
        for(unsigned i=0;i<bank[0];++i){
            unsigned at=1+i*247,program=(bank[at+246]-1)&127,slot=i&63;
            sysex(0x20000+slot*256,std::vector<uint8>(bank.begin()+at,bank.begin()+at+246));
            // Existing Exult classic patch defaults match the original bank format.
            sysex(0x14000+program*8,{2,uint8(slot),24,50,24,0,1,0});
        }
        sysex(0x40016,{70});
    }
    void initializeAil(const std::vector<uint8>& driver,bool newer) {
        unsigned shift=newer?13:0,controls=newer?4567:4549,defaults=newer?4576:4558,patches=newer?4585:4567;
        if(driver.size()<=patches+9)throw std::runtime_error("Invalid AIL Roland initialization data");
        sysex(0x1fc000,{1},12);
        sysex(0x4000d,std::vector<uint8>(driver.begin()+286+shift,driver.begin()+295+shift),4);
        sysex(0x40004,std::vector<uint8>(driver.begin()+295+shift,driver.begin()+304+shift),4);
        sysex(0x40001,std::vector<uint8>(driver.begin()+283+shift,driver.begin()+286+shift),4);
        for(unsigned i=0;i<9;++i)for(unsigned chan=1;chan<=9;++chan)event(0xb0|chan|(unsigned(driver[controls+i])<<8)|(unsigned(driver[defaults+i])<<16));
        if(!output.empty())output.back().retraces+=10;
        for(unsigned chan=1;chan<=9;++chan){event(0xe0|chan|(64<<16));if(driver[patches+chan-1]<128)event(0xc0|chan|(unsigned(driver[patches+chan-1])<<8));}
        if(!output.empty())output.back().retraces+=10;
    }
    void loadAilBank(const std::vector<uint8>& bank) {
        for(unsigned at=0;at+6<=bank.size();at+=6){
            unsigned program=bank[at],group=bank[at+1];if(program==255 || group==255)break;
            unsigned off=unsigned(bank[at+2])|(unsigned(bank[at+3])<<8)|(unsigned(bank[at+4])<<16)|(unsigned(bank[at+5])<<24);
            if(group>127 || program>127 || off>bank.size() || bank.size()-off<248 || (bank[off]|(unsigned(bank[off+1])<<8))!=248)throw std::runtime_error("Invalid AIL Roland timbre entry");
            std::copy_n(bank.begin()+off+2,246,timbres[(group<<8)|program].begin());
        }
    }
    void event(uint32 message) {
        unsigned type=message&240,chan=message&15,a=(message>>8)&127,b=(message>>16)&127;
        if(classic){if(type==0x80)message=0x90|chan|(a<<8);raw(message);return;}
        if(type==0xb0) {
            if(a>=32 && a<=46){
                unsigned q=(a-32)/5,op=(a-32)%5;
                if(op<3){unsigned bits=(2-op)*7;address[q]=(address[q]&~(127u<<bits))|(b<<bits);return;}
                queues[q][length[q]]=uint8(b);
                if(op==3 && length[q]<31){++length[q];return;}
                unsigned count=length[q]+1;
                sysex(address[q],std::vector<uint8>(queues[q].begin(),queues[q].begin()+count),op==3?4:0);
                address[q]+=count-(op==4);length[q]=0;return;
            }
            if(a==59 || a==60){if(programs[chan]>=0){patch(unsigned(programs[chan]),a==59?6:4,{uint8(b)});raw(0xc0|chan|(unsigned(programs[chan])<<8));}return;}
            if(a>=61 && a<=63){sysex(0x40000+a-60,{uint8(b)});return;}
            if(a==114){banks[chan]=b;return;}
            if(a==58){if(channelSlots[chan]>=0 && b>=24)sysex(0xc090+(b-24)*4,{uint8(channelSlots[chan])},4);return;}
            if(a==113){if(channelSlots[chan]>=0)slots[channelSlots[chan]].protect=b>=64;return;}
            if(a>=110 && a<=120)return;
            if(a==7)message=(message&65535)|(std::min(127u,b*relativeVolume/100)<<16);
        } else if(type==0xc0){
            programs[chan]=int(a);
            if(banks[chan]!=patchBanks[a])assign(a,banks[chan]);
            channelSlots[chan]=find((banks[chan]<<8)|a);
        } else if(type==0x90){++age;if(channelSlots[chan]>=0)slots[channelSlots[chan]].age=age;}
        raw(message);
    }
};
#endif
