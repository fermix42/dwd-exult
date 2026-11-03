// GPL-2.0-or-later. Optional native BG commission; original NPC Usecode stays loaded.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "native_case_commission.h"
#include "native_virtue_case.h"
#include "actors.h"
#include "actions.h"
#include "chunks.h"
#include "gamemap.h"
#include "contain.h"
#include "conversation.h"
#include "useval.h"
#include "game.h"
#include "gamewin.h"
#include "gameclk.h"
#include "databuf.h"
#include <array>
#include <set>
#include <vector>
#include <sstream>
#include <algorithm>
namespace {
enum Stage { NOT_STARTED, COMMISSION_ACCEPTED, REQUIREMENTS_IN_PROGRESS, BUILDING, READY_FOR_DELIVERY, DELIVERED };
struct Quest { unsigned stage=NOT_STARTED, requirements=0, deadline=0; } quest;
int speaker=0;
bool offer=false;
std::set<std::string> used;
const std::array<const char*,9> names={"Virtue Stone case","commission case","case materials","timber delivery","buy timber","give cloth","pay Guild fee","case progress","collect case"};
std::vector<Actor*> party() {
    Actor* actors[9];auto* w=Game_window::get_instance();const int n=w->get_party(actors,1);
    return {actors,actors+n};
}
Game_object* party_stone() {
    for(auto* actor:party()) { Game_object_vector items;actor->get_objects(items,330,c_any_qual,c_any_framenum);
        for(auto* item:items) if(item->as_virtstone())return item; }
    return nullptr;
}
int quantity(int shape) {
    int total=0;for(auto* actor:party())total+=actor->count_objects(shape,c_any_qual,c_any_framenum);return total;
}
bool take(int shape,int count) {
    if(quantity(shape)<count)return false;
    for(auto* actor:party()) { count=actor->remove_quantity(count,shape,c_any_qual,c_any_framenum);if(!count)return true; }
    return false;
}
void complete_requirement(unsigned bit) {
    quest.requirements|=bit;quest.stage=REQUIREMENTS_IN_PROGRESS;
    if(quest.requirements==7) {
        quest.stage=BUILDING;
        quest.deadline=Game_window::get_instance()->get_clock()->get_total_minutes()+48*60;
    }
}
void refresh() {
    if(quest.stage==BUILDING && Game_window::get_instance()->get_clock()->get_total_minutes()>=quest.deadline)
        quest.stage=READY_FOR_DELIVERY;
}
#include "native_case_delivery.inc"

// Native quest strings do not pass through the Usecode string-register parser.
// Format only these strings; the original NPC conversation remains untouched.
void say(Conversation* conv,std::string text) {
    std::replace(text.begin(),text.end(),'@','"');
    std::replace(text.begin(),text.end(),'~','\n');
    conv->show_npc_message(text.c_str());
}
void receipt(Conversation* conv,const char* text) {
    std::string message=text;
    if(speaker==86 && quest.stage==BUILDING)message+="~@All is in hand. The Guild may begin thy case; allow two days for the work.@";
    say(conv,message.c_str());
}
std::vector<std::string> available(int npc) {
    refresh();std::vector<std::string> result;
    if(npc!=speaker || !GAME_BG)return result;
    if(npc==86) {
        if(quest.stage==NOT_STARTED && party_stone()) {
            result.push_back("Virtue Stone case");if(offer)result.push_back("commission case");
        } else if(quest.stage==COMMISSION_ACCEPTED || quest.stage==REQUIREMENTS_IN_PROGRESS) {
            result.push_back("case materials");
            if(!(quest.requirements&2))result.push_back("give cloth");
            if(!(quest.requirements&4))result.push_back("pay Guild fee");
        }
        else if(quest.stage==BUILDING)result.push_back("case progress");
        else if(quest.stage==READY_FOR_DELIVERY && !active_delivery)result.push_back("collect case");
    }
    if(npc==93 && (quest.stage==COMMISSION_ACCEPTED || quest.stage==REQUIREMENTS_IN_PROGRESS)
        && !(quest.requirements&1)) {
        result.push_back("timber delivery");if(offer)result.push_back("buy timber");
    }
    result.erase(std::remove_if(result.begin(),result.end(),[](const auto& t){return used.count(t);}),result.end());
    return result;
}
}
void Case_commission::reset() {
    ++delivery_generation;active_delivery=nullptr;delivery_requested=false;delivery_phase=-1;placed_at=Tile_coord(-1,-1,-1);
    quest={};speaker=0;offer=false;used.clear();
}
void Case_commission::begin(Actor* npc) {
    speaker=GAME_BG && npc && (npc->get_npc_num()==86 || npc->get_npc_num()==93)?npc->get_npc_num():0;
    offer=false;used.clear();
}
void Case_commission::end() {
    const bool deliver=delivery_requested;delivery_requested=false;speaker=0;offer=false;used.clear();
    if(deliver && !start_guild_delivery()) {
        if(auto* npc=Game_window::get_instance()->get_npc(86))npc->say("Give me room to reach my workbench, then ask me again.");
    }
}
void Case_commission::topics(Conversation* conv,int npc) {
    if(npc!=speaker || !npc)return;
    for(const auto* name:names) { Usecode_value value(name);conv->remove_answer(value); }
    for(const auto& topic:available(npc)) { Usecode_value value(topic.c_str());conv->add_answer(value); }
}
int Case_commission::choose(Conversation* conv,int npc,const std::string& topic) {
    if(!npc || npc!=speaker || std::find(names.begin(),names.end(),topic)==names.end())return 0;
    const auto choices=available(npc);
    if(std::find(choices.begin(),choices.end(),topic)==choices.end())return 1;
    used.insert(topic);
    if(topic=="Virtue Stone case" && quest.stage==NOT_STARTED && party_stone()) {
        offer=true;
        say(conv,"@I should like to commission a carrying case for the Virtue Stones,@ thou sayest.~@Virtue Stones? I am not familiar with them. Might I see one?@~Thou showest her a stone from thy company's belongings.~@Oh, how lovely! And quite unusual. Tell me, what sort of case hast thou in mind?@~@Something sturdy, yet small enough to carry on my travels. I should hate to have such precious things rattling about in my pack,@ thou repliest.~@Hmm. A fine wooden case, lined with soft cloth to protect the stones. Aye, I believe I could fashion something suitable.@~@I shall need good timber and a length of cloth for the lining. The Guild's labor and fittings will cost one hundred gold.@");
    } else if(topic=="commission case" && quest.stage==NOT_STARTED && party_stone()) {
        quest.stage=COMMISSION_ACCEPTED;offer=false;
        say(conv,"@Aye, I shall undertake thy commission. Arrange the timber with William; he can send it straight to the Guild. Bring me the cloth and our fee when thou art ready. Once all is in hand, allow us two days for the work.@");
    } else if(topic=="case materials") {
        std::vector<std::string> needed;
        if(!(quest.requirements&1))needed.push_back("timber from William");
        if(!(quest.requirements&2))needed.push_back("a length of cloth");
        if(!(quest.requirements&4))needed.push_back("one hundred gold for labor and fittings");
        std::string message="@We still need ";
        for(size_t i=0;i<needed.size();++i) {
            if(i)message+=i+1==needed.size()?" and ":", ";
            message+=needed[i];
        }
        message+=". Once all is in hand, allow us two days for the work.@";say(conv,message.c_str());
    } else if(topic=="timber delivery" && npc==93 && quest.stage<BUILDING && quest.stage>NOT_STARTED && !(quest.requirements&1)) {
        offer=true;say(conv,"@I require some fine timber. Couldst thou arrange its delivery to the Artist's Guild?@ thou askest.~@Aye, I can provide suitable wood for twenty gold and have it sent to the Guild. Wilt thou buy it?@");
    } else if(topic=="buy timber" && npc==93 && quest.stage<BUILDING && quest.stage>NOT_STARTED && !(quest.requirements&1)) {
        if(!take(644,20))say(conv,"@Twenty gold for sound, seasoned timber, delivered to the Artist's Guild. Thou hast not enough gold at present.@");
        else { complete_requirement(1);say(conv,"@Twenty gold, then. I shall have the timber sent straight to the Artist's Guild.@"); }
    } else if(topic=="give cloth" && npc==86 && quest.stage<BUILDING && quest.stage>NOT_STARTED && !(quest.requirements&2)) {
        if(!take(851,1))say(conv,"@I need a length of cloth for the lining. Bring one when thou art ready.@");
        else { complete_requirement(2);receipt(conv,"@This cloth will do very well. I shall keep it safe for thy case.@"); }
    } else if(topic=="pay Guild fee" && npc==86 && quest.stage<BUILDING && quest.stage>NOT_STARTED && !(quest.requirements&4)) {
        if(!take(644,100))say(conv,"@The Guild's labor and fittings cost one hundred gold. Thou hast not enough. We can settle it when thou art able.@");
        else { complete_requirement(4);receipt(conv,"@One hundred gold. I thank thee; our fee is settled.@"); }
    } else if(topic=="collect case" && npc==86 && quest.stage==READY_FOR_DELIVERY && !active_delivery) {
        if(free_bench_spot(guild_workbench()).tx<0)say(conv,"@Thy case is finished, but I need a little clear space upon my workbench. Make room, then ask me again.@");
        else {delivery_requested=true;say(conv,"@Thy case is finished, Avatar! Come, I shall set it upon my workbench for thee.@");return 2;}
    } else if(topic=="case progress") {
        say(conv,"@Good work takes time. Our two days are not yet passed. Return anon, Avatar.@");
    }
    return 1;
}
void Case_commission::write(ODataSource* out) {
    if(!GAME_BG || quest.stage==NOT_STARTED)return;
    out->write("VSCQ",4);out->write4(1);out->write4(quest.stage);out->write4(quest.requirements);out->write4(quest.deadline);
}
void Case_commission::read(IDataSource* in) {
    reset();if(!GAME_BG || in->getSize()-in->getPos()<20)return;
    char magic[4];in->read(magic,4);if(std::string(magic,4)!="VSCQ" || in->read4()!=1)return;
    Quest value;value.stage=in->read4();value.requirements=in->read4();value.deadline=in->read4();
    if(value.stage>DELIVERED || value.requirements>7 || (value.stage>=BUILDING && (value.requirements!=7 || !value.deadline)))return;
    quest=value;
}
std::string Case_commission::inspect() {
    refresh();std::ostringstream out;out<<"{\"stage\":"<<quest.stage<<",\"requirements\":"<<quest.requirements<<",\"deadline\":"<<quest.deadline<<",\"minutes\":"<<Game_window::get_instance()->get_clock()->get_total_minutes()<<",\"delivery_active\":"<<(active_delivery?"true":"false")<<",\"delivery_phase\":"<<delivery_phase<<",\"placed_x\":"<<placed_at.tx<<",\"placed_y\":"<<placed_at.ty<<",\"placed_z\":"<<placed_at.tz<<",\"gold\":"<<quantity(644)<<",\"cloth\":"<<quantity(851)<<",\"party_has_stone\":"<<(party_stone()?"true":"false")<<"}";return out.str();
}

