// Exult music positioning metadata transfer. GPL-2.0-or-later.
// Independently implemented against the modern port's correspondence rules.
#include "pent_include.h"
#include "Opl3MusicPan.h"
#include "XMidiEventList.h"
#include <array>
#include <algorithm>
#include <map>
#include <set>
#include <limits>
#include <tuple>
#include <vector>

namespace {
using Note=std::tuple<int,int,int,int>; // Time, pitch, velocity, duration.
struct Channel {
    std::vector<Note> notes;
    std::vector<unsigned> pan;
    std::vector<std::tuple<int,int,int>> controls;
    std::set<int> programs, banks;
    bool indirect=false, explicit_pan=false, unprogrammed=false;
};
struct Score {
    std::array<Channel,16> channels;
    std::vector<std::vector<int>> structure;
    bool loops=false;
};
Score describe(XMidiEventList* list) {
    Score score;
    std::array<unsigned,16> pan;
    pan.fill(64);
    for(auto* e=list->events;e;e=e->next) {
        auto& channel=score.channels[e->getChannel()];
        if(e->getStatusType()==MidiStatus::NoteOn && e->data[1]) {
            if (channel.programs.empty()) channel.unprogrammed=true;
            channel.notes.emplace_back(e->time,e->data[0],e->data[1],e->ex.note_on.duration);
            channel.pan.push_back(pan[e->getChannel()]);
        } else if(e->getStatusType()==MidiStatus::Controller) {
            unsigned control=e->data[0];
            if(control==10) { pan[e->getChannel()]=e->data[1]; channel.explicit_pan=true; }
            if(control==121) pan[e->getChannel()]=64;
            if(control==115) channel.indirect=true;
            if(control==0 || control==32 || control==114) channel.banks.insert((control<<8)|e->data[1]);
            if(control>=116 && control<=120) score.loops=true;
            if(control==64 || control==121) channel.controls.emplace_back(e->time,control,e->data[1]);
            if(control==116 || control==117 || control==118 || control==120)
                score.structure.push_back({e->time,e->status,e->data[0],e->data[1]});
        } else if(e->getStatusType()==MidiStatus::Program) {
            channel.programs.insert(e->data[0]);
        } else if(e->getStatusType()==MidiStatus::PitchWheel) {
            channel.controls.emplace_back(e->time,256,e->data[0]|(e->data[1]<<7));
        } else if(e->status==0xff && e->data[0]==0x51) {
            std::vector<int> tempo{e->time,0x51};
            for(auto byte:e->ex.sysex_data.as_span()) tempo.push_back(byte);
            score.structure.push_back(std::move(tempo));
        }
    }
    return score;
}
}

unsigned transferOpl3MusicPan(XMidiEventList* playback,XMidiEventList* fm,XMidiEventList* roland) {
    if(!playback || !fm || !roland) return 0;
    const auto left=describe(fm),right=describe(roland);
    if(left.structure!=right.structure) return 0;
    std::array<bool,16> eligible{};
    for(unsigned c=0;c<16;++c) {
        eligible[c]=c!=9 && right.channels[c].explicit_pan && !left.channels[c].notes.empty()
            && !left.channels[c].indirect && !right.channels[c].indirect
            && left.channels[c].notes==right.channels[c].notes
            && left.channels[c].controls==right.channels[c].controls;
    }
    unsigned changed=0;
    std::array<size_t,16> note{};
    for(auto** link=&playback->events;*link;) {
        auto* e=*link;
        unsigned c=e->getChannel();
        if(eligible[c] && e->getStatusType()==MidiStatus::NoteOn && e->data[1]) {
            auto* pan=XMidiEvent::Create();
            pan->time=e->time;
            pan->status=uint8(0xb0|c);
            pan->data[0]=10;
            pan->data[1]=uint8(right.channels[c].pan[note[c]++]);
            pan->next=e;
            *link=pan;
            ++changed;
        }
        link=&e->next;
    }
    return changed;
}

