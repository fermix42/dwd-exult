// GPL-2.0-or-later. Semantic adapter: all operations run on the engine thread.
#include "cli_world.h"

#include "Astar.h"
#include "Gump_manager.h"
#include "actions.h"
#include "actors.h"
#include "chunks.h"
#include "combat.h"
#include "gamemap.h"
#include "gamewin.h"
#include "paths.h"
#include "schedule.h"
#include "shapeinf.h"
#include "ucmachine.h"

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
			int          points = 0, shape = 0;
			Game_object* weapon = nullptr;
			auto*        wi     = n->get_weapon(points, shape, weapon);
			out << ",\"npc\":" << n->get_npc_num() << ",\"health\":" << n->get_property(Actor::health)
				<< ",\"schedule\":" << int(n->get_schedule_type())
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
		out << '}';
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
