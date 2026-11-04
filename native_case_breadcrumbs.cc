// GPL-2.0-or-later. Stage E writing candidate; Stage D remains unchanged.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "native_case_breadcrumbs.h"
#include "native_case_commission.h"
#include "actors.h"
#include "contain.h"
#include "databuf.h"
#include "effects.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "gamewin.h"
#include "native_wait.h"
#include "native_virtue_case.h"
#include "ucsched.h"
#include "palette.h"
#include "utils.h"
#include "virstone.h"
#include <array>
#include <bitset>
#include <cstring>
#include <cstdint>
#include <set>
#include <sstream>

namespace {
constexpr unsigned pending_a=1, done_a=2, pending_b=4, done_b=8;
unsigned flags=0, next_minute=0;
// Transient pacing only. Eligibility always comes from current inventory.
unsigned natural_after=0;
bool natural_due() {
    return static_cast<std::int32_t>(Game::get_ticks()-natural_after)>=0;
}
int last_npc=-1;
unsigned deliveries=0;
std::string last_text;
std::array<signed char,256> map_valid;
bool valid_destination(Virtue_stone_object* stone) {
    const auto p=stone->get_target_pos();const int map=stone->get_target_map();
    if(p.tx<0 || p.ty<0 || p.tx>=c_num_tiles || p.ty>=c_num_tiles || p.tz<0 || p.tz>255
       || (p.tx==0 && p.ty==0) || map<0 || map>255)return false;
    if(!map_valid[map]) {
        char name[512];
        const bool exists=(is_system_path_defined("<PATCH>") && U7exists(Get_mapped_name(PATCH_U7MAP,map,name)))
            || U7exists(Get_mapped_name(U7MAP,map,name));
        map_valid[map]=exists?1:-1;
    }
    return map_valid[map]>0;
}
struct Inventory { int colors=0,loose_colors=0,marked=0; bool case_owned=false; };
Inventory inventory() {
    Inventory result;auto* w=Game_window::get_instance();
    if(!GAME_BG || !w || !w->get_main_actor())return result;
    Actor* group[9];const int n=w->get_party(group,1);
    std::set<Game_object*> distinct;std::bitset<8> colors,loose_colors;
    for(int i=0;i<n;++i) {
        Game_object_vector cases;group[i]->get_objects(cases,1601,c_any_qual,c_any_framenum);
        for(auto* object:cases)if(Virtue_case::is_case(object))result.case_owned=true;
        Game_object_vector objects;group[i]->get_objects(objects,330,c_any_qual,c_any_framenum);
        for(auto* object:objects) {
            auto* stone=object->as_virtstone();
            if(!stone || !distinct.insert(object).second)continue;
            bool seated=false;
            for(auto* owner=stone->get_owner();owner;owner=owner->get_owner())
                if(Virtue_case::is_case(owner)) {seated=true;break;}
            const int frame=stone->get_framenum();
            if(frame>=0 && frame<8) {colors.set(frame);if(!seated)loose_colors.set(frame);}
            if(valid_destination(stone))++result.marked;
        }
    }
    result.colors=colors.count();result.loose_colors=loose_colors.count();return result;
}
// Native Wait rejects even ordinary walking. Retain its full safety gate for
// idle play, allowing only travel actions after checking its remaining party gates.
bool quiet_or_travel(Game_window& w) {
    const char* reason=Native_wait::eligibility(w);
    if(!std::strcmp(reason,"allow"))return true;
    if(std::strcmp(reason,"party-action"))return false;
    Actor* group[9];const int n=w.get_party(group,1);
    for(int i=0;i<n;++i) {
        auto* p=group[i];
        if((p->get_action() && !p->is_moving()) || !p->can_act()
           || p->get_flag(Obj_flags::asleep) || p->get_flag(Obj_flags::poisoned)
           || p->get_type_flag(Actor::tf_bleeding) || p->get_flag(Obj_flags::charmed)
           || p->temporary_effect_remaining(Obj_flags::charmed)>=0
           || (p->get_property(Actor::food_level)<=9 && !p->get_info().does_not_eat())
           || p->get_temperature()>=50)return false;
    }
    return !Usecode_script::party_sequence();
}
Actor* speaker(int primary,int fallback) {
    auto* w=Game_window::get_instance();auto* avatar=w->get_main_actor();
    Actor* group[9];const int n=w->get_party(group,1);
    for(int wanted:{primary,fallback})for(int i=1;i<n;++i) {
        auto* npc=group[i];
        if(npc->get_npc_num()==wanted && npc->is_in_party() && !npc->is_dead()
           && npc->can_act() && npc->can_speak() && npc->get_map_num()==avatar->get_map_num()
           && npc->distance(avatar)<=8)return npc;
    }
    return nullptr;
}
}
void Case_breadcrumbs::reset() {
    natural_after=Game::get_ticks()+10000;flags=next_minute=deliveries=0;last_npc=-1;last_text.clear();map_valid.fill(0);
}
void Case_breadcrumbs::observe() {
    if(!GAME_BG)return;
    const auto items=inventory();
    const unsigned previous=flags;
    flags&=~(pending_a|pending_b);
    if(!Case_commission::is_delivered() && !items.case_owned) {
        if(items.loose_colors>=3 && !(flags&done_a))flags|=pending_a;
        if(items.marked>=1 && !(flags&done_b))flags|=pending_b;
    }
    if((flags&~previous)&(pending_a|pending_b))natural_after=Game::get_ticks()+10000;
}
void Case_breadcrumbs::inventory_added(Game_object* object) {
    if(!GAME_BG || !object || (!object->as_virtstone() && !object->as_container()))return;
    auto* owner=object->get_outermost()->as_actor();auto* w=Game_window::get_instance();
    if(owner && (owner==w->get_main_actor() || owner->is_in_party()))observe();
}
void Case_breadcrumbs::minute() {
    if(!GAME_BG)return;
    // Existing minute transitions provide bounded opportunities and revalidate
    // removals, nested ownership and old saves without frame-by-frame scans.
    observe();
    if(!(flags&(pending_a|pending_b)) || !natural_due())return;
    auto* w=Game_window::get_instance();
    const auto now=w->get_clock()->get_total_minutes();
    if(now<next_minute || !quiet_or_travel(*w)
       || w->get_pal()->is_faded_out() || w->get_effects()->has_active_sprites()
       || w->get_main_actor()->get_casting_mode()!=Actor::not_casting
       || w->get_effects()->has_text_effects())return;
    Actor* npc=nullptr;unsigned pending=0,done=0;
    if((flags&pending_a) && (npc=speaker(1,3))) {pending=pending_a;done=done_a;}
    else if((flags&pending_b) && (npc=speaker(3,4))) {pending=pending_b;done=done_b;}
    if(!npc)return;
    last_npc=npc->get_npc_num();
    if(pending==pending_a)last_text=last_npc==1
        ? "These stones are filling our packs!"
        : "Our packs are filling with stones.";
    else last_text=last_npc==3
        ? "Knowest thou where each stone leads?"
        : "Which stone leads where? I hope one leads to an inn!";
    npc->say(last_text.c_str());flags=(flags&~pending)|done;
    // One minute update delivers at most one observation. No speech request queue.
    natural_after=Game::get_ticks()+10000;
    next_minute=now+5;++deliveries;
}
void Case_breadcrumbs::write(ODataSource* out) {
    if(!GAME_BG)return;
    observe();if(!flags)return;
    out->write("VSCH",4);out->write4(1);out->write4(flags);out->write4(next_minute);
}
void Case_breadcrumbs::read(IDataSource* in) {
    reset();if(!GAME_BG || in->getSize()-in->getPos()<4)return;
    char magic[4];in->read(magic,4);
    if(!std::memcmp(magic,"VSCQ",4)) {
        if(in->getSize()-in->getPos()<16)return;
        in->skip(16);if(in->getSize()-in->getPos()<4)return;in->read(magic,4);
    }
    if(std::memcmp(magic,"VSCH",4) || in->getSize()-in->getPos()<12 || in->read4()!=1)return;
    const unsigned saved=in->read4(),cooldown=in->read4();
    if(saved>15 || (saved&3)==3 || (saved&12)==12)return;
    flags=saved;next_minute=cooldown;
}
std::string Case_breadcrumbs::inspect() {
    const auto items=inventory();std::ostringstream out;
    out<<"{\"flags\":"<<flags<<",\"a_pending\":"<<bool(flags&pending_a)<<",\"a_done\":"<<bool(flags&done_a)
       <<",\"b_pending\":"<<bool(flags&pending_b)<<",\"b_done\":"<<bool(flags&done_b)
       <<",\"natural_due\":"<<natural_due()<<",\"loose_colors\":"<<items.loose_colors<<",\"case_owned\":"<<items.case_owned
       <<",\"colors\":"<<items.colors<<",\"marked_stones\":"<<items.marked<<",\"next_minute\":"<<next_minute
       <<",\"deliveries_since_load\":"<<deliveries<<",\"last_npc\":"<<last_npc<<",\"last_text\":\""<<last_text<<"\"}";
    return out.str();
}
