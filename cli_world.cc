// GPL-2.0-or-later. Semantic adapter: all operations run on the engine thread.
#include "cli_world.h"
#include "native_case_breadcrumbs.h"
#include "cli_driver.h"
#include "mouse.h"

#include "Astar.h"
#include "Gump_manager.h"
#include "Gump.h"
#include "imagewin.h"
#include "actions.h"
#include "schedule.h"
#include "actors.h"
#include "chunks.h"
#include "combat.h"
#include "gamemap.h"
#include "gamewin.h"
#include "game.h"
#include "game_time.h"
#include "native_case_commission.h"
#include "gameclk.h"
#include "databuf.h"
#include "paths.h"
#include "schedule.h"
#include "shapeinf.h"
#include "ucmachine.h"
#include "ucsched.h"
#include "tqueue.h"
#include "native_virtue_case.h"
#include "virstone.h"
#include "spellbook.h"
#include "Spellbook_gump.h"
#include "egg.h"
#include "effects.h"
#include "ready.h"

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
			<< ",\"sprite\":{\"width\":" << (o->get_shape() ? o->get_shape()->get_width() : 0)
			<< ",\"height\":" << (o->get_shape() ? o->get_shape()->get_height() : 0)
			<< ",\"xleft\":" << (o->get_shape() ? o->get_shape()->get_xleft() : 0)
			<< ",\"yabove\":" << (o->get_shape() ? o->get_shape()->get_yabove() : 0) << "}"
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
		out << ",\"gump_tx\":" << o->get_tx() << ",\"gump_ty\":" << o->get_ty();
		if (auto* stone = o->as_virtstone())
			out << ",\"stone_target\":" << tile(stone->get_target_pos()) << ",\"stone_target_map\":" << stone->get_target_map();
		if (auto* n = o->as_actor()) {
            const auto* planned = n->find_schedule_at_time(w->get_clock()->get_hour() / 3);
            const auto* schedule = n->get_schedule();
			int          points = 0, shape = 0;
			Game_object* weapon = nullptr;
			auto*        wi     = n->get_weapon(points, shape, weapon);
			out << ",\"food\":" << n->get_property(Actor::food_level) << ",\"npc\":" << n->get_npc_num() << ",\"health\":" << n->get_property(Actor::health)
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
        if(Virtue_case::is_case(o)) {
            out << ",\"case_labels\":[";
            for(int i=0;i<8;++i){if(i)out<<',';out<<cli_quote(o->as_container()->get_case_note(i));}out<<']';
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
    if (op == "test_jawbone_items") {
        player_ready(input);
        if (!GAME_SI || a->find_item(555, c_any_qual, c_any_framenum))
            throw std::runtime_error("fixture_requires_SI_without_jawbone");
        auto* pack = a->get_readied(backpack);
        auto* container = pack ? pack->as_container() : nullptr;
        if (!container) throw std::runtime_error("fixture_requires_backpack");
        auto jawbone = w->get_map()->create_ireg_object(555, 0);
        auto tooth = w->get_map()->create_ireg_object(559, 0);
        jawbone->set_flag(Obj_flags::okay_to_take);
        tooth->set_flag(Obj_flags::okay_to_take);
        if (!jawbone->as_container() || !container->has_room(jawbone.get())
            || !container->add(jawbone.get(), false, false) || !container->add(tooth.get(), false, false))
            throw std::runtime_error("jawbone_fixture_does_not_fit");
        w->set_all_dirty();
        return "\"category\":\"test_setup\",\"synthetic_jawbone_items\":true,\"jawbone\":"
            + describe(jawbone.get()) + ",\"tooth\":" + describe(tooth.get()) + ",\"backpack\":" + describe(container);
    }
    if (op == "test_case_extra") {
        player_ready(input);
        if(!GAME_BG)throw std::runtime_error("BG_only_case_fixture");
        auto object=w->get_map()->create_ireg_object(Virtue_case::shape,0);
        if(!Virtue_case::is_case(object.get()) || !a->add(object.get(),true,false))throw std::runtime_error("case_fixture_unavailable");
        object->set_flag(Obj_flags::okay_to_take);
        return "\"category\":\"test_setup\",\"synthetic_case\":"+describe(object.get());
    }
    if (op == "spellbook_cast") {
        if(input || w->get_usecode()->in_usecode() || y<0 || y>=72 || Virtue_case::editing_slot()>=0)throw std::runtime_error("spellbook_cast_unavailable");
        auto object=resolve(x); Spellbook_gump* book_gump=nullptr;
        for(auto* g:*w->get_gump_man()) if(g->get_owner()==object.get())book_gump=dynamic_cast<Spellbook_gump*>(g);
        if(!book_gump)throw std::runtime_error("spellbook_not_open");
        cli_reply(sequence,true,"\"category\":\"gameplay\",\"status\":\"dispatched\"");
        book_gump->do_spell(y);return {};
    }
    if (op == "test_case_spell_setup") {
        player_ready(input);
        if (!GAME_BG || x<0 || x>6) throw std::runtime_error("invalid_case_spell_fixture");
        Game_object_vector objects; a->get_objects(objects, c_any_shapenum, c_any_qual, c_any_framenum);
        Spellbook_object* book = nullptr;
        for (auto* object : objects) if (auto* b=object->as_spellbook()) { book=b; break; }
        if (!book) {
            auto object=w->get_map()->create_ireg_object(761,0); book=object->as_spellbook();
            if (!book || !a->add(book,true,false)) throw std::runtime_error("fixture_book_unavailable");
        }
        book->add_spell(34); book->add_spell(36);
        if (x==3) { book->remove_spell(34); book->remove_spell(36); }
        Egg_object::set_weather(x==6?3:0);
        a->set_property(Actor::exp,x==5?0:6400); a->set_property(Actor::magic,30); a->set_property(Actor::mana,x==1?0:30);
        a->remove_quantity(10000,842,c_any_qual,c_any_framenum);
        for (int frame=0; frame<8; ++frame) {
            if (x==2 && frame==3) continue;
            auto reagent=w->get_map()->create_ireg_object(842,frame); reagent->modify_quantity(19);
            if (!a->add(reagent.get(),true,false)) throw std::runtime_error("fixture_reagent_does_not_fit");
        }
        if (x==4) book->remove_this();
        return "\"category\":\"test_setup\",\"synthetic_spell_inventory_and_stats\":true";
    }
    if (op == "test_case_commission_start") {
        player_ready(input);
        if(!GAME_BG)throw std::runtime_error("fixture_requires_BG");
        // Explicitly owner-approved isolated Minoc starting fixture, not travel evidence.
        a->move(1605,405,0,0);w->center_view(a->get_tile());w->set_all_dirty();
        return "\"category\":\"test_setup\",\"synthetic_Minoc_start\":true";
    }
    if (op == "test_case_commission_items") {
        player_ready(input);
        if(!GAME_BG || x<0 || x>5)throw std::runtime_error("invalid_commission_item_fixture");
        Actor* group[9];const int n=w->get_party(group,1);
        for(int i=0;i<n;++i) for(int shape:{330,644,851})group[i]->remove_quantity(100000,shape,c_any_qual,c_any_framenum);
        auto add=[&](Container_game_object* owner,int shape,int frame,int quantity) {
            auto item=w->get_map()->create_ireg_object(shape,frame);item->set_flag(Obj_flags::okay_to_take);
            if(quantity>1)item->modify_quantity(quantity-1);
            if(!owner->add(item.get(),true,false))throw std::runtime_error("fixture_item_does_not_fit");
        };
        if(x) {
            Container_game_object* owner=x==2 && n>1?group[1]:a;
            if(x==3) {
                auto bag=w->get_map()->create_ireg_object(801,0);owner->add(bag.get(),true,false);owner=bag->as_container();
            }
            add(owner,330,2,1);
            if(x!=4)for(int i=0;i<5;++i)add(a,644,0,100);
            if(x!=5) {add(owner,851,0,1);add(owner,851,0,1);}
        }
        w->set_all_dirty();return "\"category\":\"test_setup\",\"synthetic_commission_items\":true";
    }
    if (op == "test_case_commission_food") {
        player_ready(input);if(!GAME_BG)throw std::runtime_error("fixture_requires_BG");
        for(int i=0;i<8;++i) {
            auto food=w->get_map()->create_ireg_object(377,8);food->set_flag(Obj_flags::okay_to_take);
            if(!a->add(food.get(),true,false))throw std::runtime_error("food_fixture_does_not_fit");
        }
        return "\"category\":\"test_setup\",\"synthetic_food_inventory\":true";
    }
	if (op == "text_effects") {
		auto rect = [](const TileRect& r) {
			return "{\"x\":" + std::to_string(r.x) + ",\"y\":" + std::to_string(r.y) + ",\"w\":" + std::to_string(r.w)
				   + ",\"h\":" + std::to_string(r.h) + "}";
		};
		std::ostringstream out;
		out << "\"category\":\"inspection\",\"effects\":[";
		bool comma = false;
		for (const auto& effect : w->get_effects()->get_text_effects()) {
			if (comma) {
				out << ',';
			}
			comma            = true;
			const auto& l    = effect->get_layout();
			auto        item = effect->get_item();
			out << "{\"text\":" << cli_quote(effect->get_message()) << ",\"item\":" << (item ? reference(item.get()) : 0)
				<< ",\"lifetime_ticks\":" << effect->get_lifetime_ticks() << ",\"elapsed_ticks\":" << effect->get_elapsed_ticks()
				<< ",\"viewport\":" << rect(l.viewport) << ",\"anchor\":" << rect(l.anchor)
				<< ",\"destination\":" << rect(l.destination) << ",\"scale\":" << l.scale << ",\"width\":" << l.width
				<< ",\"height\":" << l.height << ",\"line_height\":" << l.line_height << ",\"page\":" << l.page
				<< ",\"pages\":" << l.pages << ",\"lines_per_page\":" << l.lines_per_page << ",\"lines\":[";
			for (size_t i = 0; i < l.lines.size(); ++i) {
				if (i) {
					out << ',';
				}
				out << cli_quote(l.lines[i]);
			}
			out << "],\"line_widths\":[";
			for (size_t i = 0; i < l.line_widths.size(); ++i) {
				if (i) {
					out << ',';
				}
				out << l.line_widths[i];
			}
			out << "]}";
		}
		out << ']';
		return out.str();
	}
	if (op == "test_overhead") {
		player_ready(input);
		auto object = resolve(x);
		int  edge;
		in >> edge;
		if (!object->as_actor() || edge < 0 || edge > 7 || y < -2 || y > 8) {
			throw std::runtime_error("invalid_overhead_fixture");
		}
		if (y == -2) {
			w->get_effects()->remove_text_effects();
			w->center_view(a->get_tile());
			return "\"category\":\"test_setup\",\"speech_cleared\":true";
		}
		auto      center = object->get_tile();
		const int halfw = w->get_width() / (2 * c_tilesize), halfh = w->get_height() / (2 * c_tilesize);
		if (edge == 1 || edge == 6) {
			center.tx += halfw - 2;
		}
		if (edge == 2 || edge == 7) {
			center.tx -= halfw - 2;
		}
		if (edge == 3 || edge == 6) {
			center.ty += halfh - 2;
		}
		if (edge == 4 || edge == 7) {
			center.ty -= halfh - 2;
		}
		if (edge == 5) {
			center.tx += 3;
			center.ty += 2;
		}
		w->center_view(center);
		w->get_map()->read_map_data();
		w->set_all_dirty();
		// No actor movement, flags, party membership or schedule changes.
		if (y >= 0) {
			static const char* samples[] = {
					"@'Tis horrible!@",
					"These stones are quite a collection, my friend. Finding one in our packs is becoming an adventure of its own!",
					"We are gathering quite a collection of stones. I hope we can still find what we need in our packs.",
					"I hope thou rememberest where each of these stones will take us. I would rather not find out by surprise.",
					"Which stone takes us where, Avatar? I should hate to arrive somewhere unexpected when I meant to find an inn.",
					"First line.\nSecond line.\n\nFinal paragraph."};
			std::string text;
			if (y < 6) {
				text = samples[y];
			} else if (y == 6) {
				text = std::string(200, 'W') + "!";
			} else if (y == 7) {
				for (int i = 0; i < 90; ++i) {
					text += "Line " + std::to_string(i) + ": all words remain visible.\n";
				}
			}
			w->get_effects()->remove_text_effects();
			object->say(text.c_str());
		}
		return "\"category\":\"test_setup\",\"synthetic_speech\":true,\"camera_only_fixture\":true";
	}
	if(op=="test_case_companion_start") {
        player_ready(input);if(!GAME_BG || (x!=1 && x!=3 && x!=4))throw std::runtime_error("invalid_companion_fixture");
        auto* npc=w->get_npc(x);if(npc->get_map_num()!=a->get_map_num())throw std::runtime_error("companion_fixture_map_mismatch");
        w->center_view(npc->get_tile());w->get_map()->read_map_data();
        Tile_coord target=npc->get_tile();target.tx+=2;target.ty+=2;target.tz=0;
        target=Map_chunk::find_spot(target,3,a);
        if(target.tx<0)throw std::runtime_error("no_companion_fixture_start");
        a->move(target.tx,target.ty,target.tz,npc->get_map_num());w->center_view(a->get_tile());w->set_all_dirty();
        return "\"category\":\"test_setup\",\"approved_Avatar_only_companion_start\":true";
    }
    if(op=="test_case_breadcrumb_items") {
        player_ready(input);if(!GAME_BG || x<0 || x>3)throw std::runtime_error("invalid_breadcrumb_inventory_fixture");
        if(x!=1 && x!=3) {
            Actor* group[9];const int n=w->get_party(group,1);
            for(int i=0;i<n;++i)group[i]->remove_quantity(10000,330,c_any_qual,c_any_framenum);
            for(int frame:{2,x==2?2:0}) {
                auto stone=w->get_map()->create_ireg_object(330,frame);stone->set_flag(Obj_flags::okay_to_take);
                if(!a->add(stone.get(),true,false))throw std::runtime_error("stone_fixture_does_not_fit");
            }
            return "\"category\":\"test_setup\",\"synthetic_unmarked_inventory\":true";
        }
        Tile_coord proposed=a->get_tile();proposed.tx+=4;proposed.ty+=3;
        Tile_coord spot=Map_chunk::find_spot(proposed,2,330,1);
        if(spot.tx<0)throw std::runtime_error("no_stone_fixture_ground");
        auto stone=w->get_map()->create_ireg_object(330,x==3?2:1);stone->set_flag(Obj_flags::okay_to_take);stone->move(spot);
        return "\"category\":\"test_setup\",\"synthetic_world_stone\":"+describe(stone.get());
    }
    if(op=="case_breadcrumbs_state") {
        std::string companions="[";
        for(int id:{1,3,4}) {if(id!=1)companions+=",";companions+=describe(w->get_npc(id));}
        return "\"category\":\"inspection\",\"breadcrumbs\":"+Case_breadcrumbs::inspect()+",\"companions\":"+companions+"]";
    }
    if (op == "case_commission_state") {
        std::ostringstream schedules;schedules<<"[";bool comma=false;
        if(auto* list=w->get_npc(86)->get_schedules())for(const auto& entry:*list) {
            if(comma)schedules<<",";comma=true;
            schedules<<"{\"hour\":"<<entry.get_time()*3<<",\"type\":"<<entry.get_type()<<",\"tile\":"<<tile(entry.get_pos())<<"}";
        }
        schedules<<"]";
        Actor* group[9];const int count=w->get_party(group,1);std::string members="[";
        for(int i=0;i<count;++i){if(i)members+=",";members+=describe(group[i]);}members+="]";
        return "\"category\":\"inspection\",\"commission\":"+Case_commission::inspect()
            +",\"xanthia\":"+describe(w->get_npc(86))+",\"william\":"+describe(w->get_npc(93))+",\"xanthia_schedule\":"+schedules.str()+",\"party\":"+members;
    }
    if (op == "case_spell_state") {
        std::ostringstream out;
        out << "\"category\":\"inspection\",\"weather\":" << w->get_effects()->get_weather() << ",\"actor_casting\":" << a->get_casting_mode()
            << ",\"mana\":" << a->get_property(Actor::mana)
            << ",\"magic\":" << a->get_property(Actor::magic) << ",\"level\":" << a->get_level()
            << ",\"casting\":" << Virtue_case::casting_active() << ",\"targeting\":" << Virtue_case::targeting_active()
            << ",\"reagents\":[";
        for (int i=0;i<8;++i) { if(i)out<<',';out<<a->count_objects(842,c_any_qual,i); }
        out<<']'; return out.str();
    }
    if (op == "test_virtue_case_labels") {
        if (!GAME_BG || input) throw std::runtime_error("case_label_preview_requires_BG");
        Virtue_case::preview_labels_for_test();
        return "\"category\":\"test_setup\",\"synthetic_label_preview\":true";
    }
    if (op == "test_virtue_case") {
        player_ready(input);
        if (!GAME_BG || a->find_item(Virtue_case::shape, c_any_qual, c_any_framenum))
            throw std::runtime_error("fixture_requires_BG_without_existing_case");
        auto case_item = w->get_map()->create_ireg_object(Virtue_case::shape, 0);
        if (!Virtue_case::is_case(case_item.get()) || !case_item->as_container())
            throw std::runtime_error("native_case_shape_unavailable");
        // A normal backpack can hold nine stones (90 volume) plus the coin.
        // A small bag holds only 70; never overfill a fixture then call its
        // legitimate refusal to take a stone back a case regression.
        auto bag = w->get_map()->create_ireg_object(801, 0);
        case_item->set_flag(Obj_flags::okay_to_take);
        bag->set_flag(Obj_flags::okay_to_take);
        if (!bag->as_container()) throw std::runtime_error("fixture_bag_unavailable");
        for (int frame = 0; frame < 9; ++frame) {
            auto object = w->get_map()->create_ireg_object(330, frame % 8);
            object->set_flag(Obj_flags::okay_to_take);
            auto* stone = object->as_virtstone();
            if (!stone) throw std::runtime_error("fixture_stone_unavailable");
            // Explicit synthetic pre-marked UNIT fixture, never a spell test.
            stone->set_target_pos(Tile_coord(1000+frame, 1100+frame, frame % 4));
            stone->set_target_map(frame % 2); // Exercise persistence of nonzero map data, not teleportation.
            if (!bag->as_container()->add(object.get(), true, false)) throw std::runtime_error("fixture_add_failed");
        }
        auto gold = w->get_map()->create_ireg_object(644, 0);
        gold->set_flag(Obj_flags::okay_to_take);
        const int hand = !a->get_readied(lhand) ? lhand : !a->get_readied(rhand) ? rhand : -1;
        if (hand < 0) throw std::runtime_error("fixture_requires_free_hand");
        if (!bag->as_container()->add(gold.get(), false, false)
            || !a->add_readied(bag.get(), hand) || !a->add(case_item.get(), false, false))
            throw std::runtime_error("fixture_add_failed");
        w->set_all_dirty();
        return "\"category\":\"test_setup\",\"synthetic_physical_case_fixture\":true,\"case\":"
            + describe(case_item.get()) + ",\"source_bag\":" + describe(bag.get());
    }
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
    if (op == "target_object" || op == "target_tile" || op == "target_at" || op == "aim_object") {
        if (!input || cli_input_paint || Mouse::mouse()->get_shape() != Mouse::greenselect)
            throw std::runtime_error("target_input_not_active");
        int gx,gy;
        if(op=="target_object" || op=="aim_object") {
            auto object=resolve(x);
            if (object->get_owner() || (object->as_actor() && w->get_gump_man()->find_gump(object.get(),c_any_shapenum))) {
                gx=gy=-1;
                auto* win=w->get_win();
                for(int py=win->get_start_y();py<win->get_start_y()+win->get_full_height() && gx<0;++py)
                    for(int px=win->get_start_x();px<win->get_start_x()+win->get_full_width();++px) {
                        auto* g=w->get_gump_man()->find_gump(px,py);int lx,ly;
                        if(g && w->get_gump_man()->map_game_to_gump(g,px,py,lx,ly) && (g->find_object(lx,ly)==object.get() || (!g->find_object(lx,ly) && g->find_actor(lx,ly)==object.get()))) {
                            gx=px;gy=py;break;
                        }
                    }
                if(gx<0)throw std::runtime_error("target_not_visible");
            } else {
                if(object->get_map_num()!=a->get_map_num())throw std::runtime_error("target_wrong_map");
                const auto rect=w->get_shape_rect(object.get());gx=rect.x+rect.w/2;gy=rect.y+rect.h/2;
                if(w->find_object(gx,gy)!=object.get())throw std::runtime_error("target_occluded");
            }
        } else if (op=="target_at") {
            gx=x;gy=y;
        } else {
            const auto dest=destination(x,y,in);
            w->get_shape_location(dest,gx,gy);
            if(gx<0 || gy<0 || gx>=w->get_width() || gy>=w->get_height())
                throw std::runtime_error("target_not_visible");
            if(w->find_object(gx,gy)) throw std::runtime_error("target_tile_contains_object");
        }
        if(gx<0 || gy<0 || gx>=w->get_width() || gy>=w->get_height() || (op=="target_tile" && w->get_gump_man()->find_gump(gx,gy)))
            throw std::runtime_error("target_not_visible");
        if(op=="aim_object") {
            Mouse::mouse()->move(gx,gy);Mouse::mouse()->show();w->set_all_dirty();
            return "\"category\":\"inspection\",\"native_cursor_positioned\":true";
        }
        int sx,sy;
        w->get_win()->game_to_screen(gx,gy,w->get_fastmouse(),sx,sy);
        SDL_Event event{};
        event.type=SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.button=1;event.button.x=sx;event.button.y=sy;
        event.button.windowID=SDL_GetWindowID(w->get_win()->get_screen_window());
        if(!SDL_PushEvent(&event)) throw std::runtime_error("target_event_refused");
        cli_reply(sequence,true,"\"category\":\"gameplay\",\"status\":\"dispatched\"");
        return {};
    }
    if (op == "move_gump") {
        if (input || w->get_usecode()->in_usecode() || w->get_gump_man()->modal_gump_mode())
            throw std::runtime_error("player_control_owned");
        auto owner = resolve(x);
        int target_y;
        if (!(in >> target_y)) throw std::runtime_error("missing_gump_position");
        Gump* target = nullptr;
        for (auto* g : *w->get_gump_man()) if (g->get_owner() == owner.get()) target = g;
        if (!target) throw std::runtime_error("gump_not_open");
        auto* win = w->get_win();
        int sx=-1,sy=-1;
        for (int gy=win->get_start_y();gy<win->get_start_y()+win->get_full_height() && sx<0;++gy)
            for (int gx=win->get_start_x();gx<win->get_start_x()+win->get_full_width();++gx) {
                if (w->get_gump_man()->find_gump(gx,gy)!=target) continue;
                int lx,ly;
                if (!w->get_gump_man()->map_game_to_gump(target,gx,gy,lx,ly)
                    || target->on_button(lx,ly) || target->find_object(lx,ly)) continue;
                sx=gx;sy=gy;break;
            }
        if (sx<0) throw std::runtime_error("gump_background_not_visible");
        const int dx=sx+y-target->get_x(),dy=sy+target_y-target->get_y();
        if (!w->start_dragging(sx,sy)) throw std::runtime_error("native_drag_refused");
        // A zero/sub-threshold move never detached the gump from the list.
        // Preserve the native moved result so drop raises it rather than
        // appending a second list entry pointing at the same Gump.
        const bool moved = w->drag(dx,dy);
        w->drop_dragged(dx,dy,moved);
        return "\"category\":\"gameplay\",\"x\":" + std::to_string(target->get_x()) + ",\"y\":" + std::to_string(target->get_y());
    }
    if (op == "transfer_item") {
        if (input || w->get_usecode()->in_usecode() || w->main_actor_dont_move()
            || !w->main_actor_can_act() || w->get_gump_man()->modal_gump_mode())
            throw std::runtime_error("player_control_owned");
        auto source_object = resolve(x);
        auto target_object = resolve(y);
        int drop_x = -1, drop_y = -1;
        bool positioned = false;
        if (in >> drop_x) {
            if (!(in >> drop_y) || drop_x < 0 || drop_y < 0) throw std::runtime_error("invalid_drop_coordinates");
            positioned = true;
        }
        auto* destination = target_object->as_container();
        bool destination_open = false;
        for (auto* g : *w->get_gump_man())
            if (g->get_container() == destination) destination_open = true;
        auto* member = destination ? party_inventory_actor(destination) : nullptr;
        if (!member || !member->can_act_charmed() || a->distance(member) > 8)
            throw std::runtime_error("requires_nearby_party_container");
        if (source_object->get_owner()) {
            auto* source_member = party_inventory_actor(source_object.get());
            if (!source_member || !source_member->can_act_charmed() || a->distance(source_member) > 8)
                throw std::runtime_error("requires_nearby_party_inventory");
        } else if (source_object->get_map_num() != a->get_map_num() || a->distance(source_object.get()) > 8)
            throw std::runtime_error("pickup_outside_local_reach");
        auto* win = w->get_win();
        int sx=-1,sy=-1,dx=-1,dy=-1;
        for(int gy=win->get_start_y();gy<win->get_start_y()+win->get_full_height();++gy)
            for(int gx=win->get_start_x();gx<win->get_start_x()+win->get_full_width();++gx) {
                auto* g=w->get_gump_man()->find_gump(gx,gy);
                Game_object* hit=nullptr;
                int lx=gx,ly=gy;
                if(g) {
                    if(!w->get_gump_man()->map_game_to_gump(g,gx,gy,lx,ly)) continue;
                    hit=g->find_object(lx,ly);
                } else hit=w->find_object(gx,gy);
                if(sx<0 && hit==source_object.get()) { sx=gx;sy=gy; }
                if(dx<0 && g && !g->on_button(lx,ly)
                    && ((!positioned && ((!destination_open && hit==destination)
                        || (!hit && g->get_container()==destination && g->get_cont_or_actor(lx,ly)==destination)))
                        || (positioned && g->get_container()==destination
                            && lx-g->get_x()==drop_x && ly-g->get_y()==drop_y))) { dx=gx;dy=gy; }
            }
        if(sx<0 || dx<0) throw std::runtime_error("source_or_destination_not_visible");
        cli_reply(sequence,true,"\"category\":\"gameplay\",\"status\":\"dispatched\"");
        if(w->start_dragging(sx,sy)) { w->drag(dx,dy);w->drop_dragged(dx,dy,true); }
        return {};
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
			// A partially exposed physical item need not have an exposed centre.
			// Use only an actual visible hit inside its observed sprite bounds.
			bool found = false;
			for (int py = r.y; py < r.y+r.h && !found; ++py)
				for (int px = r.x; px < r.x+r.w; ++px)
					if (px >= 0 && py >= 0 && px < w->get_width() && py < w->get_height()
						&& !w->get_gump_man()->find_gump(px, py) && w->find_object(px, py) == o.get()) {
						sx = px; sy = py; found = true; break;
					}
			if (!found) {
				throw std::runtime_error("object_occluded_or_offscreen");
			}
		}
		cli_reply(sequence, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
		w->double_clicked(sx, sy);
		return {};
	}
	throw std::runtime_error("unknown_command");
}
