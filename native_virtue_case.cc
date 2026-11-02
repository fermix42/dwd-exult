// GPL-2.0-or-later. Exult-owned physical Virtue Stone Case, production artwork.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "native_virtue_case.h"
#include "cli_driver.h"
#include "actors.h"
#include "spellbook.h"
#include "mouse.h"
#include "Audio.h"
#include "tqueue.h"
#include "ucmachine.h"
#include "ucsched.h"
#include "Gump.h"
#include "Gump_manager.h"
#include "Gump_button.h"
#include "misc_buttons.h"
#include "U7obj.h"
#include "databuf.h"
#include "fnames.h"
#include "data/exult_bg_flx.h"
#include "contain.h"
#include "font.h"
#include "font_map.h"
#include "game.h"
#include "gamewin.h"
#include "ibuf8.h"
#include "imagewin.h"
#include "objiter.h"
#include "shapevga.h"
#include "ready.h"
#include <array>
#include <memory>
#include <stdexcept>

namespace {
bool installed = false;
bool label_preview = false;
constexpr const char* label_examples[8][2] = {{"Britain","Home"},{"LB","Castle"},{"Destard","Ent"},{"Cove","Healer"},{"Trinsic","Gate"},{"Skara","Brae"},{"Shrine","Home"},{"Camp","Home"}};
constexpr int width = 196, height = 135;
// Original BG shape 330 frames: yellow/red/blue/orange/green/purple/black/white.
// Fixed columns read top to bottom: Honesty..Justice, Sacrifice..Humility.
constexpr int stone_frames[] = {2, 0, 1, 4, 3, 5, 7, 6};
int slot_for(const Game_object* object) {
    if (!object || !object->as_virtstone() || object->get_shapenum() != 330) return -1;
    for (int i = 0; i < 8; ++i) if (object->get_framenum() == stone_frames[i]) return i;
    return -1;
}
int slot_x(int slot) { return 44 + (slot / 4) * 71; }
int slot_y(int slot) { return 52 + (slot % 4) * 18; }

std::unique_ptr<Shape_file> load_art() {
    const U7object resource(BUNDLE_CHECK(BUNDLE_EXULT_BG_FLX, EXULT_BG_FLX), EXULT_BG_FLX_VIRTUE_CASE_SHP);
    std::size_t length;
    auto bytes = resource.retrieve(length);
    IBufferDataView source(bytes, length);
    auto art = std::make_unique<Shape_file>(&source);
    if (art->get_num_frames() != 14) throw std::runtime_error("Invalid Virtue Case art frame count");
    for (int i = 0; i < 14; ++i) {
        const auto* f = art->get_frame(i);
        const int w = i == 0 ? 28 : i == 1 ? width : i < 10 ? 13 : 32;
        const int h = i == 0 ? 20 : i == 1 ? height : i < 10 ? 15 : 12;
        const int ox = i == 0 ? 14 : i >= 2 && i < 10 ? 6 : 0;
        const int oy = i == 0 ? 19 : i >= 2 && i < 10 ? 7 : 0;
        if (!f || f->get_width() != w || f->get_height() != h || f->get_xleft() != ox || f->get_yabove() != oy)
            throw std::runtime_error("Invalid Virtue Case art geometry");
    }
    return art;
}

// A single scoped request; all references survive only through weak ownership.
// Original Usecode may defer the actual Mark operation to a stone script.
struct Cast_session : Time_sensitive {
    Game_object_weak owner, target;
    bool recall = false, marked = false, targeting = true;
    int slot = -1;
    void handle_event(unsigned long now, uintptr) override;
};
std::shared_ptr<Cast_session> cast_session;
void reset_cast_session() {
    if (!cast_session) return;
    Game_window::get_instance()->get_tqueue()->remove(cast_session.get());
    cast_session.reset();
}
void cast_from_case(Container_game_object* container, bool recall) {
    auto* gwin = Game_window::get_instance();
    if (Virtue_case::casting_active() || gwin->get_usecode()->in_usecode()
        || !gwin->main_actor_can_act() || gwin->main_actor_dont_move()) return;
    const int spell = recall ? 36 : 34; // Verified BG spellbook slots; effective Usecode0x664/0x662.
    auto* avatar = gwin->get_main_actor();
    Game_object_vector possessions;
    Actor* party[9];
    const int count = gwin->get_party(party, 1);
    for (int i=0; i<count; ++i) if (party[i]->can_act_charmed() && avatar->distance(party[i])<=8)
        party[i]->get_objects(possessions, c_any_shapenum, c_any_qual, c_any_framenum);
    Spellbook_object* book = nullptr;
    for (auto* item : possessions) if (auto* candidate = item->as_spellbook()) {
        if (candidate->can_do_spell(avatar, spell)) { book = candidate; break; }
    }
    if (!book) { Mouse::mouse()->flash_shape(Mouse::redx); return; }
    auto keep_book = book->shared_from_this();
    auto session = std::make_shared<Cast_session>();
    session->owner = weak_from_obj(container); session->recall = recall;
    cast_session = session;
    // The ordinary spellbook method applies costs once, begins casting and calls
    // the currently loaded Usecode (including Keyring's Mark override).
    const bool started = book->do_spell(avatar, spell);
    session->targeting = false;
    if (!started) { reset_cast_session(); return; }
    // Match Spellbook_gump: release inventory pausing after the spell has asked
    // for its target, so its original animation/effect scripts can run.
    gwin->get_gump_man()->close_all_gumps();
    gwin->get_tqueue()->add(Game::get_ticks()+100, session.get());
}
void Cast_session::handle_event(unsigned long now, uintptr) {
    auto* gwin = Game_window::get_instance();
    if (cast_session.get() != this) return;
    const auto keep_alive = cast_session;
    auto container = owner.lock(); auto stone = target.lock();
    auto* avatar = gwin->get_main_actor();
    if (!container || !Virtue_case::is_case(container.get())) { reset_cast_session(); return; }
    if (gwin->get_usecode()->in_usecode() || avatar->get_casting_mode()!=Actor::not_casting
        || Usecode_script::find(avatar) || (stone && Usecode_script::find(stone.get()))) {
        gwin->get_tqueue()->add(now+100, this); return;
    }
    // Stage C attaches the label editor here, after observed intrinsic success.
    reset_cast_session();
}

// Physical native controls retain the accepted normal/depressed artwork.
class Case_button : public Gump_button {
    Shape_file* art;
    int frame;
    const char* label;
public:
    Case_button(Gump* parent, Shape_file* art, int frame, int x, int y, const char* label)
        : Gump_button(parent, -1, x, y, SF_OTHER), art(art), frame(frame), label(label) {}
    TileRect get_rect() const override {
        int sx = 0, sy = 0; local_to_screen(sx, sy);
        return TileRect(sx, sy, 32, 12);
    }
    bool on_widget(int mx, int my) const override {
        screen_to_local(mx, my);
        return mx >= 0 && my >= 0 && mx < (32) && my < 12;
    }
    const char* cli_action() const override {
        return frame == 10 ? "mark" : "recall";
    }
    bool activate(MouseButton button) override {
        if (button != MouseButton::Left) return false;
        auto* container = parent->get_container();
        Audio::get_ptr()->play_sound_effect(Audio::game_sfx(74), AUDIO_MAX_VOLUME, 0, 0, 0, true);
        cast_from_case(container, frame==12);
        return true; // Usecode/Recall may have closed and deleted this control.

    }
    void paint() override {
        int sx = 0, sy = 0; local_to_screen(sx, sy);
        art->get_frame(frame + (is_pushed() ? 1 : 0))->paint(gwin->get_win()->get_ib8(), sx, sy);
        auto font = fontManager.get_font("TINY_BLACK_FONT");
        font->paint_text(gwin->get_win()->get_ib8(), label, sx + (32 - font->get_text_width(label))/2, sy + 3 + (is_pushed() ? 1 : 0));
    }
};

class Case_gump : public Gump {
    // Own all frames for as long as child controls borrow them.
    std::unique_ptr<Shape_file> art;
    Shape_frame* background;
    std::array<Shape_frame*, 8> seated{};
public:
    explicit Case_gump(Container_game_object* cont) : Gump(cont, 0, 0, -1), art(load_art()), background(art->get_frame(1)) {
        set_object_area(TileRect(0, 0, width, height));
        for (int i = 0; i < 8; ++i) seated[i] = art->get_frame(i+2);
        add_elem(new Case_button(this, art.get(), 10, 38, 14, "MARK"));
        add_elem(new Case_button(this, art.get(), 12, 130, 14, "RECALL"));
        // Original BG checkmark frame has bottom-right hotspot (20,20).
        add_elem(new Checkmark_button(this, 23, 68));
        set_pos();
    }
    Gump* clone(Container_game_object* cont, int initx, int inity) override {
        auto* result = new Case_gump(cont); result->set_pos(initx, inity); return result;
    }
    TileRect get_rect() const override { return TileRect(x, y, width, height); }
    bool has_point(int sx, int sy) const override {
        if (background->has_point(sx-x, sy-y)) return true;
        // The checkmark can extend over the transparent edge of the left leather side tab.
        for (auto* elem : elems) if (elem->on_widget(sx, sy)) return true;
        return false;
    }
    const char* get_click_name() const override { return "Virtue Stone Case"; }
    bool add(Game_object* obj, int, int, int, int, bool dont_check, bool) override {
        return obj && container->add(obj, dont_check, false);
    }
    void paint() override {
        background->paint(gwin->get_win()->get_ib8(), x, y);
        paint_elems();
        if (cli_enabled && label_preview) {
            auto font = fontManager.get_font("TINY_BLACK_FONT");
            for (int i=0; i<8; ++i) for (int line=0; line<2; ++line) {
                const char* text = label_examples[i][line];
                const int text_width = font->get_text_width(text);
                if (text_width <= 32)
                    font->paint_text(gwin->get_win()->get_ib8(), text,
                        x+61+71*(i/4)+(32-text_width)/2, y+45+18*(i%4)+6*line+4-font->get_text_baseline());
            }
        }
        Object_iterator next(container->get_objects());
        while (auto* object = next.get_next()) {
            const int slot = slot_for(object);
            if (slot >= 0 && seated[slot]) {
                Virtue_case::seat(container, object);
                seated[slot]->paint(gwin->get_win()->get_ib8(), x+slot_x(slot), y+slot_y(slot));
            }
        }
    }
    Game_object* find_object(int mx, int my) override {
        Object_iterator next(container->get_objects());
        while (auto* object = next.get_next()) {
            const int slot = slot_for(object);
            if (slot >= 0 && seated[slot] && seated[slot]->has_point(mx-x-slot_x(slot), my-y-slot_y(slot))) {
                // Same grab-anchor technique as Jawbone_gump::set_to_spot.
                // Dragging_info paints the original object, never this seated art.
                const auto* normal = object->get_shape();
                object->set_shape_pos(mx-x+normal->get_xleft()-normal->get_width()/2,
                    my-y+normal->get_yabove()-normal->get_height()/2);
                return object;
            }
        }
        return nullptr;
    }
};
}

