// GPL-2.0-or-later. Semantic adapter: all operations run on the engine thread.
#include "cli_world.h"

#include "Astar.h"
#include "Gump_manager.h"
#include "Gump.h"
#include "imagewin.h"
#include "actions.h"
#include "actors.h"
#include "chunks.h"
#include "combat.h"
#include "gamemap.h"
#include "gamewin.h"
#include "game_time.h"
#include "gameclk.h"
#include "databuf.h"
#include "paths.h"
#include "schedule.h"
#include "shapeinf.h"
#include "ucmachine.h"
#include "ucsched.h"
#include "tqueue.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <map>
#include <stdexcept>
#include <typeinfo>

namespace {
	std::map<int, Game_object_weak> refs;
	int                             next_ref = 1, world_map = -1;

	struct Movement {
		bool        active = false;
		Tile_coord  dest;
		uint64      deadline  = 0;
		std::string status    = "none";
		int         tolerance = 0;
	};

	Movement movement;

	// Restrict observations to existing loaded geometry; never load/guess unseen terrain.
	class Observed_client : public Actor_pathfinder_client {
		Tile_coord origin;

	public:
		mutable bool unknown = false;

		Observed_client(Actor* a, int distance) : Actor_pathfinder_client(a, distance), origin(a->get_tile()) {}

		int get_step_cost(const Tile_coord& from, Tile_coord& to) const override {
			if (to.distance_2d(origin) > 32
				|| !Game_window::get_instance()->get_map()->is_chunk_read(to.tx / c_tiles_per_chunk, to.ty / c_tiles_per_chunk)) {
				unknown = true;
				return -1;
			}
			return Actor_pathfinder_client::get_step_cost(from, to);
		}
	};

	int reference(Game_object* o) {
		if (!o) {
			return 0;
		}
		for (auto& item : refs) {
			if (item.second.lock().get() == o) {
				return item.first;
			}
		}
		int id   = next_ref++;
		refs[id] = o->weak_from_this();
		return id;
	}

	Game_object_shared resolve(int id) {
		auto it  = refs.find(id);
		auto obj = it == refs.end() ? Game_object_shared{} : it->second.lock();
		if (!obj || obj->get_outermost()->get_map_num() != Game_window::get_instance()->get_map()->get_num()) {
			throw std::runtime_error("stale_object_reference");
		}
		return obj;
	}

    Actor* party_inventory_actor(Game_object* object) {
        auto* member = object ? object->get_outermost()->as_actor() : nullptr;
        auto* avatar = Game_window::get_instance()->get_main_actor();
        return member && (member == avatar || member->is_in_party()) ? member : nullptr;
    }

	void player_ready(bool input) {
		auto* w = Game_window::get_instance();
		if (input || w->get_usecode()->in_usecode() || w->main_actor_dont_move() || !w->main_actor_can_act()
			|| w->get_gump_man()->gump_mode()) {
			throw std::runtime_error("player_control_owned");
		}
	}

	std::string tile(const Tile_coord& t) {
		return "{\"x\":" + std::to_string(t.tx) + ",\"y\":" + std::to_string(t.ty) + ",\"z\":" + std::to_string(t.tz) + "}";
	}

	Tile_coord destination(int x, int y, std::istringstream& in) {
		int z;
		if (!(in >> z) || x < 0 || x >= c_num_tiles || y < 0 || y >= c_num_tiles || z < 0 || z > 255) {
			throw std::runtime_error("invalid_world_tile");
		}
		Tile_coord t(x, y, z);
		if (t.distance_2d(Game_window::get_instance()->get_main_actor()->get_tile()) > 32) {
			throw std::runtime_error("query_radius_exceeds_32");
		}
		return t;
	}

