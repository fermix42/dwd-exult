#include "Wait_gump.h"
#include "native_orb_chart.h"
#include "Slider_gump.h"
#include "cli_driver.h"

#include "Configuration.h"
#include "effects.h"
#include "Text_button.h"
#include "Gump.h"
#include "Gump_button.h"
#include "Gump_manager.h"
#include "native_virtue_case.h"
#include "SDL3/SDL.h"
#include "Text_gump.h"
#include "Yesno_gump.h"
#include "actors.h"
#include "cli_world.h"
#include "native_wait.h"
#include "conversation.h"
#include "exult.h"
#include "fnames.h"
#include "game.h"
#include "gameclk.h"
#include "game_time.h"
#include "gamemap.h"
#include "gamewin.h"
#include "keyactions.h"
#include "server.h"
#include "tqueue.h"
#include "ucfunction.h"
#include "ucinternal.h"
#include "ucsched.h"
#include "utils.h"

#include <charconv>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <typeinfo>
#include <vector>
bool        cli_enabled = false;
std::string cli_run_dir;
void*       cli_input_paint = nullptr;
bool SaveIMG_RW(SDL_Surface*, SDL_IOStream*, bool, int);

namespace {
	// One outstanding case-editor command; retain SDL's borrowed text until
	// the normal game loop has polled and handled all queued events.
	int case_input_id = -1;
	std::string case_input_text;
	struct Inbox {
		std::mutex              mutex;
		std::deque<std::string> lines;
	};

	Inbox* inbox   = nullptr;
	bool   started = false;
}    // namespace

std::string cli_quote(const std::string& s) {
	std::ostringstream o;
	o << '"';
	for (unsigned char c : s) {
		if (c == '"' || c == '\\') {
			o << '\\' << c;
		} else if (c < 32) {
			o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
		} else {
			o << c;
		}
	}
	o << '"';
	return o.str();
}

void cli_reply(int id, bool ok, const std::string& body) {
	std::cout << "@CLI {\"id\":" << id << ",\"ok\":" << (ok ? "true" : "false") << "," << body << "}" << std::endl;
}

void cli_case_input_complete() {
	if (!cli_enabled || case_input_id < 0) return;
	const int id = case_input_id;
	case_input_id = -1;
	case_input_text.clear();
	cli_reply(id, true, "\"category\":\"gameplay\",\"status\":\"queue_drained\"");
}

static bool cli_listener_active() {
#ifdef USE_EXULTSTUDIO
	return Server_is_listening();
#else
	return false;
#endif
}

struct CliDriver {
    // A bounded lifecycle fixture, not a game clock or a direct timer callback.
    // Suspend unrelated entries, dispatch genuine text entries through the normal
    // Time_queue, then restore the untouched world queue in its existing order.
    static std::string text_lifetime(int kind) {
        if (kind < 0 || kind > 4) throw std::runtime_error("invalid_text_fixture");
        auto* w = Game_window::get_instance();
        auto* actor = w->get_main_actor();
        auto* effects = w->get_effects();
        effects->remove_text_effects();
        if (kind == 0) actor->say("A quiet moment.");
        else if (kind == 1) effects->add_text("18", actor);
        else if (kind == 2) effects->center_text("A notification.");
        else if (kind == 4) w->got_bad_feeling(1);
        else {
            Usecode_value object(actor), message("A quiet moment.");
            dynamic_cast<Usecode_internal*>(w->get_usecode())->item_say(object, message);
        }
        w->paint();
        auto* effect = effects->get_text_effects().front().get();
        const int limit = effect->get_lifetime_ticks();
        auto* queue = w->get_tqueue();
        decltype(queue->data) suspended;
        for (auto it = queue->data.begin(); it != queue->data.end();) {
            auto current = it++;
            if (current->handler != effect) suspended.splice(suspended.end(), queue->data, current);
        }
        std::ostringstream out;
        out << "\"category\":\"test_setup\",\"normal_queue_dispatch\":true,\"limit\":" << limit
            << ",\"duration_percent\":" << w->get_overhead_text_duration() << ",\"samples\":[";
        try {
            for (int step = 0; step <= limit; ++step) {
                const bool alive = !effects->get_text_effects().empty();
                if (step) out << ',';
                int entries = 0;
                for (const auto& e : queue->data) if (e.handler == effect) ++entries;
                out << "{\"step\":" << step << ",\"alive\":" << alive << ",\"entries\":" << entries << '}';
                if (step < limit) {
                    uint32 deadline;
                    if (!alive || !queue->next_deadline(deadline) || !queue->activate_next(deadline))
                        throw std::runtime_error("text_lifecycle_fixture_stalled");
                }
            }
        } catch (...) { queue->data.merge(suspended); throw; }
        queue->data.merge(suspended);
        out << "]";
        return out.str();
    }

