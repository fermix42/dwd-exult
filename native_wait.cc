#include "native_wait.h"

#include "actors.h"
#include "combat.h"
#include "gameclk.h"
#include "game_time.h"
#include "gamemap.h"
#include "gamewin.h"
#include "Gump_manager.h"
#include "npctime.h"
#include "tqueue.h"
#include "ucmachine.h"
#include "ucsched.h"

#include <cstring>
#include <algorithm>

namespace {
bool advancing = false;
struct Operation {
	Operation() { advancing = true; }
	~Operation() { advancing = false; }
};
}

namespace {
const char* eligibility_impl(Game_window& w) {
	if (w.get_gump_man()->gump_mode()) return "ui-owner";
	if (w.get_tqueue()->is_paused()) return "paused-context";
	if (w.in_combat()) return "combat";
    if (w.get_clock()->scripted_schedule_pending()) return "scripted-schedule-boundary";
    if (!advancing && w.get_clock()->schedules_pending()) return "schedule-reconciliation-pending";
	auto* a = w.get_main_actor();
	if (!a || w.main_actor_dont_move() || !w.main_actor_can_act()) return "script-or-incapacity";
	if (w.get_usecode()->in_usecode() || w.get_usecode()->get_num_faces_on_screen()) return "conversation";
	if (w.get_camera_actor() != a) return "remote-camera";
    if (!w.get_map() || a->get_map_num() != w.get_map()->get_num()) return "world-transition";
	if (w.is_time_stopped()) return "stopped-time";
	if (w.get_moving_barge()) return "moving-vehicle";
	Actor_vector nearby;
	a->find_nearby_actors(nearby, c_any_shapenum, 16);
	for (auto* n : nearby) {
		if (n == a || n->is_in_party() || n->is_dead() || !n->can_act()
			|| !Combat_schedule::is_enemy(a->get_effective_alignment(), n->get_effective_alignment())) continue;
		int points = 0;
		if (n->get_weapon(points) || n->get_property(Actor::strength) > 0) return "nearby-capable-hostile";
	}
	Actor* party[9];
	const int count = w.get_party(party, 1);
	for (int i = 0; i < count; ++i) {
		auto* p = party[i];
		if (p->is_moving() || p->get_action()) return "party-action";
		if (!p->can_act()) return "incapacitated-party";
		if (p->get_flag(Obj_flags::poisoned) || p->get_type_flag(Actor::tf_bleeding)) return "harmful-condition";
        if (p->get_flag(Obj_flags::charmed) || p->temporary_effect_remaining(Obj_flags::charmed) >= 0) return "charmed-party";
		if (p->get_property(Actor::food_level) <= 9 && !p->get_info().does_not_eat()) return "hungry-party";
		if (p->get_temperature() >= 50) return "exposure";
	}
	// Active-world scripts remain pending. Player/party-owned sequences block;
	// unrelated background scripts are never executed or blanket-refused.
	if (Usecode_script::party_sequence()) return "party-deferred-sequence";
	return "allow";
}

}

const char* Native_wait::eligibility(Game_window& w) {
    return advancing ? "reentrant-operation" : eligibility_impl(w);
}

Native_wait::Result Native_wait::advance(Game_window& w, int minutes) {
	Result result{minutes, 0, "invalid-duration"};
	if (minutes < 1 || minutes > 24 * 60) return result;
	result.reason = eligibility(w);
	if (std::strcmp(result.reason, "allow")) return result;
	Operation operation;

    const auto player_generation = w.get_main_actor()->weak_from_this();
    const auto map = w.get_map()->get_num();
    auto same_world = [&]() {
        const auto player = player_generation.lock();
        return player && player.get() == w.get_main_actor() && w.get_map() && w.get_map()->get_num() == map
            && w.get_main_actor()->get_map_num() == map;
    };
    unsigned progress = 0;
    const auto start_tick = Game_time::now();
    const auto target = start_tick + static_cast<uint32>(minutes * ticks_per_minute);
    unsigned long long remainder = 0;
    unsigned boundaries = 0;
    while (Game_time::now() < target) {
        if (++boundaries > 100000) { result.reason = "event-budget"; break; }
        if (!Game_time::dispatch(Game_time::now())) { result.reason = "event-budget"; break; }
        if (!same_world()) { result.reason = "world-changed"; break; }
        const auto now = Game_time::now();
        const auto minute_boundary = now + ticks_per_minute - w.get_clock()->get_ticks();
        const auto deadline = Game_time::next(std::min(target, minute_boundary));
        const auto delta = deadline - now;
        if (!delta) { result.reason = "event-stalled"; break; }
        remainder += static_cast<unsigned long long>(delta) * w.get_std_delay();
        const auto rate = w.get_clock()->get_time_rate();
        const auto elapsed = static_cast<uint32>(remainder / rate);
        remainder %= rate;
        Actor* party[9];
        const int count = w.get_party(party, 1);
        for (int i = 0; i < count; ++i) party[i]->elapse_temporary_effects(elapsed);
        w.get_clock()->advance_wait_ticks(delta);
        progress += delta;
        if (!same_world()) { result.reason = "world-changed"; break; }
        if (!Game_time::dispatch(Game_time::now())) { result.reason = "event-budget"; break; }
        if (!same_world()) { result.reason = "world-changed"; break; }
        result.completed_minutes = progress / ticks_per_minute;
        result.reason = eligibility_impl(w);
        if (std::strcmp(result.reason, "allow")) break;
    }
    result.completed_ticks = progress;
    result.completed_minutes = result.completed_ticks / ticks_per_minute;
    if (progress == static_cast<unsigned>(minutes * ticks_per_minute) && !std::strcmp(result.reason, "allow")) result.reason = "complete";
	w.set_all_dirty();
	return result;
}
