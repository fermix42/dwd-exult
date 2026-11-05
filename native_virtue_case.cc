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
#include <vector>
#include <string>
#include <algorithm>

namespace {
bool installed = false;
unsigned long long cast_attempts = 0;
class Case_gump;
Case_gump* active_editor = nullptr;
void prompt_label(Container_game_object*, int);
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
        const int w = i == 0 ? 14 : i == 1 ? width : i < 10 ? 13 : 32;
        const int h = i == 0 ? 10 : i == 1 ? height : i < 10 ? 15 : 12;
        const int ox = i == 0 ? 7 : i >= 2 && i < 10 ? 6 : 0;
        const int oy = i == 0 ? 9 : i >= 2 && i < 10 ? 7 : 0;
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
    Usecode_script* mark_effect = nullptr;
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
    if (active_editor || Virtue_case::casting_active() || gwin->get_usecode()->in_usecode()
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
    if (auto target=session->target.lock()) session->mark_effect=Usecode_script::find(target.get());
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
    const bool prompt = marked && !recall && slot>=0 && stone && stone->get_owner()==container.get();
    const int compartment = slot;
    reset_cast_session();
    if (prompt) prompt_label(container->as_container(), compartment);
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
        ++cast_attempts;
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

// Only characters with clean native six-pixel glyphs are accepted by this UI.
std::vector<std::string> note_lines(const std::string& note) {
    if (note.size()>12) return {};
    for (unsigned char c:note) if(c<32 || c>126) return {};
    auto font=fontManager.get_font("TINY_BLACK_FONT");
    if (font->get_text_width(note.c_str())<=32) return {note};
    size_t cut=note.size();
    while(cut && font->get_text_width(note.substr(0,cut).c_str())>32) --cut;
    if(!cut) return {};
    const size_t space=note.rfind(' ',cut);
    const bool at_space=space!=std::string::npos && space>0;
    const auto first=note.substr(0,at_space?space:cut);
    const auto second=note.substr(at_space?space+1:cut);
    if (font->get_text_width(second.c_str())>32) return {};
    return {first,second};
}
class Case_label_button : public Gump_button {
    int slot;
    std::string action;
public:
    Case_label_button(Gump* par,int slot):Gump_button(par,-1,59+71*(slot/4),43+18*(slot%4),SF_OTHER),slot(slot),action("edit_label_"+std::to_string(slot)) {}
    const char* cli_action() const override { return action.c_str(); }
    TileRect get_rect() const override { int sx=0,sy=0;local_to_screen(sx,sy);return TileRect(sx,sy,36,16); }
    bool on_widget(int mx,int my) const override { screen_to_local(mx,my);return mx>=0 && my>=0 && mx<36 && my<16; }
    void paint() override {} // Original parchment artwork is already present.
    bool activate(MouseButton button) override;
};
class Case_gump : public Gump {
    // Own all frames for as long as child controls borrow them.
    std::unique_ptr<Shape_file> art;
    Shape_frame* background;
    std::array<Shape_frame*, 8> seated{};
    int editing = -1;
    std::string draft;
public:
    explicit Case_gump(Container_game_object* cont) : Gump(cont, 0, 0, -1), art(load_art()), background(art->get_frame(1)) {
        set_object_area(TileRect(0, 0, width, height));
        for (int i = 0; i < 8; ++i) seated[i] = art->get_frame(i+2);
        add_elem(new Case_button(this, art.get(), 10, 38, 14, "MARK"));
        add_elem(new Case_button(this, art.get(), 12, 130, 14, "RECALL"));
        // Original BG checkmark frame has bottom-right hotspot (20,20).
        add_elem(new Checkmark_button(this, 23, 68));
        for(int i=0;i<8;++i) add_elem(new Case_label_button(this,i));
        set_pos();
    }
    ~Case_gump() override { if(active_editor==this) cancel_edit(); }
    int edit_slot() const { return editing; }
    void cancel_edit() {
        editing=-1;draft.clear();handles_kbd=false;
        if(active_editor==this) {
            active_editor=nullptr;
            auto* focus=gumpman->get_kbd_focus();
            if(focus==this)gumpman->set_kbd_focus(nullptr);
            if(!focus || focus==this)SDL_StopTextInput(gwin->get_win()->get_screen_window());
        }
        gwin->set_all_dirty();
    }
    void begin_edit(int slot) {
        if(slot<0 || slot>=8 || Virtue_case::casting_active() || gwin->get_usecode()->in_usecode()) return;
        if(active_editor) active_editor->cancel_edit();
        editing=slot;draft=container->get_case_note(slot);active_editor=this;handles_kbd=true;
        gumpman->set_kbd_focus(this);SDL_StartTextInput(gwin->get_win()->get_screen_window());gwin->set_all_dirty();
    }
    void update_gump() override {
        if(editing>=0 && gumpman->get_kbd_focus()!=this) cancel_edit();
    }
    bool key_down(SDL_Keycode key, SDL_Keycode) override {
        if(editing<0) return false;
        if(key==SDLK_ESCAPE) cancel_edit();
        else if(key==SDLK_RETURN || key==SDLK_KP_ENTER) { container->set_case_note(editing,draft);cancel_edit(); }
        else if(key==SDLK_BACKSPACE) { if(!draft.empty())draft.pop_back();gwin->set_all_dirty(); }
        return true;
    }
    bool text_input(const char* text) override {
        if(editing<0) return false;
        for(const unsigned char* p=reinterpret_cast<const unsigned char*>(text); *p; ++p) {
            if(*p<32 || *p>126) continue;
            const std::string candidate=draft+static_cast<char>(*p);
            if(!note_lines(candidate).empty()) draft=candidate;
        }
        gwin->set_all_dirty();return true;
    }
    bool handle_kbd_event(void* ev) override {
        if(editing<0) return false;
        auto& event=*static_cast<SDL_Event*>(ev);
        if(event.type==SDL_EVENT_TEXT_INPUT)return text_input(event.text.text);
        if(event.type==SDL_EVENT_KEY_DOWN)return key_down(event.key.key,0);
        return false;
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
        auto font=fontManager.get_font("TINY_BLACK_FONT");
        for(int slot=0;slot<8;++slot) {
            const auto& note=editing==slot ? draft : container->get_case_note(slot);
            const auto lines=note_lines(note);
            for(size_t line=0;line<lines.size();++line) {
                const int w=font->get_text_width(lines[line].c_str());
                const int tx=x+61+71*(slot/4)+(32-w)/2;
                const int ty=y+45+18*(slot%4)+(lines.size()==1?3:6*line);
                font->paint_text(gwin->get_win()->get_ib8(),lines[line].c_str(),tx,ty+4-font->get_text_baseline());
                if(editing==slot && line+1==lines.size())
                    gwin->get_win()->get_ib8()->fill8(0,1,6,x+61+71*(slot/4)+std::min(31,(32-w)/2+w),ty);
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
bool Case_label_button::activate(MouseButton button) {
    if(button!=MouseButton::Left) return false;
    static_cast<Case_gump*>(parent)->begin_edit(slot);return true;
}
void prompt_label(Container_game_object* container,int slot) {
    Virtue_case::open(container);
    auto* manager=Game_window::get_instance()->get_gump_man();
    if(auto* g=dynamic_cast<Case_gump*>(manager->find_gump(container,-1))) g->begin_edit(slot);
}
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
bool Virtue_case::available() { return installed && GAME_BG; }
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
    Game_object_vector cases;
    Game_window::get_instance()->get_main_actor()->get_objects(cases,shape,c_any_qual,0);
    for(auto* object:cases) if(is_case(object)) {
        auto* container=object->as_container();
        for(int i=0;i<8;++i)container->set_case_note(i,std::string(label_examples[i][0])+" "+label_examples[i][1]);
        Game_window::get_instance()->set_all_dirty();break;
    }
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
    if (stone && container && stone.get()==object && object->get_owner()==container.get()
        && (cast_session->targeting || (cast_session->mark_effect && Usecode_script::find(object)==cast_session->mark_effect)))
        cast_session->marked = true;
}

bool Virtue_case::targeting_active() { return casting_active() && cast_session->targeting; }

int Virtue_case::editing_slot() { return active_editor ? active_editor->edit_slot() : -1; }
Container_game_object* Virtue_case::editing_case() { return active_editor ? active_editor->get_container() : nullptr; }
int Virtue_case::notes_ireg_size(const Container_game_object* container) {
    if(!is_case(container)) return 0;
    bool any=false;int size=16; // Four-byte special header, VSC1, eight byte lengths.
    for(int i=0;i<8;++i) { const auto& note=container->get_case_note(i);any|=!note.empty();size+=note.size(); }
    return any?size:0;
}
void Virtue_case::write_notes(ODataSource* out,const Container_game_object* container) {
    const int size=notes_ireg_size(container);if(!size)return;
    out->write1(255);out->write1(6);out->write2(size-4);out->write("VSC1",4);
    for(int i=0;i<8;++i) { const auto& note=container->get_case_note(i);out->write1(note.size());out->write(note.data(),note.size()); }
}
void Virtue_case::read_notes(Game_object* object,const unsigned char* data,int length) {
    if(!is_case(object) || length<12 || std::string(reinterpret_cast<const char*>(data),4)!="VSC1") return;
    std::array<std::string,8> notes;int pos=4;
    for(auto& note:notes) {
        if(pos>=length)return;const int count=data[pos++];if(count>12 || pos+count>length)return;
        note.assign(reinterpret_cast<const char*>(data+pos),count);pos+=count;
        for(unsigned char c:note)if(c<32 || c>126)return;
    }
    if(pos!=length)return;
    for(int i=0;i<8;++i)object->as_container()->set_case_note(i,notes[i]);
}

unsigned long long Virtue_case::cast_attempt_count() { return cast_attempts; }
