// GPL-2.0-or-later. A physical reference chart; travel belongs to active Usecode.
#pragma once
class Game_object;
class Shapes_vga_file;
namespace Orb_chart {
constexpr int shape = 1600;
void install_shape(Shapes_vga_file& shapes);
bool is_chart(const Game_object* object);
bool give();
void open(Game_object* chart = nullptr);
}