void Virtue_case::install_shape(Shapes_vga_file& shapes) {
    installed = false;
    // Never claim a third-party shape. This mirrors the accepted Orb chart guard.
    if (!GAME_BG || (shape < shapes.get_num_shapes() && shapes.get_num_frames(shape))) return;
    auto* item = shapes.new_shape(shape);
    if (!item) return;
    auto art = load_art();
    item->add_frame(std::move(*art->begin()), 0);
    Shape_info info;
    info.set_shape_class(Shape_info::container);
    info.set_weight_volume(10, 3);
    info.set_ready_type(lhand);
    info.set_alt_ready(rhand, backpack);
    info.set_3d(1, 1, 1);
    shapes.set_info(shape, info);
    installed = true;
}
bool Virtue_case::is_case(const Game_object* object) {
    return installed && GAME_BG && object && object->get_shapenum() == shape && object->get_framenum() == 0;
}
bool Virtue_case::accepts(const Container_game_object* cont, const Game_object* object) {
    if (!is_case(cont)) return true;
    const int slot = slot_for(object);
    if (slot < 0) return false;
    Object_iterator next(const_cast<Container_game_object*>(cont)->get_objects());
    while (auto* existing = next.get_next()) if (slot_for(existing) == slot) return false;
    return true;
}
void Virtue_case::seat(Container_game_object* cont, Game_object* object) {
    if (is_case(cont)) {
        const int slot = slot_for(object);
        if (slot >= 0) object->set_shape_pos(slot_x(slot), slot_y(slot));
    }
}
void Virtue_case::open(Container_game_object* cont) {
    auto* manager = Game_window::get_instance()->get_gump_man();
    if (!is_case(cont) || manager->find_gump(cont, -1)) return;
    manager->add_gump(new Case_gump(cont));
    Game_window::get_instance()->set_all_dirty();
}

void Virtue_case::preview_labels_for_test() {
    if (!cli_enabled) return;
    label_preview = true;
    Game_window::get_instance()->set_all_dirty();
}

bool Virtue_case::casting_active() {
    if (cast_session && cast_session->owner.expired()) reset_cast_session();
    return static_cast<bool>(cast_session);
}
bool Virtue_case::accept_spell_target(Game_object* object) {
    if (!cast_session || !cast_session->targeting) return true;
    auto container = cast_session->owner.lock();
    const int slot = slot_for(object);
    if (!container || !object || object->get_owner()!=container.get() || slot<0) return false;
    auto previous = cast_session->target.lock();
    if (previous && previous.get()!=object) return false;
    cast_session->target = weak_from_obj(object); cast_session->slot = slot;
    return true;
}
void Virtue_case::mark_succeeded(Game_object* object) {
    if (!cast_session || cast_session->recall) return;
    auto stone = cast_session->target.lock(); auto container = cast_session->owner.lock();
    if (stone && container && stone.get()==object && object->get_owner()==container.get())
        cast_session->marked = true;
}

bool Virtue_case::targeting_active() { return casting_active() && cast_session->targeting; }