    static std::string inspect_usecode(int number) {
        if (number < 0) throw std::runtime_error("invalid_function_id");
        auto* uc = dynamic_cast<Usecode_internal*>(Game_window::get_instance()->get_usecode());
        auto* function = uc->find_function(number);
        if (!function) throw std::runtime_error("usecode_function_not_found");
        std::ostringstream out;
        out << "\"category\":\"inspection\",\"function\":" << number
            << ",\"length\":" << function->len << ",\"bytes\":\"";
        for (int i = 0; i < function->len; ++i)
            out << std::hex << std::setw(2) << std::setfill('0') << int(function->code[i]);
        out << "\",\"has_original\":" << (function->orig != nullptr);
        return out.str();
    }
    static std::string queue_snapshot() {
        auto* q = Game_window::get_instance()->get_tqueue();
        std::ostringstream out;
        out << q->paused << ':' << q->pause_time << ';';
        for (const auto& e : q->data)
            out << e.handler << ':' << e.sp_handler.get() << ':' << e.udata << ':' << e.time << ':' << e.pause_added << ';';
        return out.str();
    }
	static std::string state(bool input) {
		auto* w         = Game_window::get_instance();
		auto* a         = w->get_main_actor();
		auto* c         = w->get_usecode()->cli_conversation();
		auto  t         = a->get_tile();
		auto* uc        = dynamic_cast<Usecode_internal*>(w->get_usecode());
		int   overrides = 0;
		for (auto& g : uc->funs) {
			for (auto* f : g) {
				if (f && f->orig) {
					++overrides;
				}
			}
		}
		std::string legal = "[]";
		if (input) {
			if (!cli_input_paint && Mouse::mouse()->get_shape() == Mouse::greenselect) {
				legal = "[\"target_object\",\"target_tile\",\"key\"]";
			} else if (cli_input_paint == c && c->cli_choices_active()) {
				legal = "[\"choose\"]";
			} else if (cli_input_paint == c || dynamic_cast<Text_gump*>(static_cast<Paintable*>(cli_input_paint))) {
				legal = "[\"continue\",\"dismiss\"]";
			}
		} else if (w->get_gump_man()->gump_mode() || w->get_gump_man()->modal_gump_mode()) {
			legal = "[\"controls\",\"activate_control\"]";
		} else if (!w->main_actor_dont_move() && w->main_actor_can_act() && !uc->in_usecode()) {
			legal = "[\"move_to\",\"move_to_object\",\"interact_object\",\"combat\",\"quit_prompt\",\"save\",\"load\"]";
		}

		std::ostringstream o;
		o << "\"category\":\"inspection\",\"state\":{\"game\":" << cli_quote(GAME_BG ? "BG" : "SI")
		  << ",\"video\":" << cli_quote(SDL_GetCurrentVideoDriver()) << ",\"studio_listener_active\":" << cli_listener_active()
		  << ",\"baseline\":\"79c4ffc18a34ac4801cb082679f300ca621e7908\",\"usecode_path\":" << cli_quote(get_system_path(USECODE))
		  << ",\"patch_usecode_path\":" << cli_quote(get_system_path(PATCH_USECODE))
		  << ",\"gamedat_path\":" << cli_quote(get_system_path(GAMEDAT))
		  << ",\"save_path\":" << cli_quote(get_system_path("<SAVEGAME>")) << ",\"overrides\":" << overrides
		  << ",\"day\":" << w->get_clock()->get_day() << ",\"hour\":" << w->get_clock()->get_hour()
		  << ",\"minute\":" << w->get_clock()->get_minute()
          << ",\"clock_phase\":" << w->get_clock()->get_ticks()
          << ",\"time_rate\":" << w->get_clock()->get_time_rate()
          << ",\"game_time_metadata_valid\":" << Game_time::save_metadata_valid()
          << ",\"schedule_reconciliation_pending\":" << w->get_clock()->schedules_pending()
          << ",\"scripted_schedule_pending\":" << w->get_clock()->scripted_schedule_pending()
          << ",\"game_time_tick\":" << Game_time::now()
          << ",\"game_time_events\":" << Game_time::describe()
          << ",\"case_cast_attempts\":" << Virtue_case::cast_attempt_count()
          << ",\"case_editing\":" << Virtue_case::editing_slot()
          << ",\"case_casting\":" << Virtue_case::casting_active() << ",\"case_targeting\":" << Virtue_case::targeting_active()
          << ",\"food\":" << a->get_property(Actor::food_level)
          << ",\"temperature\":" << a->get_temperature()
          << ",\"protection_remaining_ms\":" << a->temporary_effect_remaining(Obj_flags::protection)
          << ",\"might_remaining_ms\":" << a->temporary_effect_remaining(Obj_flags::might) << ",\"x\":" << t.tx << ",\"y\":" << t.ty << ",\"z\":" << t.tz
		  << ",\"map\":" << a->get_map_num() << ",\"health\":" << a->get_property(Actor::health)
		  << ",\"dont_move\":" << w->main_actor_dont_move() << ",\"can_act\":" << w->main_actor_can_act()
		  << ",\"moving\":" << a->is_moving() << ",\"combat\":" << w->in_combat() << ",\"input\":" << input
		  << ",\"in_usecode\":" << uc->in_usecode() << ",\"legal_actions\":" << legal
		  << ",\"orb_chart_layout\":" << uc->orb_chart_layout()
		  << ",\"orb_given\":" << uc->get_global_flag_bool(0xdd)
		  << ",\"orb_left_trinsic\":" << uc->get_global_flag_bool(87)
		  << ",\"orb_sphere_broken\":" << uc->get_global_flag_bool(4)
		  << ",\"orb_timelord_repaired\":" << uc->get_global_flag_bool(0x134)
		  << ",\"target_intercept_pending\":" << bool(uc->get_intercept_click_on_item() || uc->get_intercept_click_on_tile())
		  << ",\"dialogue_text\":" << cli_quote(c->cli_text()) << ",\"speaker_face\":" << c->cli_speaker_face()
		  << ",\"input_owner\":"
		  << cli_quote(
					 input                                  ? (!cli_input_paint && Mouse::mouse()->get_shape() == Mouse::greenselect ? "target"
															   : c->cli_choices_active()                                              ? "answer_menu"
															   : dynamic_cast<Text_gump*>(static_cast<Paintable*>(cli_input_paint)) ? "text"
															   : cli_input_paint == c                                               ? "acknowledgement"
																																	: "unsupported_input")
					 : w->get_gump_man()->modal_gump_mode() ? "modal"
					 : w->get_gump_man()->gump_mode()       ? "gump"
															: "gameplay")
		  << ",\"text_page\":"
		  << cli_quote(
					 input && dynamic_cast<Text_gump*>(static_cast<Paintable*>(cli_input_paint))
							 ? dynamic_cast<Text_gump*>(static_cast<Paintable*>(cli_input_paint))->cli_text()
							 : "")
		  << ",\"faces\":" << c->get_num_faces_on_screen() << ",\"scripts\":" << Usecode_script::get_count()
		  << ",\"si_first_scene\":" << uc->get_global_flag_bool(Usecode_machine::si_did_first_scene)
		  << ",\"overhead_text_duration\":" << w->get_overhead_text_duration()
		  << ",\"queue_entries\":" << w->get_tqueue()->data.size() << ",\"choices_active\":" << c->cli_choices_active()
		  << ",\"choices\":[";
		for (int i = 0; i < c->get_num_answers(); ++i) {
			if (i) {
				o << ',';
			}
			o << cli_quote(c->get_answer(i));
		}
		o << "],\"gumps\":[";
		bool sep = false;
		for (auto* g : *w->get_gump_man()) {
			if (sep) {
				o << ',';
			}
			sep = true;
			o << "{\"id\":" << g->cli_identity() << ",\"modal\":" << g->is_modal() << ",\"type\":" << cli_quote(typeid(*g).name())
			  << ",\"shape\":" << g->get_shapenum() << ",\"x\":" << g->get_x() << ",\"y\":" << g->get_y();
            if (auto* wait = dynamic_cast<Wait_gump*>(g))
                o << ",\"hours\":" << wait->duration() << ",\"current_minutes\":" << wait->start_time()
                  << ",\"result_minutes\":" << wait->end_time() << ",\"result_text\":" << cli_quote(wait->result_time());
            if (auto* slider = dynamic_cast<Slider_gump*>(g))
                o << ",\"value\":" << slider->get_val() << ",\"minimum\":" << slider->minimum()
                  << ",\"maximum\":" << slider->maximum() << ",\"increment\":" << slider->increment();
            o << '}';
		}
		o << "],\"nearby\":[";
		Actor_vector nearby;
		a->find_nearby_actors(nearby, c_any_shapenum, 20);
		sep = false;
		for (auto* n : nearby) {
			if (sep) {
				o << ',';
			}
			sep    = true;
			auto p = n->get_tile();
			auto r = w->get_shape_rect(n);
			o << "{\"screen_x\":" << (r.x + r.w / 2) << ",\"screen_y\":" << (r.y + r.h / 2) << ",";
			auto* f = uc->find_function(n->get_usecode());
			o << "\"usecode\":" << n->get_usecode() << ",\"usecode_override\":" << (f && f->orig ? 1 : 0)
			  << ",\"npc\":" << n->get_npc_num() << ",\"name\":" << cli_quote(n->get_name()) << ",\"x\":" << p.tx
			  << ",\"y\":" << p.ty << ",\"schedule\":" << int(n->get_schedule_type()) << '}';
		}
		o << "],\"equipment\":[";
		sep = false;
		for (int i = 0; i < 18; ++i) {
			if (auto* obj = a->get_readied(i)) {
				if (sep) {
					o << ',';
				}
				sep = true;
				o << "{\"slot\":" << i << ",\"shape\":" << obj->get_shapenum() << ",\"name\":" << cli_quote(obj->get_name()) << '}';
			}
		}
		o << "],\"flags\":[";
		sep = false;
		for (int i = 0; i <= 38; ++i) {
			if (a->get_flag(i)) {
				if (sep) {
					o << ',';
				}
				sep = true;
				o << i;
			}
		}
		o << "],\"inventory\":[";
		Game_object_vector objects;
		a->get_objects(objects, c_any_shapenum, c_any_qual, c_any_framenum);
		sep = false;
		for (auto* obj : objects) {
			if (sep) {
				o << ',';
			}
			sep = true;
			o << "{\"shape\":" << obj->get_shapenum() << ",\"frame\":" << obj->get_framenum()
			  << ",\"quality\":" << obj->get_quality() << ",\"name\":" << cli_quote(obj->get_name()) << '}';
		}
		o << "],\"queue\":[";
		sep      = false;
		auto now = SDL_GetTicks();
		for (auto& e : w->get_tqueue()->data) {
			if (sep) {
				o << ',';
			}
			sep           = true;
			auto* handler = e.handler ? e.handler : e.sp_handler.get();
			o << "{\"due_in_ms\":" << (e.time > now ? e.time - now : 0)
			  << ",\"type\":" << cli_quote(handler ? typeid(*handler).name() : "null") << '}';
		}
		o << "],\"queue_pause_depth\":" << w->get_tqueue()->paused << "}";
		return o.str();
	}
};

