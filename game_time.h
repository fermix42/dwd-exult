#ifndef GAME_TIME_H
#define GAME_TIME_H
#include "tqueue.h"
#include <string>
class Game_object;
class ODataSource;
class IDataSource;
// Explicit game-calendar queue, backed by the existing Time_queue container.
// Only typed native capabilities can be registered; never an executable Usecode callback.
namespace Game_time {
uint32 now();
bool fuel(Game_object* owner, int period, int spent_shape, int spent_quality, int sound, int legacy_timer = 0);
void cancel(Game_object* owner);
bool has_event(const Game_object* owner);
// End of serialized object materialization, after optional records/legacy scripts.
bool ensure_fuel(Game_object* owner);
uint32 next(uint32 limit);
bool dispatch(uint32 cutoff, unsigned budget = 4096);
void rate_changed(int previous, int current);
void calendar_jump(uint32 previous);
void clear();
void write(ODataSource* out, Game_object* owner);
bool read(IDataSource* in, Game_object* owner, int length);
void finish_load();
bool save_metadata_valid();
unsigned long long metadata_epoch();
void accept_save_metadata(unsigned long long epoch);
std::string describe(const Game_object* owner = nullptr);
}
#endif
