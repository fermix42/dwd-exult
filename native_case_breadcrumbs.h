// GPL-2.0-or-later. Optional native BG companion observations.
#pragma once
#include <string>
class Game_object;
class IDataSource;
class ODataSource;
namespace Case_breadcrumbs {
void reset();
void observe();
void inventory_added(Game_object* object);
void minute();
void write(ODataSource* out);
// Reads from the beginning of the optional native usecode.var trailer.
void read(IDataSource* in);
std::string inspect();
}
