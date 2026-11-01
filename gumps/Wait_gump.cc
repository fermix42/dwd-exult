// GPL-2.0-or-later.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "Wait_gump.h"
#include "Gump_button.h"
#include "misc_buttons.h"
#include "font.h"
#include "game.h"
#include "gameclk.h"
#include "gamewin.h"
#include "imagewin.h"
#include <cstdio>
#include <algorithm>
namespace {
std::string clock_text(int minutes) {
    const int h = (minutes / 60) % 24, m = minutes % 60;
    char text[32]; std::snprintf(text, sizeof(text), "%d:%02d %s", h % 12 ? h % 12 : 12, m, h < 12 ? "am" : "pm");
    return text;
}
class Wait_check : public Checkmark_button {
public:
    Wait_check(Gump* par) : Checkmark_button(par, 104, 142) {}
    const char* cli_action() const override { return "confirm"; }
};
class Wait_button : public Gump_button {
    int step;
public:
    Wait_button(Wait_gump* parent, int shape, int px, int py, int step)
        : Gump_button(parent, shape, px, py), step(step) {}
    const char* cli_action() const override { return step < 0 ? "decrease" : step > 0 ? "increase" : "cancel"; }
    bool activate(MouseButton button) override {
        if (button != MouseButton::Left) return false;
        auto* wait = static_cast<Wait_gump*>(parent);
        if (step) wait->adjust(step); else wait->cancel();
        return true;
    }
};
}
Wait_gump::Wait_gump() : Modal_gump(nullptr, game->get_shape("gumps/scroll")),
    initial_minutes(gwin->get_clock()->get_total_minutes()) {
    // Original game-specific parchment and original controls; no new artwork.
    add_elem(new Wait_check(this));
    add_elem(new Wait_button(this, game->get_shape("gumps/nobtn"), 149, 141, 0));
    add_elem(new Wait_button(this, game->get_shape("gumps/slider_left"), 60, 82, -1));
    add_elem(new Wait_button(this, game->get_shape("gumps/slider_right"), 182, 82, 1));
}
void Wait_gump::adjust(int delta) {
    hours = std::max(1, std::min(24, hours + delta));
    gwin->add_dirty(get_rect());
}
std::string Wait_gump::result_time() const {
    return clock_text(end_time()) + (end_time() / 1440 > initial_minutes / 1440 ? " (+1 day)" : "");
}
void Wait_gump::paint() {
    paint_shape(x, y); paint_elems();
    auto* ib = gwin->get_win()->get_ib8();
    const auto header = "Current time: " + clock_text(initial_minutes);
    const auto selected = std::to_string(hours) + (hours == 1 ? " hour" : " hours");
    const auto until = "Until " + result_time();
    font->paint_text_box(ib, "Wait", x + 51, y + 32, 142, 12, 0, false, true);
    font->paint_text_box(ib, header.c_str(), x + 51, y + 53, 142, 14, 0, false, true);
    font->paint_text_box(ib, selected.c_str(), x + 79, y + 79, 88, 14, 0, false, true);
    font->paint_text_box(ib, until.c_str(), x + 51, y + 104, 142, 24, 0, false, true);
    gwin->set_painted();
}
bool Wait_gump::key_down(SDL_Keycode key, SDL_Keycode unicode) {
    switch (key) {
    case SDLK_LEFT: case SDLK_DOWN: adjust(-1); return true;
    case SDLK_RIGHT: case SDLK_UP: adjust(1); return true;
    case SDLK_HOME: adjust(-24); return true;
    case SDLK_END: adjust(24); return true;
    case SDLK_RETURN: close(); return true;
    case SDLK_ESCAPE: return false; // Native modal cancellation; never confirms.
    default: return Modal_gump::key_down(key, unicode);
    }
}