void cli_start() {
	if (!cli_enabled || started) {
		return;
	}
	inbox   = new Inbox;
	started = true;
	Game_window::get_instance()->get_focus();
	std::thread([] {
		std::string line;
		while (std::getline(std::cin, line)) {
			std::lock_guard<std::mutex> lock(inbox->mutex);
			inbox->lines.push_back(line);
		}
		std::lock_guard<std::mutex> lock(inbox->mutex);
		inbox->lines.push_back("0 exit");
	}).detach();
	cli_reply(0, true, "\"event\":\"ready\",\"category\":\"lifecycle\"");
}

namespace {
	std::vector<int> command_arguments(const std::string& op, std::istringstream& in) {
		int min = 0, max = 0;
		if (op == "screenshot") {
            min = 0; max = 1; // Optional full renderer capture, without guard-band cropping.
        } else if (op == "test_calendar" || op == "drop_item") {
            min = max = 4;
		} else if (op == "move_gump" || op == "test_overhead") {
			min = max = 3;
		} else if (op == "transfer_item") {
			min = 2; max = 4; // Optional destination-gump-local drop coordinates.
		} else if (op == "ready_inventory") {
			min = max = 2;
		} else if (
				op == "test_light_item" || op == "test_remove_fuel_receiver" || op == "test_stall" || op == "test_equipment"
				|| op == "test_stale_event_metadata" || op == "test_legacy_clock" || op == "test_time_rate"
				|| op == "test_temporary_effect" || op == "native_wait" || op == "inspect_inventory" || op == "interact_inventory"
				|| op == "inspect_usecode" || op == "test_case_commission_items" || op == "test_case_companion_start"
				|| op == "test_case_breadcrumb_items" || op == "test_case_spell_setup" || op == "text_character"
				|| op == "aim_object" || op == "test_text_lifetime") {
			min = max = 1;
		} else if (
				op == "key" || op == "move" || op == "interact" || op == "click" || op == "control_at" || op == "target_at"
				|| op == "spellbook_cast") {
			min = max = 2;
		} else if (op == "find_objects" || op == "find_actors" || op == "inspect_world") {
			min = 2;
			max = 4;
		} else if (op == "inspect_tile" || op == "path_query" || op == "move_to" || op == "target_tile") {
			min = 3;
			max = 4;
		} else if (
				op == "choose" || op == "save" || op == "load" || op == "inspect_object" || op == "inspect_actor"
				|| op == "interact_object" || op == "move_to_object" || op == "activate_control" || op == "target_object") {
			min = 1;
			max = 2;
		} else if (
				op != "state" && op != "ui_state" && op != "exit" && op != "continue" && op != "dismiss" && op != "combat"
				&& op != "test_exposure" && op != "test_orb_chart_item" && op != "test_virtue_case"
				&& op != "test_virtue_case_labels" && op != "test_case_commission_food" && op != "test_case_commission_start"
				&& op != "text_effects" && op != "case_breadcrumbs_state" && op != "case_commission_state"
				&& op != "case_spell_state" && op != "test_case_extra" && op != "test_jawbone_items" && op != "wait_eligibility"
				&& op != "controls" && op != "screenshot" && op != "quit_prompt" && op != "movement_status") {
			throw std::runtime_error("unknown_command");
		}
		std::vector<int> args;
		std::string      token;
		while (in >> token) {
			int  value  = 0;
			auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
			if (parsed.ec != std::errc() || parsed.ptr != token.data() + token.size()) {
				throw std::runtime_error("invalid_integer_argument");
			}
			args.push_back(value);
			if (int(args.size()) > max) {
				throw std::runtime_error("invalid_argument_count");
			}
		}
		if (int(args.size()) < min) {
			throw std::runtime_error("missing_argument");
		}
		return args;
	}
	namespace fs = std::filesystem;

