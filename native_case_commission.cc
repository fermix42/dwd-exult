// GPL-2.0-or-later. Optional native BG commission; original NPC Usecode stays loaded.
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "native_case_commission.h"
#include "native_virtue_case.h"
#include "actors.h"
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
const std::array<const char*,9> names={"Virtue Stone case","commission case","case materials","case timber","buy case timber","give cloth","pay Guild fee","case progress","collect case"};
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
void receipt(Conversation* conv,const char* text) {
    std::string message=text;
    if(quest.stage==BUILDING)message+="~@All is in hand. The Guild may begin thy case; allow two days for the work.@";
    conv->show_npc_message(message.c_str());
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
    }
    if(npc==93 && (quest.stage==COMMISSION_ACCEPTED || quest.stage==REQUIREMENTS_IN_PROGRESS)
        && !(quest.requirements&1)) {
        result.push_back("case timber");if(offer)result.push_back("buy case timber");
    }
    result.erase(std::remove_if(result.begin(),result.end(),[](const auto& t){return used.count(t);}),result.end());
    return result;
}
}
void Case_commission::reset() { quest={};speaker=0;offer=false;used.clear(); }
void Case_commission::begin(Actor* npc) {
    speaker=GAME_BG && npc && (npc->get_npc_num()==86 || npc->get_npc_num()==93)?npc->get_npc_num():0;
    offer=false;used.clear();
}
void Case_commission::end() { speaker=0;offer=false;used.clear(); }
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
        conv->show_npc_message("Thou sayest, @I should like to commission a carrying case for the Virtue Stones.@~@Virtue Stones? I am not familiar with them. Might I see one?@~Thou showest her a stone from thy company's belongings.~@Oh, how lovely! And quite unusual. Tell me, what sort of case hast thou in mind?@~@Something sturdy, yet small enough to carry on my travels. I should hate to have such precious things rattling about in my pack.@~@Hmm. A fine wooden case, lined with soft cloth to protect the stones. Aye, I believe I could fashion something suitable.@~@Arrange timber with William, bring me a length of cloth, and pay one hundred gold for our labor and fittings. Allow us two days once all is ready.@");
    } else if(topic=="commission case" && quest.stage==NOT_STARTED && party_stone()) {
        quest.stage=COMMISSION_ACCEPTED;offer=false;
        conv->show_npc_message("@Aye, I shall undertake thy commission. William can send the timber directly to our Guild. Thou mayst bring the cloth and pay our fee when thou art ready.@");
    } else if(topic=="case materials") {
        std::string message="@We still require ";
        if(!(quest.requirements&1))message+="timber arranged with William; ";
        if(!(quest.requirements&2))message+="a length of cloth; ";
        if(!(quest.requirements&4))message+="one hundred gold for labor and fittings; ";
        message+="then allow two days for the work.@";conv->show_npc_message(message.c_str());
    } else if(topic=="case timber" && npc==93 && quest.stage<BUILDING && quest.stage>NOT_STARTED && !(quest.requirements&1)) {
        offer=true;conv->show_npc_message("@Xanthia's commission? Aye, I can spare sound, seasoned timber for a small case. Twenty gold, and I shall deliver it straight to the Artist's Guild. Wilt thou buy it?@");
    } else if(topic=="buy case timber" && npc==93 && quest.stage<BUILDING && quest.stage>NOT_STARTED && !(quest.requirements&1)) {
        if(!take(644,20))conv->show_npc_message("@Twenty gold for sound, seasoned timber, delivered to the Artist's Guild. Thou hast not enough gold at present.@");
        else { complete_requirement(1);receipt(conv,"@Twenty gold, then. I shall have the timber sent straight to Xanthia at the Artist's Guild. Thou needst not carry it thyself.@"); }
    } else if(topic=="give cloth" && npc==86 && quest.stage<BUILDING && quest.stage>NOT_STARTED && !(quest.requirements&2)) {
        if(!take(851,1))conv->show_npc_message("@I need a length of cloth for the lining. Bring one when thou art ready.@");
        else { complete_requirement(2);receipt(conv,"@This cloth will do very well. I shall keep it safe for thy case.@"); }
    } else if(topic=="pay Guild fee" && npc==86 && quest.stage<BUILDING && quest.stage>NOT_STARTED && !(quest.requirements&4)) {
        if(!take(644,100))conv->show_npc_message("@The Guild's labor and fittings cost one hundred gold. Thou hast not enough. We can settle it when thou art able.@");
        else { complete_requirement(4);receipt(conv,"@One hundred gold. I thank thee; our fee is settled.@"); }
    } else if(topic=="case progress") {
        conv->show_npc_message("@Good work takes time. Our two days are not yet passed. Return anon, Avatar.@");
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
    refresh();std::ostringstream out;out<<"{\"stage\":"<<quest.stage<<",\"requirements\":"<<quest.requirements<<",\"deadline\":"<<quest.deadline<<",\"minutes\":"<<Game_window::get_instance()->get_clock()->get_total_minutes()<<",\"gold\":"<<quantity(644)<<",\"cloth\":"<<quantity(851)<<",\"party_has_stone\":"<<(party_stone()?"true":"false")<<"}";return out.str();
}

