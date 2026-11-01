#include "game_time.h"
#include "actors.h"
#include "Audio.h"
#include "chunks.h"
#include "databuf.h"
#include "gameclk.h"
#include "gamewin.h"
#include "iregobjs.h"
#include "contain.h"
#include "ucsched.h"
#include "ucinternal.h"
#include "game.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <map>
#include <memory>
#include <sstream>
#include <random>
#include <vector>

namespace {
Time_queue calendar_queue;
bool plain_exhaustion_shape(int shape) {
    const auto type = ShapeID::get_info(shape).get_shape_class();
    return type == Shape_info::unusable || type == Shape_info::quality || type == Shape_info::quantity
        || type == Shape_info::has_hp || type == Shape_info::quality_flags;
}
struct Fuel : Time_sensitive {
    Game_object_weak owner;
    int lit, period, spent, quality, sound, rate;
    uint32 deadline;
    unsigned deliveries = 0;
    // This existing descriptor declares the original SI sconce completion:
    // 435 -> 481, retaining orientation and advancing frame 0/1 to spent 2/3.
    bool sconce_completion() const { return GAME_SI && lit == 435 && spent == 481; }
    bool valid() const {
        const auto o = owner.lock();
        return o && o->get_shapenum() == lit && o->get_quality() > 0
            && o->get_info().get_shape_class() == Shape_info::quality && plain_exhaustion_shape(spent)
            && (!sconce_completion() || (o->get_framenum() >= 0 && o->get_framenum() <= 1
                && o->get_framenum() + 2 < ShapeID(spent, 0).get_num_frames()));
    }
    void handle_event(unsigned long tick, uintptr) override;
};
std::map<const Game_object*, std::shared_ptr<Fuel>> events;
struct Pending { Game_object_weak owner; int period, spent, quality, sound, rate, lit; uint32 deadline; };
std::vector<Pending> restored;
bool delivering = false;
bool loading = true;
bool metadata_valid = false;
unsigned long long epoch = 0;
void prune() {
    for (auto it = events.begin(); it != events.end();) {
        if (!it->second->valid()) {
            calendar_queue.remove(it->second.get());
            it = events.erase(it);
        } else ++it;
    }
}
void install(const std::shared_ptr<Fuel>& f) {
    auto o = f->owner.lock();
    if (!o) return;
    Game_time::cancel(o.get());
    events[o.get()] = f;
    calendar_queue.add(f->deadline, f.get());
}
void Fuel::handle_event(unsigned long tick, uintptr) {
    auto o = owner.lock();
    if (!o || !valid()) { if (o) Game_time::cancel(o.get()); return; }
    const auto keep_alive = events.at(o.get());
    ++deliveries;
    // This is the entire execution capability. No Usecode invocation, actor
    // action, egg activation, world transition, input or script scheduling API.
    const int remaining = o->get_quality() - 1;
    o->set_quality(remaining);
    if (!remaining) {
        const int spent_frame = sconce_completion() ? o->get_framenum() + 2 : -1;
        if (auto* container = o->get_owner()) container->change_member_shape(o.get(), spent);
        else {
            auto* chunk = o->get_chunk();
            if (chunk) chunk->remove(o.get());
            o->set_shape(spent);
            if (chunk) chunk->add(o.get());
        }
        if (spent_frame >= 0) o->set_frame(spent_frame);
        if (quality >= 0) o->set_quality(quality);
        if (auto* actor = o->get_outermost()->as_actor()) actor->refigure_gear();
        if (sound >= 0) Audio::get_ptr()->play_sound_effect(sound);
        Game_window::get_instance()->get_clock()->reset_palette();
        Game_window::get_instance()->set_all_dirty();
        Game_time::cancel(o.get());
    } else {
        deadline = static_cast<uint32>(tick) + period * rate;
        calendar_queue.add(deadline, events.at(o.get()).get());
    }
}
}
uint32 Game_time::now() {
    const auto* c = Game_window::get_instance()->get_clock();
    return static_cast<uint32>(c->get_total_minutes() * ticks_per_minute + c->get_ticks());
}
bool Game_time::fuel(Game_object* owner, int period, int spent, int quality, int sound, int legacy_timer) {
    // A deliberately narrow object-local capability. Validate everything before
    // registration; it cannot accept a callback pointer, actor, container or egg.
    if (!owner || !dynamic_cast<Ireg_game_object*>(owner) || owner->as_actor() || owner->as_container()
        || owner->is_egg() || period < 1 || period > 3600 || spent < 0 || spent >= 1024
        || quality < -1 || quality > 255 || sound < -1 || sound > 255 || legacy_timer < 0 || legacy_timer > 65535 || owner->get_quality() <= 0) return false;
    if (owner->get_info().get_shape_class() != Shape_info::quality || !plain_exhaustion_shape(spent)) return false;
    if (GAME_SI && owner->get_shapenum() == 435 && spent == 481
        && (owner->get_framenum() < 0 || owner->get_framenum() > 1
            || owner->get_framenum() + 2 >= ShapeID(spent, 0).get_num_frames())) return false;
    long migrated_delay = -1;
    if (legacy_timer) Usecode_script::adopt_declared_timer(owner, legacy_timer, migrated_delay);
    prune();
    auto found = events.find(owner);
    if (found != events.end()) {
        const auto& f = found->second;
        if (f->lit == owner->get_shapenum() && f->period == period && f->spent == spent
            && f->quality == quality && f->sound == sound) return true; // idempotent ready/reinit
    }
    auto f = std::make_shared<Fuel>();
    f->owner = owner->weak_from_this(); f->lit = owner->get_shapenum();
    f->period = period; f->spent = spent; f->quality = quality; f->sound = sound;
    f->rate = Game_window::get_instance()->get_clock()->get_time_rate();
    f->deadline = now() + period * f->rate;
    if (migrated_delay >= 0) {
        const auto step = Game_window::get_instance()->get_std_delay();
        f->deadline = now() + static_cast<uint32>((migrated_delay + step - 1) / step) * f->rate;
    }
    install(f);
    return true;
}
bool Game_time::ensure_fuel(Game_object* owner) {
    // The built-in vanilla participant explicitly declares this small state
    // table. Respect normal patch precedence: arbitrary overriding mod code is
    // never invoked or presumed to implement this contract during materialization.
    auto* uc = dynamic_cast<Usecode_internal*>(Game_window::get_instance()->get_usecode());
    if (!owner || !uc || !uc->native_light_contract() || owner->get_quality() <= 0) return false;
    const int frame = owner->get_framenum();
    if (frame < 0 || frame >= owner->get_num_frames()) return false;
    const int sound = GAME_BG ? 106 : 65;
    auto ensure = [&](int spent, int quality) {
        if (has_event(owner)) {
            long ignored = -1;
            Usecode_script::adopt_declared_timer(owner, 0x600, ignored);
            return true; // Valid native identity/deadline always wins.
        }
        return fuel(owner, 50, spent, quality, sound, 0x600);
    };
    switch (owner->get_shapenum()) {
    case 701:
        if (frame != 0) return false;
        return ensure(595, 255);
    case 338:
        if (frame != 0) return false;
        return ensure(997, -1);
    case 435:
        if (frame > 1) return false;
        return ensure(GAME_BG ? 535 : 481, -1);
    default: return false;
    }
}
void Game_time::cancel(Game_object* owner) {
    auto it = events.find(owner);
    if (it == events.end()) return;
    calendar_queue.remove(it->second.get());
    events.erase(it);
}
bool Game_time::has_event(const Game_object* owner) {
    auto it = events.find(owner);
    return it != events.end() && it->second->valid();
}
uint32 Game_time::next(uint32 limit) {
    prune(); uint32 tick;
    return calendar_queue.next_deadline(tick) ? std::min(limit, std::max(now(), tick)) : limit;
}
bool Game_time::dispatch(uint32 cutoff, unsigned budget) {
    if (delivering) return false;
    struct Scope { Scope(){ delivering = true; } ~Scope(){ delivering = false; } } scope;
    prune();
    uint32 tick;
    while (calendar_queue.next_deadline(tick) && tick <= cutoff) {
        if (!budget--) return false;
        calendar_queue.activate_next(cutoff);
    }
    return true;
}
void Game_time::rate_changed(int previous, int current) {
    if (previous <= 0 || previous == current) return;
    const auto tick = now();
    for (auto& item : events) {
        auto& f = item.second;
        const auto left = f->deadline > tick ? f->deadline - tick : 0;
        calendar_queue.remove(f.get());
        f->deadline = tick + static_cast<uint32>((static_cast<unsigned long long>(left) * current + previous - 1) / previous);
        f->rate = current;
        calendar_queue.add(f->deadline, f.get());
    }
}
void Game_time::calendar_jump(uint32 previous) {
    // Legacy Usecode clock jumps (not Native Wait) retain their established
    // caller-owned rest/effect reconciliation. Preserve pending phase here so
    // e.g. Keyring's explicit sleep-time fuel reduction is not applied twice.
    const auto current = now();
    for (auto& item : events) {
        auto& f = item.second;
        const auto left = f->deadline > previous ? f->deadline - previous : 0;
        calendar_queue.remove(f.get()); f->deadline = current + left;
        calendar_queue.add(f->deadline, f.get());
    }
}
void Game_time::clear() {
    calendar_queue.clear(); events.clear(); restored.clear(); delivering = false; loading = true; metadata_valid = false; epoch = 0;
}
void Game_time::write(ODataSource* out, Game_object* owner) {
    auto it = events.find(owner);
    if (it == events.end() || !it->second->valid()) return;
    const auto& f = *it->second;
    // Optional length-delimited IREG extension: role=fuel, version=2. Old readers
    // skip/drop unknown records, so downgrade/resave cannot retain a stale event.
    out->write1(255); out->write1(5); out->write2(25);
    out->write1(2); out->write2(f.lit); out->write2(f.period); out->write2(f.spent);
    out->write2(f.quality + 1); out->write2(f.sound + 1); out->write2(f.rate); out->write4(f.deadline);
    const auto epoch = metadata_epoch();
    out->write4(static_cast<uint32>(epoch)); out->write4(static_cast<uint32>(epoch >> 32));
}
bool Game_time::read(IDataSource* in, Game_object* owner, int length) {
    if (!owner || !metadata_valid || length != 25 || in->read1() != 2) return false;
    Pending p;
    p.owner = owner->weak_from_this(); p.lit = in->read2(); p.period = in->read2(); p.spent = in->read2();
    p.quality = in->read2() - 1; p.sound = in->read2() - 1; p.rate = in->read2(); p.deadline = in->read4();
    const auto low = in->read4();
    const auto saved_epoch = static_cast<unsigned long long>(in->read4()) << 32 | low;
    if (!owner || saved_epoch != epoch || p.rate < 1 || p.rate > 65535 || p.lit != owner->get_shapenum()) return false;
    // Register using the exact same validation as an intrinsic. The deadline is
    // restored after readied Usecode so restart/halt cannot create a duplicate.
    if (!fuel(owner, p.period, p.spent, p.quality, p.sound)) return false;
    auto f = events.at(owner);
    calendar_queue.remove(f.get()); f->deadline = p.deadline; f->rate = p.rate;
    calendar_queue.add(f->deadline, f.get());
    if (loading) restored.push_back(p);
    return true;
}
void Game_time::finish_load() {
    loading = false;
    auto pending = std::move(restored); restored.clear();
    for (const auto& p : pending) {
        auto o = p.owner.lock();
        if (!o || o->get_shapenum() != p.lit) continue;
        auto it = events.find(o.get());
        if (it == events.end()) continue; // Respect explicit cancellation by readied Usecode.
        auto f = it->second;
        calendar_queue.remove(f.get());
        // The currently loaded participant owns its plan. Restore phase, never
        // overwrite a new mod's capabilities with its previous saved plan.
        if (f->period == p.period && f->rate == p.rate) f->deadline = p.deadline;
        else {
            const auto left = p.deadline > now() ? p.deadline - now() : 0;
            const auto old_period = static_cast<unsigned long long>(p.period) * p.rate;
            f->deadline = now() + static_cast<uint32>((left * static_cast<unsigned long long>(f->period) * f->rate + old_period - 1) / old_period);
        }
        calendar_queue.add(f->deadline, f.get());
    }
}
std::string Game_time::describe(const Game_object* owner) {
    std::ostringstream out; out << '['; bool comma = false;
    for (const auto& item : events) {
        const auto& f = *item.second;
        auto o = f.owner.lock();
        if (!o || !f.valid() || (owner && owner != o.get())) continue;
        if (comma) out << ','; comma = true;
        out << "{\"purpose\":\"fuel\",\"shape\":" << f.lit << ",\"quality\":" << o->get_quality()
            << ",\"deadline\":" << f.deadline << ",\"remaining_ticks\":" << (f.deadline > now() ? f.deadline - now() : 0)
            << ",\"period_clock_steps\":" << f.period << ",\"rate\":" << f.rate << ",\"deliveries\":" << f.deliveries << '}';
    }
    out << ']'; return out.str();
}

bool Game_time::save_metadata_valid() { return metadata_valid; }
unsigned long long Game_time::metadata_epoch() {
    if (!epoch) {
        // Save identity only, independent of the game's gameplay RNG. This is
        // not an authentication token or a per-script identity system.
        std::random_device source;
        epoch = static_cast<unsigned long long>(source()) << 32 | source();
        if (!epoch) epoch = 1;
    }
    return epoch;
}
void Game_time::accept_save_metadata(unsigned long long value) { epoch = value; metadata_valid = value != 0; }