// Recover only source-proven omissions on an independently aligned channel.
// Existing FM notes (including their different gate lengths) are never replaced.
unsigned recoverOpl3MusicNotes(XMidiEventList* playback, XMidiEventList* fm, XMidiEventList* roland) {
    if (!playback || !fm || !roland) return 0;
    const auto left=describe(fm), right=describe(roland), played=describe(playback);
    if (left.loops || right.loops || left.structure!=right.structure) return 0;
    int offset=0;
    bool aligned=false;
    for (unsigned c=0;c<16;++c) if (!left.channels[c].notes.empty()) {
        if (played.channels[c].notes.empty()) return 0;
        offset=std::get<0>(played.channels[c].notes.front())-std::get<0>(left.channels[c].notes.front());
        aligned=true; break;
    }
    if (!aligned || offset<0) return 0;
    // Verify the real playback list has only the parser's initial setup offset.
    // Do not guess a mapping for a rearranged or already recovered event list.
    for (unsigned c=0;c<16;++c) {
        const auto& source=left.channels[c].notes;
        const auto& actual=played.channels[c].notes;
        if (source.size()!=actual.size()) return 0;
        for (size_t i=0;i<source.size();++i) {
            auto expected=source[i]; std::get<0>(expected)+=offset;
            if (expected!=actual[i]) return 0;
        }
    }
    using Anchor=std::tuple<int,int,int>; // Normalized onset, pitch, velocity; not gate length.
    auto anchor=[](const Note& n) { return Anchor{std::get<0>(n),std::get<1>(n),std::get<2>(n)}; };
    std::array<std::map<Anchor,unsigned>,16> remaining;
    std::array<bool,16> eligible{};
    std::array<int,16> first{},last{};
    for (unsigned c=0;c<16;++c) {
        const auto& a=left.channels[c]; const auto& b=right.channels[c];
        if (c==9 || a.notes.empty() || a.indirect || b.indirect || a.unprogrammed || b.unprogrammed || a.programs.size()!=1
            || a.programs!=b.programs || a.banks!=b.banks || a.controls!=b.controls) continue;
        std::map<Anchor,unsigned> needed;
        std::set<int> onsets;
        first[c]=std::numeric_limits<int>::max(); last[c]=0;
        for (const auto& n:a.notes) {
            ++needed[anchor(n)]; onsets.insert(std::get<0>(n));
            first[c]=std::min(first[c],std::get<0>(n));
            last[c]=std::max(last[c],std::get<0>(n)+std::get<3>(n));
        }
        if (onsets.size()<2) continue; // Need a phrase span that can bracket recovered material.
        for (const auto& n:b.notes) ++remaining[c][anchor(n)];
        bool subset=true;
        for (auto [key,count]:needed) {
            auto& available=remaining[c][key];
            if (available<count) { subset=false; break; }
            available-=count;
        }
        // Same-onset unison multiplicities need a separate identity correspondence.
        // Refuse that ambiguity rather than guessing which duration is the omitted note.
        for (const auto& n:b.notes) if (needed[anchor(n)]>1) subset=false;
        std::map<Anchor,unsigned> multiplicity;
        for (const auto& n:b.notes) if (++multiplicity[anchor(n)]>1) subset=false;
        eligible[c]=subset;
    }
    unsigned recovered=0;
    std::array<size_t,16> noteIndex{};
    for (auto* source=roland->events;source;source=source->next) {
        const unsigned c=source->getChannel();
        if (source->getStatusType()!=MidiStatus::NoteOn || !source->data[1]) continue;
        const size_t index=noteIndex[c]++;
        if (!eligible[c]) continue;
        const auto& n=right.channels[c].notes[index];
        auto& count=remaining[c][anchor(n)];
        if (!count) continue;
        --count;
        const int duration=source->ex.note_on.duration;
        if (duration<=0 || source->time<first[c] || source->time>last[c]-duration) continue;
        const int time=source->time+offset;
        unsigned pan=64;
        auto** at=&playback->events;
        // Retain every original event's ordering, including same-tick setup and notes.
        while (*at && (*at)->time<=time) {
            auto* e=*at;
            if (e->getChannel()==c && e->getStatusType()==MidiStatus::Controller) {
                if (e->data[0]==10) pan=e->data[1];
                if (e->data[0]==121) pan=64;
            }
            at=&e->next;
        }
        auto* note=XMidiEvent::Create();
        note->time=time; note->status=source->status;
        note->data[0]=source->data[0]; note->data[1]=source->data[1];
        note->ex.note_on.duration=duration;
        note->next=*at; *at=note;
        // A temporary pan update is captured by this note only; restore the FM channel immediately.
        const unsigned desired=right.channels[c].explicit_pan ? right.channels[c].pan[index] : pan;
        if (desired!=pan) {
            auto* before=XMidiEvent::Create(); auto* after=XMidiEvent::Create();
            before->time=after->time=time; before->status=after->status=uint8(0xb0|c);
            before->data[0]=after->data[0]=10;
            before->data[1]=uint8(desired); after->data[1]=uint8(pan);
            before->next=note; *at=before; after->next=note->next; note->next=after;
        }
        playback->chan_mask|=uint16(1u<<c);
        playback->setLength(std::max(playback->getLength(),uint32(time+duration)));
        ++recovered;
    }
    return recovered;
}
