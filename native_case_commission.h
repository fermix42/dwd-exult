// GPL-2.0-or-later. Native BG Virtue Stone Case commission.
#pragma once
#include <string>
class Actor;
class Conversation;
class IDataSource;
class ODataSource;
namespace Case_commission {
void reset();
void begin(Actor* npc);
void end();
void topics(Conversation* conversation, int npc);
// 0: original topic, 1: handled, 2: end original conversation normally.
int choose(Conversation* conversation, int npc, const std::string& topic);
void write(ODataSource* out);
void read(IDataSource* in);
std::string inspect();
}