	bool path_inside_run(const fs::path& path) {
		auto root     = fs::weakly_canonical(cli_run_dir);
		auto relative = fs::weakly_canonical(path).lexically_relative(root);
		return !relative.empty() && *relative.begin() != ".." && !relative.is_absolute();
	}

	bool run_tree_isolated() {
		try {
			auto root = fs::absolute(cli_run_dir).lexically_normal();
			for (auto ancestor = root; !ancestor.empty();) {
				if (fs::is_symlink(fs::symlink_status(ancestor))) {
					return false;
				}
				auto parent = ancestor.parent_path();
				if (parent == ancestor) {
					break;
				}
				ancestor = parent;
			}
			for (const auto& entry : fs::recursive_directory_iterator(root)) {
				auto status = entry.symlink_status();
				if (fs::is_symlink(status) || !path_inside_run(entry.path())) {
					return false;
				}
				if (fs::is_regular_file(status) && fs::hard_link_count(entry.path()) != 1) {
					return false;
				}
			}
			return true;
		} catch (...) {
			return false;
		}
	}
}    // namespace

void cli_require_write_path(const std::string& path) {
	if (!cli_enabled) {
		return;
	}
	try {
		if (path_inside_run(path)) {
			return;
		}
	} catch (...) {
	}
	cli_reply(0, false, "\"category\":\"lifecycle\",\"error\":\"CLI write path escapes isolated run\"");
	std::exit(64);
}

