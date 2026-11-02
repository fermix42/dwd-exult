// GPL-2.0-or-later. Native BG physical case, spells and compartment notes.
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
// Explicit unified-CLI fixture: fills the first owned case with sample notes.
void preview_labels_for_test();
}

namespace Virtue_case {
bool casting_active();
bool targeting_active();
unsigned long long cast_attempt_count();
// Invoked only at the existing click-on-item and successful Mark intrinsic boundaries.
bool accept_spell_target(Game_object* object);
void mark_succeeded(Game_object* object);
}

class ODataSource;
namespace Virtue_case {
int notes_ireg_size(const Container_game_object* container);
void write_notes(ODataSource* out, const Container_game_object* container);
void read_notes(Game_object* object, const unsigned char* data, int length);
int editing_slot();
Container_game_object* editing_case();
}
