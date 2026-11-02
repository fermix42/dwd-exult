// GPL-2.0-or-later. Stage A physical container only.
#pragma once
class Game_object;
class Container_game_object;
class Shapes_vga_file;
namespace Virtue_case {
constexpr int shape = 1601; // Adjacent to the accepted native Orb chart (1600).
void install_shape(Shapes_vga_file& shapes);
bool is_case(const Game_object* object);
bool accepts(const Container_game_object* container, const Game_object* object);
void seat(Container_game_object* container, Game_object* object);
void open(Container_game_object* container);
// Explicit unified-CLI visual trial only; never stored in saves.
void preview_labels_for_test();
}
