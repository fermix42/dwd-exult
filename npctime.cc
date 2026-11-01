/*
 *  Npctime.cc - Timed-even handlers for NPC's.
 *
 *  Copyright (C) 2000-2022  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "npctime.h"
#include "game_time.h"
#include <algorithm>
#include <string>

#include "actors.h"
#include "game.h"
#include "gameclk.h"
#include "gamemap.h"
#include "gamewin.h"
#include "ignore_unused_variable_warning.h"
#include "items.h"
#include "schedule.h"

using std::rand;

extern bool god_mode;

/*
 *  Base class for keeping track of things like poison, protection, hunger.
 */
class Npc_timer : public Time_sensitive, public Game_singletons {
protected:
	Npc_timer_list* list;            // Where NPC stores ->this.
	uint32          get_minute();    // Get game minutes.
public:
	Npc_timer(Npc_timer_list* l, int start_delay = 0);
	~Npc_timer() override;
};

/*
 *  Handle starvation.
 */
class Npc_hunger_timer : public Npc_timer {
	uint32 last_time;    // Last game minute when penalized.
public:
	Npc_hunger_timer(Npc_timer_list* l) : Npc_timer(l, 5000) {
		last_time = get_minute();
	}

	// Handle events:
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Handle poison.
 */
class Npc_poison_timer : public Npc_timer {
	uint32 end_time;    // Time when it wears off.
public:
	Npc_poison_timer(Npc_timer_list* l);
	// Handle events:
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Handle sleep.
 */
class Npc_sleep_timer : public Npc_timer {
	uint32 end_time;    // Time when it wears off.
public:
	Npc_sleep_timer(Npc_timer_list* l) : Npc_timer(l) {
		// Lasts 5-10 seconds..
		end_time = Game::get_ticks() + 5000 + rand() % 5000;
	}

	// Handle events:
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Invisibility timer.
 */
class Npc_invisibility_timer : public Npc_timer {
	friend class Npc_timer_list;
	uint32 end_time;    // Time when it wears off.
public:
	Npc_invisibility_timer(Npc_timer_list* l) : Npc_timer(l) {
		// Lasts 60-80 seconds..
		end_time = Game::get_ticks() + 60000 + rand() % 20000;
	}

	// Handle events:
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Protection timer.
 */
class Npc_protection_timer : public Npc_timer {
	friend class Npc_timer_list;
	uint32 end_time;    // Time when it wears off.
public:
	Npc_protection_timer(Npc_timer_list* l) : Npc_timer(l) {
		// Lasts 60-80 seconds..
		end_time = Game::get_ticks() + 60000 + rand() % 20000;
	}

	// Handle events:
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Handle bleeding.
 */
class Npc_bleed_timer : public Npc_timer {
	uint32 end_time;    // Time when it wears off.
public:
	Npc_bleed_timer(Npc_timer_list* l);
	// Handle events:
	void handle_event(unsigned long curtime, uintptr udata) override;
};

/*
 *  Timer for flags that don't need any other checks.
 */
class Npc_flag_timer : public Npc_timer {
	friend class Npc_timer_list;
	int    flag;        // Flag # in Obj_flags.
	uint32 end_time;    // Time when it wears off.
public:
	Npc_flag_timer(Npc_timer_list* l, int f) : Npc_timer(l), flag(f) {
		// Lasts 60-120 seconds..
		end_time = Game::get_ticks() + 60000 + rand() % 60000;
	}