	std::string describe(Game_object* o) {
		auto* w         = Game_window::get_instance();
		auto* a         = w->get_main_actor();
		auto& info      = o->get_info();
		auto  t         = o->get_tile();
		auto  foot      = o->get_footprint();
		auto  owner     = o->get_owner();
		bool  reachable = o->get_outermost()->get_map_num() == a->get_map_num() && Fast_pathfinder_client::is_grabable(a, o);
		auto  rect      = w->get_shape_rect(o);
		auto* hit       = owner ? nullptr : w->find_object(rect.x + rect.w / 2, rect.y + rect.h / 2);
		std::ostringstream out;
		out << "{\"ref\":" << reference(o) << ",\"shape\":" << o->get_shapenum() << ",\"frame\":" << o->get_framenum()
			<< ",\"class_id\":" << int(info.get_shape_class()) << ",\"type\":"
			<< cli_quote(
					   o->as_actor()       ? "actor"
					   : info.is_door()    ? "door"
					   : o->as_container() ? "container"
										   : "unknown")
			<< ",\"name\":" << cli_quote(o->get_name()) << ",\"tile\":" << tile(t)
			<< ",\"map\":" << o->get_outermost()->get_map_num() << ",\"chunk_x\":" << t.tx / c_tiles_per_chunk
			<< ",\"chunk_y\":" << t.ty / c_tiles_per_chunk << ",\"owner\":" << reference(owner)
			<< ",\"quantity\":" << o->get_quantity() << ",\"quality\":" << o->get_quality() << ",\"solid\":" << info.is_solid()
			<< ",\"height\":" << info.get_3d_height() << ",\"footprint\":{\"x\":" << foot.x << ",\"y\":" << foot.y
			<< ",\"w\":" << foot.w << ",\"h\":" << foot.h << "},\"closed_door\":" << (info.is_door() ? o->is_closed_door() : false)
			<< ",\"reachable\":" << reachable << ",\"hit_test_target\":" << (hit == o)
			<< ",\"interactable\":" << (!owner && hit == o && reachable) << ",\"distance\":" << a->distance(o);
		if (auto* n = o->as_actor()) {
            const auto* planned = n->find_schedule_at_time(w->get_clock()->get_hour() / 3);
            const auto* schedule = n->get_schedule();
			int          points = 0, shape = 0;
			Game_object* weapon = nullptr;
			auto*        wi     = n->get_weapon(points, shape, weapon);
			out << ",\"npc\":" << n->get_npc_num() << ",\"health\":" << n->get_property(Actor::health)
				<< ",\"schedule\":" << int(n->get_schedule_type())
                << ",\"scheduled_type_at_calendar\":" << (planned ? int(planned->get_type()) : -1)
                << ",\"effective_schedule\":" << (schedule ? schedule->get_actual_type(n) : n->get_schedule_type())
				<< ",\"action\":" << cli_quote(n->get_action() ? typeid(*n->get_action()).name() : "none")
				<< ",\"alignment\":" << n->get_effective_alignment() << ",\"hostile_to_avatar\":"
				<< Combat_schedule::is_enemy(a->get_effective_alignment(), n->get_effective_alignment())
				<< ",\"target\":" << reference(n->get_target())
				<< ",\"combat_schedule\":" << (n->get_schedule_type() == Schedule::combat)
				<< ",\"weapon_ref\":" << reference(weapon) << ",\"weapon_shape\":" << shape
				<< ",\"effective_range\":" << n->get_effective_range(wi)
				<< ",\"straight_path_to_avatar\":" << Fast_pathfinder_client::is_straight_path(n, a)
				<< ",\"line_of_fire_to_avatar\":" << Fast_pathfinder_client::is_straight_path(n, a)
				<< ",\"line_of_fire_method\":\"Fast_pathfinder_client::is_straight_path (native combat "
				   "predicate)\",\"target_validity\":null,\"flags\":[";
			bool sep = false;
			for (int f = 0; f <= 38; ++f) {
				if (n->get_flag(f)) {
					if (sep) {
						out << ',';
					}
					sep = true;
					out << f;
				}
			}
			out << "],\"equipment\":[";
			sep = false;
			for (int slot = 0; slot < 18; ++slot) {
				if (auto* obj = n->get_readied(slot)) {
					if (sep) {
						out << ',';
					}
					sep = true;
					out << "{\"slot\":" << slot << ",\"ref\":" << reference(obj) << ",\"shape\":" << obj->get_shapenum() << '}';
				}
			}
			out << ']';
		}
        out << ",\"game_time_events\":" << Game_time::describe(o) << ",\"scripts\":[";
        bool comma = false;
        Usecode_script* script = nullptr;
        while ((script = Usecode_script::find(o, script))) {
            if (comma) out << ',';
            comma = true;
            out << "{\"executable\":" << script->executable()
                << ",\"nohalt\":" << script->is_no_halt()
                << ",\"due_in_ms\":" << w->get_tqueue()->find_delay(script, SDL_GetTicks()) << '}';
        }
        out << "]}";
        return out.str();
	}