int cli_poll(bool input, void*) {
	if (!cli_enabled || !started) {
		return 0;
	}
	cli_world_boundary();
	std::string line;
	{
		std::lock_guard<std::mutex> lock(inbox->mutex);
		if (inbox->lines.empty()) {
			return 0;
		}
		line = inbox->lines.front();
		inbox->lines.pop_front();
	}
	int                id = 0;
	std::string        op;
	std::istringstream in(line);
	in >> id >> op;
	auto* w = Game_window::get_instance();
	auto* c = w->get_usecode()->cli_conversation();
	int   x = 0, y = 0;
	try {
		if (op.empty() || id < 0) {
			throw std::runtime_error("invalid_command_header");
		}
		auto args = command_arguments(op, in);
		if (!args.empty()) {
			x = args[0];
		}
		if (args.size() > 1) {
			y = args[1];
		}
		std::ostringstream rest;
		for (size_t i = 2; i < args.size(); ++i) {
			rest << args[i] << ' ';
		}
		in.str(rest.str());
		in.clear();
        if (op == "text_character") {
            if(input || x<0 || x>0x10ffff)throw std::runtime_error("text_input_not_available");
            char text[5]{};
            if(x<0x80)text[0]=static_cast<char>(x);
            else if(x<0x800){text[0]=0xc0|(x>>6);text[1]=0x80|(x&63);}
            else if(x<0x10000){text[0]=0xe0|(x>>12);text[1]=0x80|((x>>6)&63);text[2]=0x80|(x&63);}
            else {text[0]=0xf0|(x>>18);text[1]=0x80|((x>>12)&63);text[2]=0x80|((x>>6)&63);text[3]=0x80|(x&63);}
            if(Virtue_case::editing_slot()<0 || !w->get_gump_man()->get_kbd_focus())throw std::runtime_error("no_case_text_entry_focus");
            if(case_input_id>=0)throw std::runtime_error("case_input_pending");
            case_input_text=text;
            SDL_Event event{};event.type=SDL_EVENT_TEXT_INPUT;
            event.text.windowID=SDL_GetWindowID(w->get_win()->get_screen_window());
            event.text.text=case_input_text.c_str();
            if(!SDL_PushEvent(&event))throw std::runtime_error("text_event_queue_refused");
            case_input_id=id;
        } else if (op == "key") {
            SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN;
            event.key.key = static_cast<SDL_Keycode>(x); event.key.mod = static_cast<SDL_Keymod>(y); event.key.down = true;
            event.key.windowID=SDL_GetWindowID(w->get_win()->get_screen_window());
            const bool case_editor=Virtue_case::editing_slot()>=0;
            if(case_editor && case_input_id>=0)throw std::runtime_error("case_input_pending");
            if(!SDL_PushEvent(&event))throw std::runtime_error("key_down_queue_refused");
            event.type = SDL_EVENT_KEY_UP; event.key.down = false;
            if(!SDL_PushEvent(&event))throw std::runtime_error("key_up_queue_refused");
            if(case_editor)case_input_id=id;
            else cli_reply(id, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
        } else if (op == "test_orb_chart_item") {
            if (input || w->get_usecode()->in_usecode() || w->get_gump_man()->gump_mode())
                throw std::runtime_error("chart_fixture_requires_gameplay");
            const bool supplied = Orb_chart::give();
            cli_reply(id, supplied, "\"category\":\"test_setup\",\"chart_supplied\":" + std::to_string(supplied));
        } else if (op == "inspect_usecode") {
            cli_reply(id, true, CliDriver::inspect_usecode(x));
        } else if (op == "state" || op == "ui_state") {
			cli_reply(id, true, CliDriver::state(input));
		} else if (op == "exit") {
			cli_reply(id, true, "\"category\":\"lifecycle\"");
			if (!input && !w->get_usecode()->in_usecode() && !w->get_gump_man()->modal_gump_mode()) {
				quitting_time = QUIT_TIME_YES;
			} else {
				cli_reply(0, false, "\"category\":\"lifecycle\",\"event\":\"cancelled\",\"cleanup_complete\":false");
				std::exit(2);
			}
		} else if (op == "continue" || op == "choose" || (op == "dismiss" && input)) {
			if (!input) {
				throw std::runtime_error("not awaiting input");
			}
			if (!c->cli_choices_active() && cli_input_paint != c
				&& !dynamic_cast<Text_gump*>(static_cast<Paintable*>(cli_input_paint))) {
				throw std::runtime_error("unsupported_input_owner: acknowledgement not established");
			}
			int key = ' ';
			if (op == "choose") {
				if (cli_input_paint != c || !c->cli_choices_active() || x < 0 || x >= c->get_num_answers() || x >= 35) {
					throw std::runtime_error("invalid dialogue choice");
				}
				key = std::string("123456789abcdefghijklmnopqrstuvwxyz")[x];
			} else if (c->cli_choices_active()) {
				throw std::runtime_error("choose a dialogue response");
			}
			if (op == "dismiss" && dynamic_cast<Text_gump*>(static_cast<Paintable*>(cli_input_paint))) {
				key = -1;
			}
			cli_reply(id, true, "\"category\":\"gameplay\"");
			return key;
        } else if (op == "test_calendar" || op == "test_legacy_clock") {
            if (input || w->get_usecode()->in_usecode()) throw std::runtime_error("owned_test_context");
            auto* clock = w->get_clock();
            const auto previous = Game_time::now();
            if (op == "test_calendar") {
                if (x < 0 || x > 65535 || y < 0 || y > 23 || args[2] < 0 || args[2] > 59 || args[3] < 0 || args[3] >= 25)
                    throw std::runtime_error("invalid_calendar_fixture");
                clock->set_day(x); clock->set_hour(y); clock->set_minute(args[2]); clock->set_ticks(args[3]);
                Game_time::calendar_jump(previous);
            } else {
                if (x < 0 || x > 1440) throw std::runtime_error("invalid_legacy_interval");
                clock->increment(x);
            }
            cli_reply(id, true, "\"category\":\"test_setup\",\"synthetic_calendar\":true");
        } else if (op == "test_light_item") {
            if (!GAME_SI || input || w->main_actor_dont_move() || !w->main_actor_can_act()
                || w->get_usecode()->in_usecode() || w->get_gump_man()->gump_mode() || (x != 1 && x != 2))
                throw std::runtime_error("SI_light_unit_requires_idle_context");
            auto item = w->get_map()->create_ireg_object(x == 1 ? 595 : 481, 0);
            item->set_quality(x == 1 ? 40 : 2); // Explicit fuel UNIT setup; no registration.
            if (x == 1) {
                if (!w->get_main_actor()->add(item.get(), true)) throw std::runtime_error("light_fixture_add_failed");
            } else {
                auto at = w->get_main_actor()->get_tile();
                item->move(at.tx + 2, at.ty + 2, at.tz);
            }
            cli_reply(id, true, "\"category\":\"test_setup\",\"synthetic_light_item\":true");
        } else if (op == "test_exposure" || op == "test_equipment") {
            if (input || w->main_actor_dont_move() || !w->main_actor_can_act() || w->get_usecode()->in_usecode())
                throw std::runtime_error("fixture_requires_player_control");
            auto* actor = w->get_main_actor();
            if (op == "test_exposure") {
                if (!GAME_SI) throw std::runtime_error("SI_only_environment_fixture");
                actor->set_temperature(49);
                actor->set_flag(Obj_flags::freeze); // Environment fixture, never a quest/control flag.
                cli_reply(id, true, "\"category\":\"test_setup\",\"synthetic_environment\":true");
            } else {
                if ((GAME_BG && x != 1) || (GAME_SI && x != 2) || actor->get_readied(lfinger))
                    throw std::runtime_error("unsupported_equipment_fixture_or_occupied_slot");
                auto ring = w->get_map()->create_ireg_object(GAME_BG ? 297 : 296, 0);
                const bool equipped = actor->add_readied(ring.get(), lfinger);
                cli_reply(id, true, "\"category\":\"test_setup\",\"synthetic_equipment\":true,\"equipped\":"
                    + std::to_string(equipped) + ",\"flag\":" + std::to_string(GAME_BG ? Obj_flags::protection : Obj_flags::invisible));
            }
        } else if (op == "test_stall") {
            if (input || w->get_usecode()->in_usecode() || x < 1 || x > 1500) throw std::runtime_error("invalid_frame_stall_fixture");
            SDL_Delay(x); // Controlled frame-lag unit workload, never normal gameplay.
            cli_reply(id, true, "\"category\":\"test_setup\",\"synthetic_frame_stall\":true");
        } else if (op == "test_time_rate") {
            if (input || w->get_usecode()->in_usecode() || x < 1 || x > 20) throw std::runtime_error("invalid_time_rate_setup");
            w->get_clock()->set_time_rate(x);
            cli_reply(id, true, "\"category\":\"test_setup\",\"time_rate\":" + std::to_string(x));
        } else if (op == "test_temporary_effect") {
            // Explicit CLI-only condition fixture. Never a quest/control flag,
            // arbitrary intrinsic or claimed natural spellcasting action.
            if (input || w->main_actor_dont_move() || !w->main_actor_can_act() || w->get_usecode()->in_usecode())
                throw std::runtime_error("fixture_requires_player_control");
            if (x != 1 && x != 2) throw std::runtime_error("only_protection_or_might_fixture_supported");
            auto* actor = w->get_main_actor();
            const int flag = x == 1 ? Obj_flags::protection : Obj_flags::might;
            if (actor->get_flag(flag)) throw std::runtime_error("effect_already_present");
            actor->set_flag(flag);
            cli_reply(id, true, "\"category\":\"test_setup\",\"synthetic_condition\":true,\"remaining_ms\":"
                + std::to_string(actor->temporary_effect_remaining(flag)));
        } else if (op == "native_wait" || op == "wait_eligibility") {
            const auto before_tick = Game_time::now();
            const auto before_minutes = w->get_clock()->get_total_minutes();
            const auto before_phase = w->get_clock()->get_ticks();
            const auto before_queue = CliDriver::queue_snapshot();
            const auto before_native_events = Game_time::describe();
            Native_wait::Result result{x, 0, "input-owner"};
            if (!input) {
                if (op == "wait_eligibility") result.reason = Native_wait::eligibility(*w);
                else result = Native_wait::advance(*w, x);
            }
            std::ostringstream out;
            out << "\"category\":\"gameplay\",\"requested_minutes\":" << result.requested_minutes
                << ",\"completed_minutes\":" << result.completed_minutes
                << ",\"completed_ticks\":" << result.completed_ticks
                << ",\"initial_tick\":" << before_tick << ",\"final_tick\":" << Game_time::now()
                << ",\"reason\":" << cli_quote(result.reason)
                << ",\"initial_minutes\":" << before_minutes
                << ",\"final_minutes\":" << w->get_clock()->get_total_minutes()
                << ",\"phase_unchanged\":" << (before_phase == w->get_clock()->get_ticks())
                << ",\"queue_unchanged\":" << (before_queue == CliDriver::queue_snapshot())
                << ",\"native_events_unchanged\":" << (before_native_events == Game_time::describe());
            cli_reply(id, true, out.str());
        } else if (op == "save" || op == "load") {
			if (input || w->get_usecode()->in_usecode() || w->main_actor_dont_move() || !w->main_actor_can_act()
				|| w->get_gump_man()->modal_gump_mode()) {
				throw std::runtime_error("save/load refused in owned context");
			}
			if (x < 0 || x > 9) {
				throw std::runtime_error("slot must be 0..9");
			}
			if (!run_tree_isolated()) {
				throw std::runtime_error("redirected_run_path");
			}
			if (op == "save") {
				w->write(true);
				w->save_gamedat(x, "CLI disposable checkpoint");
			} else {
				w->restore_gamedat(x);
				w->read();
			}
			cli_reply(id, true, "\"category\":\"lifecycle\"");
		} else if (op == "move" || op == "interact" || op == "combat") {
			if (input || w->get_gump_man()->modal_gump_mode() || w->main_actor_dont_move() || !w->main_actor_can_act()
				|| w->get_usecode()->in_usecode()) {
				throw std::runtime_error("player action refused in owned context");
			}
			cli_reply(id, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
			if (op == "move") {
				w->start_actor_along_path(x, y, w->get_std_delay());
			} else if (op == "interact") {
				w->double_clicked(x, y);
			} else {
				w->toggle_combat();
			}
		} else if (op == "control_at") {
            auto* g = w->get_gump_man()->find_gump(x,y);
            int lx=x,ly=y;
            Gump_button* b=nullptr;
            if (g && w->get_gump_man()->map_game_to_gump(g,x,y,lx,ly)) b=g->on_button(lx,ly);
            cli_reply(id,true,"\"category\":\"inspection\",\"action\":" + cli_quote(b?b->cli_action():""));
		} else if (op == "test_text_lifetime") {
            if (input || w->get_usecode()->in_usecode() || w->get_gump_man()->gump_mode()) throw std::runtime_error("player_control_owned");
            cli_reply(id, true, CliDriver::text_lifetime(x));
        } else if (op == "controls" || op == "activate_control" || op == "dismiss") {
			std::ostringstream out;
			out << "\"category\":\"inspection\",\"controls\":[";
			bool  sep     = false;
			int   index   = 0;
			int   actionx = 0, actiony = 0;
			bool  actionfound = false;
            Gump_button* semantic_button = nullptr;
			Gump* modal_owner = nullptr;
			for (auto* g : *w->get_gump_man()) {
				if (g->is_modal()) {
					modal_owner = g;
				}
			}
			for (auto* g : *w->get_gump_man()) {
				struct Bounds {
					int minx = 100000, miny = 100000, maxx = -100000, maxy = -100000;
				};

				std::map<Gump_button*, Bounds> found;
				auto*                          win = w->get_win();
				for (int gy = win->get_start_y(); gy < win->get_start_y() + win->get_full_height(); ++gy) {
					for (int gx = win->get_start_x(); gx < win->get_start_x() + win->get_full_width(); ++gx) {
                        // A control under another Gump cannot receive this native
                        // click. Do not advertise/activate its occluded hit area.
                        // Native modal input is dispatched directly to the top modal,
                        // including its procedural buttons (not shape hit masks).
                        if (g != modal_owner && w->get_gump_man()->find_gump(gx, gy) != g) continue;
						int lx, ly;
						if (!w->get_gump_man()->map_game_to_gump(g, gx, gy, lx, ly)) {
							continue;
						}
						if (auto* b = g->on_button(lx, ly)) {
							auto& r = found[b];
							r.minx  = std::min(r.minx, gx);
							r.maxx  = std::max(r.maxx, gx);
							r.miny  = std::min(r.miny, gy);
							r.maxy  = std::max(r.maxy, gy);
						}
					}
				}
				for (auto& entry : found) {
					auto* b = entry.first;
					auto& r = entry.second;
					if (sep) {
						out << ',';
					}
					sep = true;
					out << "{\"id\":" << b->cli_identity() << ",\"available\":" << (!modal_owner || modal_owner == g)
						<< ",\"action\":" << cli_quote(b->cli_action()) << ",\"gump_id\":" << g->cli_identity()
						<< ",\"gump\":" << index << ",\"shape\":" << b->get_shapenum() << ",\"checkmark\":" << b->is_checkmark()
						<< ",\"label\":" << cli_quote(dynamic_cast<Text_button*>(b) ? dynamic_cast<Text_button*>(b)->get_text() : "")
						<< ",\"x\":" << (r.minx + r.maxx) / 2 << ",\"y\":" << (r.miny + r.maxy) / 2 << '}';
					if ((!modal_owner || modal_owner == g)
						&& ((op == "activate_control" && b->cli_identity() == static_cast<unsigned long long>(x))
							|| (op == "dismiss" && std::string(b->cli_action()) == (g->is_modal() ? "cancel" : "close")))) {
						actionx     = (r.minx + r.maxx) / 2;
						actiony     = (r.miny + r.maxy) / 2;
						actionfound = true;
                        semantic_button = b;
					}
				}
				++index;
			}
			out << ']';
			if (op == "controls") {
				cli_reply(id, true, out.str());
			} else {
				if (input || !actionfound) {
					throw std::runtime_error("unsupported_ui_action_or_stale_control");
				}
                const std::string action = semantic_button->cli_action();
                if (action == "close" || action == "cancel" || action == "confirm" || action == "increase" || action == "decrease") {
                    // Revalidated native semantic control. Its activate method is
                    // the same method called by ordinary button release; do not
                    // leave asynchronous mouse gestures outstanding between commands.
                    cli_reply(id, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
                    semantic_button->activate(Gump_Base::MouseButton::Left);
                    return 0;
                }
				int sx, sy;
				w->get_win()->game_to_screen(actionx, actiony, w->get_fastmouse(), sx, sy);
				SDL_Event e{};
				e.type            = SDL_EVENT_MOUSE_BUTTON_DOWN;
				e.button.button   = 1;
				e.button.x        = sx;
				e.button.y        = sy;
				e.button.windowID = SDL_GetWindowID(w->get_win()->get_screen_window());
				SDL_PushEvent(&e);
				e.type = SDL_EVENT_MOUSE_BUTTON_UP;
				SDL_PushEvent(&e);
				cli_reply(id, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
			}
		} else if (op == "click") {
			if (input || !w->get_gump_man()->find_gump(x, y)) {
				throw std::runtime_error("click requires a Gump at game coordinates");
			}
			int sx, sy;
			w->get_win()->game_to_screen(x, y, w->get_fastmouse(), sx, sy);
			SDL_Event e{};
			e.type            = SDL_EVENT_MOUSE_BUTTON_DOWN;
			e.button.button   = 1;
			e.button.x        = sx;
			e.button.y        = sy;
			e.button.windowID = SDL_GetWindowID(w->get_win()->get_screen_window());
			if (!SDL_PushEvent(&e)) {
				throw std::runtime_error("event queue refused down");
			}
			e.type = SDL_EVENT_MOUSE_BUTTON_UP;
			if (!SDL_PushEvent(&e)) {
				throw std::runtime_error("event queue refused up");
			}
			cli_reply(id, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
		} else if (op == "screenshot") {
			w->paint();
			if (!run_tree_isolated()) {
				throw std::runtime_error("redirected_run_path");
			}
			auto  screenshot_path = (std::filesystem::path(cli_run_dir) / "screenshot.png").string();
			auto* f               = SDL_IOFromFile(screenshot_path.c_str(), "wb");
			if (!f) {
				throw std::runtime_error("screenshot open failed");
			}
			bool ok;
            if (x == 1) {
                w->show(true);
                auto* renderer = SDL_GetRenderer(w->get_win()->get_screen_window());
                auto* surface = renderer ? SDL_RenderReadPixels(renderer, nullptr) : nullptr;
                if (!surface) { SDL_CloseIO(f); throw std::runtime_error("renderer capture failed"); }
                ok = SaveIMG_RW(surface, f, true, 0);
                SDL_DestroySurface(surface);
            } else ok = w->get_win()->screenshot(f, false);
			if (!ok) {
				throw std::runtime_error("screenshot failed");
			}
			cli_reply(id, true, "\"category\":\"inspection\",\"file\":\"screenshot.png\"");
		} else if (op == "quit_prompt") {
			if (input || w->get_usecode()->in_usecode()) {
				throw std::runtime_error("player_control_owned");
			}
			cli_reply(id, true, "\"category\":\"gameplay\",\"status\":\"dispatched\"");
			ActionQuit(nullptr);
		} else {
			auto body = cli_world(id, op, x, y, in, input);
			if (!body.empty()) {
				cli_reply(id, true, body);
			}
		}
	} catch (const std::exception& e) {
		cli_reply(
				id, false,
				"\"category\":\"error\",\"operation\":" + cli_quote(op) + ",\"input_owner\":"
						+ cli_quote(
								input                                  ? (c->cli_choices_active() ? "answer_menu" : "input")
								: w->get_gump_man()->modal_gump_mode() ? "modal"
																	   : "gameplay")
						+ ",\"error\":" + cli_quote(e.what()));
	}
	return 0;
}

bool cli_validate_paths() {
	if (!cli_enabled) {
		return true;
	}
	namespace fs = std::filesystem;
	auto root    = fs::weakly_canonical(fs::path(cli_run_dir));
	auto inside  = [&](const std::string& path) {
        auto target   = fs::weakly_canonical(fs::path(path));
        auto relative = target.lexically_relative(root);
        return !relative.empty() && *relative.begin() != ".." && !relative.is_absolute();
	};
	if (!run_tree_isolated() || !inside(get_system_path(GAMEDAT)) || !inside(get_system_path("<SAVEGAME>"))) {
		cli_reply(0, false, "\"category\":\"lifecycle\",\"error\":\"CLI save paths must be inside --test-run-dir\"");
		std::exit(64);
	}
	config->set("config/audio/enabled", "no", false);
	return true;
}

void cli_cleanup_complete(bool cancelled) {
	if (cli_enabled) {
		cli_reply(
				0, !cancelled,
				"\"category\":\"lifecycle\",\"event\":\"shutdown\",\"cleanup_complete\":true,\"cancelled\":"
						+ std::string(cancelled ? "true" : "false"));
	}
}

bool cli_config_isolated(const std::string& file) {
	namespace fs = std::filesystem;
	try {
		auto root = fs::weakly_canonical(cli_run_dir);
		return fs::is_directory(root) && fs::is_regular_file(root / ".exult-test-run") && fs::is_regular_file(file)
			   && fs::weakly_canonical(file).parent_path() == root && run_tree_isolated();
	} catch (...) {
		return false;
	}
}