	// Handle events:
	void handle_event(unsigned long curtime, uintptr udata) override;
};

Npc_timer_list::Npc_timer_list(Actor* n) : npc(n->weak_from_this()) {}

// Needs to be here because Npc_timer is an incomplete type in the header.
Npc_timer_list::~Npc_timer_list() = default;

void Npc_timer_list::stop_hunger() {
	hunger.reset();
}

void Npc_timer_list::stop_poison() {
	poison.reset();
}

void Npc_timer_list::stop_sleep() {
	sleep.reset();
}

void Npc_timer_list::stop_invisibility() {
	invisibility.reset();
}

void Npc_timer_list::stop_protection() {
	protection.reset();
}

void Npc_timer_list::stop_flag(int flag) {
	auto iter = flags.find(flag);
	if (iter != flags.end()) {
		flags.erase(iter);
	}
}

void Npc_timer_list::stop_bleeding() {
	bleed.reset();
}

std::shared_ptr<Actor> Npc_timer_list::get_npc() const {
	std::shared_ptr<Actor> npc_shared = std::static_pointer_cast<Actor>(npc.lock());
	return npc_shared;
}

/*
 *  Start hunger (if not already there).
 */

void Npc_timer_list::start_hunger() {
	if (hunger == nullptr) {
		// Not already there?
		hunger = std::make_unique<Npc_hunger_timer>(this);
	}
}

/*
 *  Start poison.
 */

void Npc_timer_list::start_poison() {
	poison = std::make_unique<Npc_poison_timer>(this);
}

/*
 *  Start sleep.
 */

void Npc_timer_list::start_sleep() {
	sleep = std::make_unique<Npc_sleep_timer>(this);
}

/*
 *  Start invisibility.
 */

void Npc_timer_list::start_invisibility() {
	invisibility = std::make_unique<Npc_invisibility_timer>(this);
}

/*
 *  Start protection.
 */

void Npc_timer_list::start_protection() {
	protection = std::make_unique<Npc_protection_timer>(this);
}

void Npc_timer_list::start_flag(int flag) {
	flags[flag] = std::make_unique<Npc_flag_timer>(this, flag);
}

/*
 *  Start might.
 */

void Npc_timer_list::start_might() {
	start_flag(Obj_flags::might);
}

/*
 *  Start curse.
 */

void Npc_timer_list::start_curse() {
	start_flag(Obj_flags::cursed);
}

/*
 *  Start charm.
 */

void Npc_timer_list::start_charm() {
	start_flag(Obj_flags::charmed);
}

/*
 *  Start paralyze.
 */

void Npc_timer_list::start_paralyze() {
	start_flag(Obj_flags::paralyzed);
}

/*
 *  Start bleeding.
 */

void Npc_timer_list::start_bleeding() {
	bleed = std::make_unique<Npc_bleed_timer>(this);
}

/*
 *  Get game minute from start.
 */

uint32 Npc_timer::get_minute() {
	return 60 * gclock->get_total_hours() + gclock->get_minute();
}

/*
 *  Start timer.
 */

Npc_timer::Npc_timer(
		Npc_timer_list* l,
		int             start_delay    // Time in msecs. before starting.
		)
		: list(l) {
	gwin->get_tqueue()->add(Game::get_ticks() + start_delay, this);
}

/*
 *  Be sure we're no longer in the time queue.
 */

Npc_timer::~Npc_timer() {
	if (in_queue()) {
		Time_queue* tq = Game_window::get_instance()->get_tqueue();
		tq->remove(this);
	}
}

/*
 *  Time to penalize for hunger.
 */

void Npc_hunger_timer::handle_event(unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(udata);
	Actor_shared npc = list->get_npc();
	if (!npc) {
		list->stop_hunger();
		return;
	}
	const int food = npc->get_property(static_cast<int>(Actor::food_level));
	// No longer a party member? or no longer hungry? or dead?
	if (!npc->is_in_party() || food > 9 || npc->is_dead()) {
		list->stop_hunger();
		return;
	}
	const uint32 minute = get_minute();
	// Once/minute.
	if (minute != last_time) {
		if (!npc->is_knocked_out()) {
			if ((rand() % 3) == 0) {
				npc->say_hunger_message();
			}
			if (food <= 0 && (rand() % 5) < 2) {
				npc->reduce_health(1, Weapon_data::starvation_damage);
			}
		}
		last_time = minute;
	}
	gwin->get_tqueue()->add(curtime + 5000, this);
}

/*
 *  Initialize poison timer.
 */

Npc_poison_timer::Npc_poison_timer(Npc_timer_list* l) : Npc_timer(l, 5000) {
	// Lasts 1-3 minutes.
	end_time = Game::get_ticks() + 60000 + rand() % 120000;
}

/*
 *  Time to penalize for poison, or see if it's worn off.
 */

void Npc_poison_timer::handle_event(unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(udata);
	Actor_shared npc = list->get_npc();
	if (!npc) {
		list->stop_poison();
		return;
	}
	// Long enough? Or cured? Or dead?
	if (curtime >= end_time || !npc->get_flag(Obj_flags::poisoned) || npc->is_dead()) {
		npc->clear_flag(Obj_flags::poisoned);
		list->stop_poison();
		return;
	}
	const int penalty = rand() % 3;
	npc->reduce_health(penalty, Weapon_data::poison_damage);

	// Check again in 10-20 secs.
	gwin->get_tqueue()->add(curtime + 10000 + rand() % 10000, this);
}

/*
 *  Time to see if we should wake up.
 */

void Npc_sleep_timer::handle_event(unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(udata);
	Actor_shared npc = list->get_npc();
	if (!npc) {
		list->stop_sleep();
		return;
	}
	if (npc->get_property(static_cast<int>(Actor::health)) <= 0) {
		if (npc->is_in_party() || gmap->is_chunk_read(npc->get_cx(), npc->get_cy())) {
			// 1 in 6 every half minute = approx. 1 HP every 3 min.
			if (rand() % 6 == 0) {
				npc->mend_wounds(false);
			}
		} else {
			// If not nearby, and not in party, just set health and mana to
			// full.
			npc->set_property(static_cast<int>(Actor::health), npc->get_property(static_cast<int>(Actor::strength)));
			npc->set_property(static_cast<int>(Actor::mana), npc->get_property(static_cast<int>(Actor::magic)));
		}
	}
	// Don't wake up someone beaten into unconsciousness.
	// Long enough?  Or cured?
	if (npc->get_property(static_cast<int>(Actor::health)) >= 1 && (curtime >= end_time || !npc->get_flag(Obj_flags::asleep))) {
		// Avoid waking sleeping people.
		if (npc->get_schedule_type() == Schedule::sleep) {
			// This breaks Gorlab Swamp in SI/SS.
			// npc->clear_sleep();
		} else if (!npc->is_dead()) {
			// Don't wake the dead.
			npc->clear_flag(Obj_flags::asleep);
			const int frnum = npc->get_framenum();
			if ((frnum & 0xf) == Actor::sleep_frame &&
				// Slimes don't change.
				!npc->get_info().has_strange_movement()) {
				// Stand up.
				npc->change_frame(Actor::standing | (frnum & 0x30));
			}
		}
		list->stop_sleep();
		return;
	}
	// Check again every half a game minute.
	gwin->get_tqueue()->add(curtime + (ticks_per_minute * gwin->get_std_delay()) / 2, this);
}

/*
 *  Check for a given ring.
 */

inline bool Wearing_ring(
		Actor* actor,
		int    shnum,    // Ring shape to look for.
		int    frnum) {
	// See if wearing ring.
	auto is_this_ring = [&](Game_object* ring) {
		return ring != nullptr && ring->get_shapenum() == shnum && ring->get_framenum() == frnum;
	};
	return is_this_ring(actor->get_readied(lfinger)) || is_this_ring(actor->get_readied(rfinger));
}

/*
 *  See if invisibility wore off.
 */

void Npc_invisibility_timer::handle_event(unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(udata);
	Actor_shared npc = list->get_npc();
	if (!npc) {
		list->stop_invisibility();
		return;
	}
	// TODO: de-hard-code this. Works for SI and BG.
	if (Wearing_ring(npc.get(), 296, 0)) {
		// Wearing invisibility ring.
		// Don't need timer.
		list->stop_invisibility();
		return;
	}
	// Long enough?  Or cleared.
	if (curtime >= end_time || !npc->get_flag(Obj_flags::invisible)) {
		npc->clear_flag(Obj_flags::invisible);
		if (!npc->is_dead()) {
			gwin->add_dirty(npc.get());
		}
		list->stop_invisibility();
		return;
	}
	// Check again in 2 secs.
	gwin->get_tqueue()->add(curtime + 2000, this);
}

/*
 *  See if protection wore off.
 */

void Npc_protection_timer::handle_event(unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(udata);
	Actor_shared npc = list->get_npc();
	if (!npc) {
		list->stop_protection();
		return;
	}
	// TODO: de-hard-code this. SI has an Amulet.
	if (Wearing_ring(npc.get(), 297, 0)) {
		// Wearing protection ring.
		// Don't need timer.
		list->stop_protection();
		return;
	}
	// Long enough?  Or cleared.
	if (curtime >= end_time || !npc->get_flag(Obj_flags::protection)) {
		npc->clear_flag(Obj_flags::protection);
		if (!npc->is_dead()) {
			gwin->add_dirty(npc.get());
		}
		list->stop_protection();
		return;
	}
	// Check again in 2 secs.
	gwin->get_tqueue()->add(curtime + 2000, this);
}

/*
 *  Initialize bleeding timer.
 */

Npc_bleed_timer::Npc_bleed_timer(Npc_timer_list* l) : Npc_timer(l, 5000) {
	Actor_shared npc = list->get_npc();
	// If this assert ever triggers, we have bigger problems in our hands...
	assert(npc);
	npc->bleed();
	// Lasts 1-3 minutes.
	end_time = Game::get_ticks() + 60000 + rand() % 120000;
}

/*
 *  Time to bleed, or see if it's worn off.
 */

void Npc_bleed_timer::handle_event(unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(udata);
	Actor_shared npc = list->get_npc();
	if (!npc) {
		list->stop_bleeding();
		return;
	}
	// Long enough? Or cured? Or dormant? Or dead?
	if (curtime >= end_time || !npc->get_type_flag(Actor::tf_bleeding) || npc->is_dead() || npc->is_dormant()) {
		npc->clear_type_flag(Actor::tf_bleeding);
		list->stop_bleeding();
		return;
	}
	// TODO: Maybe also damage the NPC a bit when bleeding?
	npc->bleed();
	// Check again in .5-1 secs.
	gwin->get_tqueue()->add(curtime + 500 + rand() % 500, this);
}

/*
 *  Might/curse/charm/paralyze wore off.
 */

void Npc_flag_timer::handle_event(unsigned long curtime, uintptr udata) {
	ignore_unused_variable_warning(udata);
	Actor_shared npc = list->get_npc();
	if (!npc) {
		list->stop_flag(flag);
		return;
	}
	// Long enough?  Or cleared.
	if (curtime >= end_time || !npc->get_flag(flag)) {
		npc->clear_flag(flag);
		list->stop_flag(flag);
		return;
	}
	// Check again in 10 secs.
	gwin->get_tqueue()->add(curtime + 10000, this);
}

void Actor::elapse_temporary_effects(uint32 milliseconds) {
    if (timers) timers->elapse(milliseconds);
}
int Actor::temporary_effect_remaining(int flag) const {
    return timers ? timers->remaining(flag) : -1;
}

int Npc_timer_list::remaining(int flag) const {
    uint32 end;
    if (flag == Obj_flags::protection && protection) end = protection->end_time;
    else if (flag == Obj_flags::invisible && invisibility) end = invisibility->end_time;
    else {
        auto it = flags.find(flag);
        if (it == flags.end()) return -1;
        end = it->second->end_time;
    }
    const auto delta = static_cast<sint32>(end - Game::get_ticks());
    return delta > 0 ? delta : 0;
}
void Npc_timer_list::elapse(uint32 milliseconds) {
    const auto now = Game::get_ticks();
    auto* queue = Game_window::get_instance()->get_tqueue();
    auto age = [&](auto& owner, int flag) {
        if (!owner) return;
        auto* timer = owner.get();
        const auto left = static_cast<uint32>(remaining(flag));
        timer->end_time = now + (left > milliseconds ? left - milliseconds : 0);
        while (queue->remove(timer)) {}
        // Existing cleanup may destroy timer: never dereference it afterwards.
        timer->handle_event(now, 0);
    };
    age(protection, Obj_flags::protection);
    age(invisibility, Obj_flags::invisible);
    for (auto it = flags.begin(); it != flags.end();) {
        const auto current = it++;
        age(current->second, current->first);
    }
}

namespace {
// Optional v2 metadata in the existing named-attribute envelope. Values in that
// envelope are 16-bit, so durations use two unsigned words, with 0 = absent and
// remaining+1 = present. Do not put millisecond durations in a single attribute.
constexpr const char* effect_version = "exult.effect.v";
constexpr const char* effect_epoch[] = {"exult.effect.e0", "exult.effect.e1", "exult.effect.e2", "exult.effect.e3"};
struct Effect_key { int flag; const char* low; const char* high; };
const Effect_key effect_keys[] = {
    {Obj_flags::protection, "exult.effect.p.lo", "exult.effect.p.hi"},
    {Obj_flags::invisible, "exult.effect.i.lo", "exult.effect.i.hi"},
    {Obj_flags::might, "exult.effect.m.lo", "exult.effect.m.hi"},
    {Obj_flags::cursed, "exult.effect.c.lo", "exult.effect.c.hi"},
    {Obj_flags::charmed, "exult.effect.h.lo", "exult.effect.h.hi"},
    {Obj_flags::paralyzed, "exult.effect.z.lo", "exult.effect.z.hi"}
};
}
void Actor::append_effect_durations(Atts_vector& attributes) const {
    // Discard the previous serialized snapshot, never reserialize stale metadata.
    attributes.erase(std::remove_if(attributes.begin(), attributes.end(), [](const auto& a) {
        return std::string(a.first).compare(0, 13, "exult.effect.") == 0;
    }), attributes.end());
    bool has_effect = false;
    for (const auto& key : effect_keys)
        has_effect = has_effect || get_flag(key.flag) || temporary_effect_remaining(key.flag) >= 0;
    if (!has_effect) return;
    attributes.emplace_back(effect_version, 2);
    const auto epoch = Game_time::metadata_epoch();
    for (int i = 0; i < 4; ++i) attributes.emplace_back(effect_epoch[i], (epoch >> (16 * i)) & 65535);
    for (const auto& key : effect_keys) {
        const auto value = static_cast<uint32>(temporary_effect_remaining(key.flag) + 1);
        attributes.emplace_back(key.low, value & 65535);
        attributes.emplace_back(key.high, value >> 16);
    }
}
void Actor::restore_effect_durations() {
    // Legacy/unknown-version saves follow the unchanged normal reconstruction.
    if (!Game_time::save_metadata_valid() || get_attribute(effect_version) != 2) return;
    unsigned long long epoch = 0;
    for (int i = 0; i < 4; ++i) epoch |= (static_cast<unsigned long long>(get_attribute(effect_epoch[i])) & 65535) << (16 * i);
    if (epoch != Game_time::metadata_epoch()) return;
    for (const auto& key : effect_keys) {
        const uint32 value = (static_cast<uint32>(get_attribute(key.low)) & 65535)
            | ((static_cast<uint32>(get_attribute(key.high)) & 65535) << 16);
        // Current supported effects never exceed 120s. Reject corrupt metadata
        // instead of extending arbitrary flags for unbounded periods.
        if (value > 120001) continue;
        need_timers()->restore_remaining(key.flag, static_cast<int>(value) - 1);
    }
    // Consume the loaded snapshot. Saving computes fresh metadata from records.
    set_attribute(effect_version, 0);
}
void Npc_timer_list::restore_remaining(int flag, int milliseconds) {
    const auto actor = get_npc();
    if (!actor) return;
    auto restore = [&](auto& owner) {
        if (milliseconds < 0) { owner.reset(); return; }
        // Do not resurrect a cleared status or manufacture an effect from metadata.
        if (!owner || !actor->get_flag(flag)) return;
        auto* timer = owner.get();
        // The normal cached game tick is refreshed on return to the event loop.
        // Rebase at the actual end of loading, not at its earlier cached epoch.
        const auto now = static_cast<uint32>(SDL_GetTicks());
        timer->end_time = now + milliseconds;
        auto* queue = Game_window::get_instance()->get_tqueue();
        while (queue->remove(timer)) {}
        timer->handle_event(now, 0);
    };
    if (flag == Obj_flags::protection) restore(protection);
    else if (flag == Obj_flags::invisible) restore(invisibility);
    else {
        auto it = flags.find(flag);
        if (it != flags.end()) {
            if (milliseconds < 0) flags.erase(it);
            else restore(it->second);
        }
    }
}