	void update_movement() {
		if (!movement.active) {
			return;
		}
		auto* a = Game_window::get_instance()->get_main_actor();
		if (a->get_tile().distance(movement.dest) <= movement.tolerance) {
			movement.status = "reached";
			movement.active = false;
		} else if (SDL_GetTicks() >= movement.deadline) {
			movement.status = "timed_out";
			movement.active = false;
		}    // Do not cancel a live native action.
		else if (!a->is_moving()) {
			movement.status = "interrupted";
			movement.active = false;
		}
	}
}    // namespace

void cli_world_reset() {
	refs.clear();
	movement  = {};
	world_map = -1;
}

void cli_world_boundary() {
	int current = Game_window::get_instance()->get_map()->get_num();
	if (world_map != current) {
		refs.clear();
		movement  = {};
		world_map = current;
	}
	update_movement();
}

std::string cli_world(int sequence, const std::string& op, int x, int y, std::istringstream& in, bool input) {
	auto* w = Game_window::get_instance();
	auto* a = w->get_main_actor();
    if (op == "test_stale_event_metadata") {
        auto o = resolve(x);
        if (input || !Game_time::save_metadata_valid() || !Game_time::has_event(o.get()))
            throw std::runtime_error("requires_loaded_real_game_time_event");
        std::ostringstream bytes(std::ios::binary);
        OStreamDataSource output(&bytes);
        Game_time::write(&output, o.get());
        auto packet = bytes.str();
        if (packet.size() != 29) throw std::runtime_error("unexpected_event_record");
        packet[21] ^= 1; // Header4 + epoch offset17; never alter live/save data.
        IBufferDataView input(reinterpret_cast<const unsigned char*>(packet.data() + 4), 25);
        const auto before = Game_time::describe(o.get());
        const bool accepted = Game_time::read(&input, o.get(), 25);
        auto unsafe = bytes.str();
        const auto actor_shape = a->get_shapenum();
        unsafe[9] = actor_shape & 255; unsafe[10] = (actor_shape >> 8) & 255;
        IBufferDataView invalid_target(reinterpret_cast<const unsigned char*>(unsafe.data() + 4), 25);
        const bool target_accepted = Game_time::read(&invalid_target, o.get(), 25);
        const bool actor_accepted = Game_time::fuel(a, 50, 595, 255, 106);
        return "\"category\":\"test_setup\",\"stale_record_accepted\":" + std::to_string(accepted)
            + ",\"unsafe_target_accepted\":" + std::to_string(target_accepted)
            + ",\"actor_receiver_accepted\":" + std::to_string(actor_accepted)
            + ",\"event_unchanged\":" + std::to_string(before == Game_time::describe(o.get()));
    }
    if (op == "test_remove_fuel_receiver") {
        player_ready(input);
        auto o = resolve(x);
        if (o->get_owner() || o->as_actor() || !Game_time::has_event(o.get()) || a->distance(o.get()) > 8)
            throw std::runtime_error("fixture_requires_local_registered_ground_receiver");
        const auto weak = o->weak_from_this();
        o->remove_this(); o.reset();
        Game_time::dispatch(Game_time::now());
        return "\"category\":\"test_setup\",\"synthetic_receiver_removal\":true,\"receiver_expired\":"
            + std::to_string(weak.expired()) + ",\"remaining_events\":" + Game_time::describe();
    }
    if (op == "drop_item") {
        if (input || w->get_usecode()->in_usecode() || w->main_actor_dont_move()
            || !w->main_actor_can_act() || w->get_gump_man()->modal_gump_mode())
            throw std::runtime_error("player_control_owned");
        auto o = resolve(x);
        int dx, dy;
        {
            auto* member = party_inventory_actor(o.get());
            if (!member || !member->can_act_charmed() || !o->get_owner() || a->distance(member) > 8)
                throw std::runtime_error("requires_nearby_party_inventory");
            int ty, tz; if (!(in >> ty >> tz)) throw std::runtime_error("missing_world_tile");
            Tile_coord dest(y, ty, tz);
            if (y < 0 || ty < 0 || y >= c_num_tiles || ty >= c_num_tiles || tz < 0 || tz > 255
                || a->get_tile().distance(dest) > 8) throw std::runtime_error("drop_outside_local_reach");
            w->get_shape_location(dest, dx, dy);
            if (w->get_gump_man()->find_gump(dx, dy)) throw std::runtime_error("drop_target_occluded");
        }
        auto* source = w->get_gump_man()->find_gump(o.get());
        int sx = -1, sy = -1;
        auto* win = w->get_win();
        for (int gy = win->get_start_y(); gy < win->get_start_y() + win->get_full_height() && sx < 0; ++gy) {
            for (int gx = win->get_start_x(); gx < win->get_start_x() + win->get_full_width(); ++gx) {
                auto* g = w->get_gump_man()->find_gump(gx, gy);
                if (o->get_owner()) {
                    int lx, ly;
                    if (!source || g != source || !w->get_gump_man()->map_game_to_gump(g, gx, gy, lx, ly)
                        || g->find_object(lx, ly) != o.get()) continue;
                } else if (g || w->find_object(gx, gy) != o.get()) continue;
                sx = gx; sy = gy; break;
            }
        }
        if (sx < 0) throw std::runtime_error("object_occluded_or_offscreen");
        cli_reply(sequence, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
        if (w->start_dragging(sx, sy)) { w->drag(dx, dy); w->drop_dragged(dx, dy, true); }
        return {};
    }
    if (op == "inspect_inventory") {
        Game_object_shared held;
        auto* container = static_cast<Container_game_object*>(a);
        if (x) {
            held = resolve(x);
            container = held->as_container();
        }
        if (!container) throw std::runtime_error("not_party_inventory");
        if (!party_inventory_actor(container)) throw std::runtime_error("not_party_inventory");
        Game_object_vector items;
        container->get_objects(items, c_any_shapenum, c_any_qual, c_any_framenum);
        std::ostringstream out;
        out << "\"category\":\"inspection\",\"objects\":[";
        bool comma = false;
        for (auto* item : items) {
            if (comma) out << ',';
            comma = true;
            out << describe(item);
        }
        out << ']';
        return out.str();
    }
    if (op == "ready_inventory") {
        if (input || w->get_usecode()->in_usecode() || w->main_actor_dont_move()
            || !w->main_actor_can_act() || w->get_gump_man()->modal_gump_mode())
            throw std::runtime_error("player_control_owned");
        auto o = resolve(x);
        auto* member = party_inventory_actor(o.get());
        if (!member || !member->can_act_charmed() || !o->get_owner() || a->distance(member) > 8
            || member->find_readied(o.get()) >= 0) throw std::runtime_error("requires_unreadied_party_inventory_item");
        if (y < 0 || y >= 18 || member->get_readied(y)) throw std::runtime_error("requires_empty_ready_slot");
        if (!w->get_gump_man()->find_gump(o.get())) throw std::runtime_error("inventory_not_visible");
        auto* owner = o->get_owner();
        const auto tx = o->get_tx(), ty = o->get_ty();
        Game_object_shared hold;
        o->remove_this(&hold);
        // The ordinary native Gump equipment operation, including fit checks
        // and real readied Usecode. Never force a slot or create an item.
        if (!member->add_readied(o.get(), y)) {
            owner->add(o.get(), true, false);
            o->set_shape_pos(tx, ty);
            throw std::runtime_error("native_ready_refused");
        }
        w->set_all_dirty();
        return "\"category\":\"gameplay\",\"readied\":true";
    }
    if (op == "interact_inventory") {
        if (input || w->get_usecode()->in_usecode() || w->main_actor_dont_move()
            || !w->main_actor_can_act() || w->get_gump_man()->modal_gump_mode())
            throw std::runtime_error("player_control_owned");
        auto o = resolve(x);
        auto* member = party_inventory_actor(o.get());
        if (!member || !member->can_act_charmed() || !o->get_owner() || a->distance(member) > 8)
            throw std::runtime_error("requires_nearby_party_inventory");
        auto* g = w->get_gump_man()->find_gump(o.get());
        if (!g) throw std::runtime_error("inventory_not_visible");
        // Semantic hit testing in the actual visible native Gump. Never activate
        // an invisible contained object by bypassing its ordinary interaction path.
        auto* win = w->get_win();
        for (int gy = win->get_start_y(); gy < win->get_start_y() + win->get_full_height(); ++gy) {
            for (int gx = win->get_start_x(); gx < win->get_start_x() + win->get_full_width(); ++gx) {
                if (w->get_gump_man()->find_gump(gx, gy) != g) continue;
                int lx, ly;
                if (!w->get_gump_man()->map_game_to_gump(g, gx, gy, lx, ly) || g->find_object(lx, ly) != o.get()) continue;
                cli_reply(sequence, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
                w->double_clicked(gx, gy);
                return {};
            }
        }
        throw std::runtime_error("inventory_object_occluded");
    }
	if (op == "inspect_object" || op == "inspect_actor") {
		auto o = resolve(x);
		if (op == "inspect_actor" && !o->as_actor()) {
			throw std::runtime_error("reference_is_not_actor");
		}
		return "\"category\":\"inspection\",\"object\":" + describe(o.get());
	}
	if (op == "find_objects" || op == "find_actors" || op == "inspect_world") {
		int limit = y, offset = 0, shape = c_any_shapenum;
		in >> offset >> shape;
		if (x < 0 || x > 32 || limit < 1 || limit > 200 || offset < 0 || offset > 10000) {
			throw std::runtime_error("invalid_query_bounds");
		}
		Game_object_vector objects;
		a->find_nearby(objects, shape, x, 0, c_any_qual);
		if (op == "find_actors") {
			objects.erase(
					std::remove_if(
							objects.begin(), objects.end(),
							[](auto* o) {
								return !o->as_actor();
							}),
					objects.end());
		}
		std::ostringstream out;
		out << "\"category\":\"inspection\",\"map\":" << a->get_map_num() << ",\"avatar\":" << reference(a)
			<< ",\"total\":" << objects.size() << ",\"coverage\":\"loaded_chunks_only\",\"offset\":" << offset << ",\"objects\":[";
		bool sep = false;
		for (int i = offset; i < int(objects.size()) && i < offset + limit; ++i) {
			if (sep) {
				out << ',';
			}
			sep = true;
			out << describe(objects[i]);
		}
		out << ']';
		return out.str();
	}
	if (op == "inspect_tile" || op == "path_query" || op == "move_to") {
		auto dest = destination(x, y, in);
		if (!w->get_map()->is_chunk_read(x / c_tiles_per_chunk, y / c_tiles_per_chunk)) {
			throw std::runtime_error("world_region_not_loaded");
		}
		auto  target = dest;
		auto& info   = a->get_info();
		int   xt = info.get_3d_xtiles(a->get_framenum()), yt = info.get_3d_ytiles(a->get_framenum()), lift = dest.tz;
		bool  blocked = Map_chunk::is_blocked(
                info.get_3d_height(), dest.tz, FIX_COORD(x + 1 - xt), FIX_COORD(y + 1 - yt), xt, yt, lift, a->get_type_flags(), 1);
		target.tz      = lift;
		auto* obstacle = Game_object::find_blocking(dest);
		auto* chunk    = w->get_map()->get_chunk(x / c_tiles_per_chunk, y / c_tiles_per_chunk);
		auto  flat     = chunk->get_flat(x % c_tiles_per_chunk, y % c_tiles_per_chunk);
		if (op == "inspect_tile") {
			return "\"category\":\"inspection\",\"tile\":" + tile(dest) + ",\"resolved_tile\":" + tile(target) + ",\"blocked\":"
				   + std::to_string(blocked) + ",\"obstacle\":" + std::to_string(reference(obstacle)) + ",\"terrain_shape\":"
				   + std::to_string(flat.get_shapenum()) + ",\"terrain_frame\":" + std::to_string(flat.get_framenum());
		}
		Observed_client client(a, 0);
		Astar           path;
		bool            found = path.NewPath(a->get_tile(), dest, &client);
		if (op == "path_query") {
			return "\"category\":\"inspection\",\"found\":" + std::to_string(found)
				   + ",\"blocked_destination\":" + std::to_string(blocked) + ",\"obstacle\":" + std::to_string(reference(obstacle))
				   + ",\"steps\":" + std::to_string(found ? path.get_num_steps() : 0)
				   + ",\"bounded_search\":true,\"unknown_region_encountered\":" + std::to_string(client.unknown);
		}
		player_ready(input);
		int timeout = 10000;
		if (in >> timeout) {
		}
		if (timeout < 1 || timeout > 120000) {
			throw std::runtime_error("invalid_movement_timeout");
		}
		movement.tolerance = 0;
		movement.dest      = dest;
		movement.deadline  = SDL_GetTicks() + timeout;
		movement.active    = false;
		movement.status    = "blocked";
		if (a->get_tile() == dest) {
			movement.status = "reached";
		} else if (found && a->walk_path_to_tile(dest, w->get_std_delay(), 0, 0)) {
			movement.active = true;
			movement.status = "moving";
			update_movement();
		}
		return "\"category\":\"gameplay\",\"movement\":" + cli_quote(movement.status) + ",\"destination\":" + tile(dest);
	}
	if (op == "movement_status") {
		update_movement();
		return "\"category\":\"inspection\",\"movement\":" + cli_quote(movement.status) + ",\"destination\":" + tile(movement.dest)
			   + ",\"actual\":" + tile(a->get_tile());
	}
	if (op == "move_to_object") {
		player_ready(input);
		auto o = resolve(x);
		if (o->get_owner() || a->distance(o.get()) > 32) {
			throw std::runtime_error("object_not_in_local_world");
		}
		auto            dest = o->get_tile();
		Observed_client client(a, 1);
		Astar           path;
		bool            found = path.NewPath(a->get_tile(), dest, &client);
		movement.dest         = dest;
		movement.tolerance    = 1;
		movement.deadline     = SDL_GetTicks() + 10000;
		movement.active       = false;
		movement.status       = "blocked";
		if (a->get_tile().distance(dest) <= 1) {
			movement.status = "reached";
		} else if (found && a->walk_path_to_tile(dest, w->get_std_delay(), 0, 1)) {
			movement.active = true;
			movement.status = "moving";
			update_movement();
		}
		return "\"category\":\"gameplay\",\"movement\":" + cli_quote(movement.status) + ",\"destination\":" + tile(dest);
	}
	if (op == "interact_object") {
		player_ready(input);
		auto o = resolve(x);
		if (o->get_owner()) {
			throw std::runtime_error("contained_object_requires_visible_inventory_control");
		}
		if (!Fast_pathfinder_client::is_grabable(a, o.get())) {
			throw std::runtime_error("object_unreachable");
		}
		auto r  = w->get_shape_rect(o.get());
		int  sx = r.x + r.w / 2, sy = r.y + r.h / 2;
		if (w->find_object(sx, sy) != o.get()) {
			throw std::runtime_error("object_occluded_or_offscreen");
		}
		cli_reply(sequence, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
		w->double_clicked(sx, sy);
		return {};
	}
	throw std::runtime_error("unknown_command");
}
