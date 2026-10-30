// GPL-2.0-or-later. A launcher UI for the existing Exult music player.
#ifndef EXULT_MUSIC_PLAYER_GUMP_H
#define EXULT_MUSIC_PLAYER_GUMP_H
#include "Modal_gump.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
class Gump_button;

class MusicPlayer_gump final : public Modal_gump {
    std::array<std::unique_ptr<Gump_button>,7> buttons;
    std::vector<int> tracks;
    size_t selected=0;
    bool can_switch, switch_requested=false, auditioning=false, audition_owned=false;
    std::string status="Select a track.";
    void play();
    void stop();
    void previous();
    void next();
    void audio_options();
    void change_game();
    void step(int delta);
    Gump_button* on_button(int mx,int my) override;
    bool key_down(SDL_Keycode key,SDL_Keycode unicode) override;
    bool run() override;
public:
    explicit MusicPlayer_gump(bool can_switch_game);
    ~MusicPlayer_gump() override;
    void paint() override;
    void close() override;
    bool wants_other_game() const { return switch_requested; }
};
#endif
