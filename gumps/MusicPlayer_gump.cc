// GPL-2.0-or-later. No audio preferences are written by this player.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "MusicPlayer_gump.h"
#include "Audio.h"
#include "AudioOptions_gump.h"
#include "Flex.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "Text_button.h"
#include "font.h"
#include "fnames.h"
#include "game.h"
#include "gamewin.h"
#include "mouse.h"
#include "utils.h"
#include <set>

using MusicButton=CallbackTextButton<MusicPlayer_gump>;

MusicPlayer_gump::MusicPlayer_gump(bool can_switch_game)
    : Modal_gump(nullptr,-1),can_switch(can_switch_game) {
    std::set<int> available;
    // Use original numeric IDs; empty entries and playback stop codes are not tracks.
    for(const char* path:{MAINMUS,MAINMUS_AD,"<PATCH>/mt32mus.dat","<PATCH>/adlibmus.dat"}) {
        try {
            if(!U7exists(path) || !Flex::is_flex(std::string(path))) continue;
            FlexFile bank{File_spec(path)};
            for(unsigned id=0;id<bank.number_of_objects() && id<255;++id) {
                if(id==0 && Game::get_game_type()==BLACK_GATE) continue;
                size_t length=0;
                bank.get_entry_info(id,length);
                if(length) available.insert(id);
            }
        } catch(const std::exception&) {
            // A missing backend-specific bank need not hide another usable bank.
        }
    }
    tracks.assign(available.begin(),available.end());
    if(tracks.empty()) status="No music tracks found.";
    SetProceduralBackground(TileRect(0,0,250,142));
    const char* game=Game::get_game_type()==BLACK_GATE ? "Game: Black Gate" : "Game: Serpent Isle";
    buttons[0]=std::make_unique<MusicButton>(this,&MusicPlayer_gump::change_game,game,40,25,170);
    buttons[1]=std::make_unique<MusicButton>(this,&MusicPlayer_gump::previous,"Previous",8,78,74);
    buttons[2]=std::make_unique<MusicButton>(this,&MusicPlayer_gump::play,"Play",88,78,74);
    buttons[3]=std::make_unique<MusicButton>(this,&MusicPlayer_gump::next,"Next",168,78,74);
    buttons[4]=std::make_unique<MusicButton>(this,&MusicPlayer_gump::stop,"Stop",88,96,74);
    buttons[5]=std::make_unique<MusicButton>(this,&MusicPlayer_gump::audio_options,"Audio Options",12,120,104);
    buttons[6]=std::make_unique<MusicButton>(this,&MusicPlayer_gump::close,"Back",134,120,104);
}

MusicPlayer_gump::~MusicPlayer_gump() { stop(); }

void MusicPlayer_gump::play() {
    if(tracks.empty()) return;
    auto* audio=Audio::get_ptr();
    if(!audio->is_audio_enabled() || !audio->is_music_enabled()) {
        status="Music is disabled in Audio Options.";
        return;
    }
    // Always stop first, so replay also works after an Audio Options change.
    audio->stop_music();
    try {
        audition_owned=true;
        auditioning=audio->start_music(tracks[selected],false,MyMidiPlayer::Force_None,MAINMUS);
        status=auditioning ? "Playing track "+std::to_string(tracks[selected]) : "Unavailable for this audio setting.";
    } catch(const std::exception&) {
        auditioning=false;
        status="Unable to load the selected track.";
    }
}

void MusicPlayer_gump::stop() {
    if(audition_owned) Audio::get_ptr()->stop_music();
    audition_owned=false;
    auditioning=false;
    status="Stopped.";
}

void MusicPlayer_gump::step(int delta) {
    if(tracks.empty()) return;
    stop();
    selected=delta<0 ? (selected+tracks.size()-1)%tracks.size() : (selected+1)%tracks.size();
    play();
}
void MusicPlayer_gump::previous() { step(-1); }
void MusicPlayer_gump::next() { step(1); }
void MusicPlayer_gump::audio_options() {
    stop();
    AudioOptions_gump options;
    gumpman->do_modal_gump(&options,Mouse::hand);
    status="Press Play to audition these settings.";
}
void MusicPlayer_gump::change_game() {
    if(!can_switch) { status="The other game is not configured."; return; }
    stop();
    switch_requested=true;
    done=true;
}
void MusicPlayer_gump::close() { stop(); done=true; }

void MusicPlayer_gump::paint() {
    Modal_gump::paint();
    auto* buffer=gwin->get_win()->get_ib8();
    font->center_text(buffer,x+125,y+7,"Music Player");
    const std::string track=tracks.empty() ? "Track: --" : "Track ID: "+std::to_string(tracks[selected]);
    font->center_text(buffer,x+125,y+48,track.c_str());
    font->center_text(buffer,x+125,y+63,status.c_str());
    for(auto& button:buttons) button->paint();
    gwin->set_painted();
}

Gump_button* MusicPlayer_gump::on_button(int mx,int my) {
    for(auto& button:buttons) if(auto* found=button->on_button(mx,my)) return found;
    return Modal_gump::on_button(mx,my);
}
bool MusicPlayer_gump::key_down(SDL_Keycode key,SDL_Keycode unicode) {
    if(key==SDLK_LEFT) previous();
    else if(key==SDLK_RIGHT) next();
    else if(key==SDLK_RETURN || key==SDLK_SPACE) play();
    else if(key==SDLK_S) stop();
    else return Modal_gump::key_down(key,unicode);
    return true;
}
bool MusicPlayer_gump::run() {
    bool redraw=Modal_gump::run();
    if(auditioning) {
        auto* midi=Audio::get_ptr()->get_midi();
        if(!midi || !midi->is_track_playing(tracks[selected])) {
            auditioning=false;
            status="Finished. Press Play to replay.";
            redraw=true;
        }
    }
    return redraw;
}
