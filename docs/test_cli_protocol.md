# Command protocol

`driver.py` accepts one JSON object per line and emits a JSON record containing the
request and `response`. Engine-backed responses include `id`, `ok`, `category`, and
command-specific fields. `ok` means the command was accepted/handled; it is **not
arrival, damage, dialogue completion, or a test PASS**. A move's `movement` field
establishes its progress/outcome. A session needs executed assertions and ordinary
successful cleanup for PASS.

The public JSON interface is a client adapter for the same Exult binary's existing
local pipe protocol. Do not send public JSON straight to the private engine pipe.
No TCP service is exposed. Current English BG/SI resources were exercised; text
fields expose the engine's current stored text/glyph strings, not OCR. Sign/rune
font encodings may not be normal prose.

## Request fields

All requests require `cmd`. IDs/refs are positive integers. Boolean observations
are generally engine 0/1 values. Unknown observations use JSON null or the explicit
`unknown` classification. The frontend retains previous scripts and commands.

| cmd | Additional request fields | Category / response |
| --- | --- | --- |
| init | game: bg/si; mod optional: Keyring/sifixes; run: fresh alphanumeric/hyphen/underscore name | Lifecycle. Actual normal new-game/resource initialization, ready event, run path, resource hashes and executable hash. One game per process. |
| state, ui_state | none | Inspection. `state` object described below. |
| inspect_world, find_objects | radius 0..32 (default 12), limit 1..200 (50), offset >=0 (0, max 10000), shape optional (native any-shape default) | Inspection. map, avatar ref, total, offset, coverage=loaded_chunks_only, objects array. |
| find_actors | same query bounds | Inspection. Same object schema, restricted to actors. |
| inspect_object, inspect_actor | ref | Inspection. `object`; actor form refuses non-actor refs. |
| inspect_tile | x,y,z world tile | Inspection. tile, resolved_tile, blocked for Avatar footprint, obstacle ref (0 if none identified), terrain_shape/frame. |
| path_query | x,y,z world tile | Inspection. found, blocked_destination, obstacle, steps, bounded_search, unknown_region_encountered. |
| move_to | x,y,z world tile; timeout seconds default 10, maximum 120, minimum .001 | Gameplay. movement and destination. Native pathfinding/walking; completion requires movement_status. |
| move_to_object | ref | Gameplay. Native path toward a **snapshot** of the target's anchor tile, stopping within one tile. Contained/nonlocal targets refused. Does not continuously chase a moving object. |
| movement_status | none | Inspection. movement: none/moving/reached/blocked/interrupted/timed_out, destination, actual. |
| interact_object | ref | Gameplay. Weak-ref resolution, player-control check, native reachability check, current on-screen hit-test verification, then ordinary double_clicked. Returns dispatched before potentially nested Usecode. |
| continue | none | Gameplay. Acknowledge an actual conversation/text input page. Refuses answer menus and unidentified input owners. |
| choose | exact text OR zero-based index (0..34) | Gameplay. Select a reported response through the normal Get_click choice key path. Refuses absent/invalid choices. |
| dismiss | none | Gameplay. Acknowledge current conversation page, native ESC-equivalent return for a supported book/sign input, or native Gump close/cancel control. Never silently chooses an answer. Unsupported/no-UI states refuse explicitly. |
| controls | none | Inspection. controls array: id, gump_id/index, action, available, shape, checkmark, game-view x/y. |
| activate_control | id | Gameplay. Re-resolve current control by per-instance identity; use current native hit target and local SDL button down/up. Underlying controls are unavailable while a modal owns input. |
| quit_prompt | none | Gameplay. Ordinary ActionQuit: opens the real quit confirmation. Choose native cancel to return to play, or confirm to quit. |
| combat | none | Gameplay. Ordinary combat toggle. Target interactions still use native double_clicked combat behavior, with conservative semantic reachability guards. |
| save, load | slot 0..9 (default 0) | Lifecycle. Existing gamedat/archive APIs; refused in owned input/Usecode/incapacity contexts. Load invalidates all object refs. |
| wait, run_until | until optional flat state field/value object; timeout seconds, capped at 120 | Inspection/orchestration. Poll actual state while normal engine events run. No time acceleration or native gameplay Wait implementation. Without until, allow elapsed real time. |
| assert | state: flat expected field/value object; label optional | Inspection assertion against a fresh state snapshot. |
| screenshot | none | Inspection. Optional renderer PNG in run/screenshot.png, overwriting earlier capture. No desktop capture. |
| exit | none | Lifecycle. Idle request leads to normal Play cleanup and shutdown event. Nested unsupported exit/EOF explicitly cancels, exit 2, cleanup_complete=false. |
| move, interact, click | x,y in engine game-view coordinates | Preserved legacy gameplay commands. Prefer world refs/tiles and semantic controls. click requires a Gump. |

World destinations must be valid world coordinates/lift, within 32 tiles of the
Avatar, and in already loaded geometry. An unloaded destination returns
`world_region_not_loaded`; observations do not load a scenario to make an answer
appear known. The native A* client is reused with an observation-only loaded/local
domain check. `found:0` means no route was found in that bounded native search,
not a global proof of permanent unreachability. An encountered unknown region is
reported. `blocked` movement includes a route the bounded native query could not
establish. There is no Python route solver.

`reached` is measured from the actual Avatar tile (or the documented one-tile
object-snapshot tolerance). `blocked` means no native path was started.
`interrupted` means the native movement stopped short. `timed_out` means its
observation deadline elapsed; it does not forcibly stop/replace a live engine
action. The engine may subsequently finish that action. Dynamic obstacles can
change after a query; inspect actual progress. No action is declared complete
merely because it was dispatched.

## Observation schemas

Object fields: ref, shape, frame, class_id (native shape class), type
(actor/door/container/unknown), authoritative resource name, tile {x,y,z}, map,
chunk_x/y, owner ref (0 if none), quantity, quality, solid, height, footprint
{x,y,w,h}, closed_door, reachable, hit_test_target, interactable, distance.
`interactable` means the conservative ordinary hit-test/reachability path is
available; it does not promise that the object's Usecode will produce a result.
An object can impose further native distance/quest/context rules. Unknown classes
are not assigned invented capabilities. A contained object's location is not a
world placement; use its owner/outer map and the explicit interaction refusal.

Actor additions: npc number, health, schedule type, action RTTI name or none,
effective alignment, hostile_to_avatar from the engine's alignment predicate,
target ref, combat_schedule, weapon_ref/shape, native effective_range,
straight_path_to_avatar, line_of_fire_to_avatar (the same native straight-path
predicate used by combat), method label, target_validity=null, conditions as
Obj_flags IDs 0..38, and equipment [{slot,ref,shape}]. This does not decide hits,
damage, target selection, or override a combat implementation. Complete target
validity depends on native combat context; it is not invented here.

State preserves original Avatar/clock/map/combat/control/movement fields and adds
studio_listener_active, dialogue_text, speaker_face (portrait resource ID, -1 if
unknown), input_owner, text_page, legal_actions, and Gump identities/types/modal
status. It also exposes actual choices/choices_active, nearby schedules, relevant
inventory/equipment, flags, Usecode/script counts, timed queue types/deadlines and
pause depth, resource/save paths. `input_owner` is gameplay/gump/modal,
acknowledgement/answer_menu/text, or unsupported_input. Legal actions identify
supported command families; a control action still needs a current available ID.
Speaker face is not falsely equated to an NPC number.

Object refs are weak and monotonic within a process. Save/load and current-map
changes clear the reference registry; deleted/off-world objects refuse resolution.
The driver keeps no unsafe raw object pointers across those boundaries. Requery
objects after load. Gump/control IDs belong to each native instance and are not
recycled when a Gump closes. Expired controls refuse without altering the session.
Pagination reflects live observations; moving actors can change pages between
requests. Query limits do not imply a snapshot of the entire world.

## Errors and termination

A native refusal contains category=error, operation, input_owner, and error. The
frontend emits ok=false/category=command_error with that structured detail.
Transport/timeouts have harness_error where applicable. Unknown commands, stale
refs, unavailable controls, owned player control, unreachable/occluded objects,
invalid bounds, inappropriate choices/dismissals, and unloaded regions are explicit
refusals. Interactive sessions can continue after a refusal. Script mode stops on
failure and exits nonzero; acceptance scripts can explicitly expect refusals.

Initialization has a 30-second ready bound; replies have a 5-second bound; polling
and shutdown are bounded. Logs and result.json survive failure. Ordinary idle exit
reports shutdown, cleanup_complete=true, cancelled=false, exit 0. Nested exit/EOF
reports cancelled, cleanup_complete=false, exit 2 and session FAIL. A client watchdog
kill is labelled termination=forced and FAIL. Normal nonzero/unknown termination
is failed. A completed save is independent evidence and is never inferred from an
exit status. Normal native modal cancellation is supported: cancel its control,
wait for gameplay ownership, then exit idle for full cleanup.

## Native Wait gameplay service

`native_wait` with `minutes` invokes the actual `Native_wait::advance` service;
`wait_eligibility` is read-only. Typed native calendar events advance; arbitrary
Usecode stays pending. Executable party-owned sequences and scripted schedule
boundaries refuse or interrupt. Existing `wait`/`run_until` still poll ordinary
real engine time. See `game_time_events.md` for the scheduling contract.

Responses include requested/completed minutes, reason, exact initial/final calendar
minutes, phase_unchanged and queue_unchanged. Dispatch success does not imply a
complete wait. Invalid duration (including zero) changes nothing. The service
accepts 1..1440 minutes; the native UI selects 1..24 hours.

`inspect_inventory` (`ref` optional, default Avatar) returns existing party-owned
contents with weak refs. `interact_inventory` requires the item to be hit-testable
inside its visible native Gump. `ready_inventory` (`ref`, `slot` 0..17) requires an
unreadied nearby party-owned item, its visible inventory Gump, and an empty destination;
it invokes normal engine fit/readied callbacks. It cannot force a slot or create
items. Inspect the resulting equipment and item state to establish completion.

Native close/cancel/confirm controls now revalidate the unoccluded hit region and
invoke the control's existing activate method on the engine thread. Other controls
retain their original local SDL path. This avoids an outstanding synthetic mouse
gesture between semantic commands, while preserving the native control callback.

`test_temporary_effect` (`effect`: protection or might) is an explicit CLI-only
synthetic condition fixture. It invokes existing Actor::set_flag and its randomized
timer; it is not natural spellcasting. It cannot mutate quest/control flags, set
arbitrary durations or replace an existing effect. Normal gameplay exposes none of
these test commands. Read-only state additionally exposes clock_phase, food,
temperature and Protection/Might remaining_ms (-1 = no owned temporary timer).
Object observations expose per-receiver scheduled executable/nohalt/due_in_ms.


## Calendar observations and isolated fixtures

`native_wait` also returns completed_ticks (25 per minute), initial/final calendar
ticks and native_events_unchanged. Check actual duration/reason, not dispatch ok.
State exposes time_rate, game_time_tick, game_time_events, metadata validity and
pending schedule reconciliation. Objects expose their native events and legacy
script executable/nohalt/due observations. Actor inspection exposes effective and
calendar schedule types. These are semantic observations, not admission controls.

CLI-only unit fixtures: test_time_rate (1..20); test_calendar (day/hour/minute/phase);
test_legacy_clock (existing increment path); test_stale_event_metadata (corrupts an
in-memory serialization of an already registered event to verify rejection);
test_equipment (BG protection/SI invisibility ring); test_exposure (SI environment);
test_stall (1..1500ms real engine-thread delay). None registers a fuel event.
The stale fixture also verifies unsafe exhaustion shape/actor receiver rejection.
Fixtures are labelled synthetic in the retained scenarios. test_temporary_effect
remains Protection/Might only. No arbitrary quest flag or callback invocation exists.

Driver init optionally accepts checkpoint (existing .sav, copied to the observed
fresh run save_path; then use native load) and patch_path (read-only conventional
Usecode patch with resource hash). The legacy-light timing fixture uses patch_path
only to compare original SDL-delay cadence in the same executable. It is not a
required feature mod and is never used for principal fuel acceptance.


## Native item drop

`drop_item` takes ref,x,y,z (world tile). It requires a visible nearby party-owned item,
player control and an unoccluded target within eight tiles. It revalidates a native
source hit and uses the existing start_dragging/drag/drop_dragged path, including
normal unready/fit/reach/drop handling. It returns dispatched; inspect actual owner,
position and event afterward. No item is created, placed forcibly or deleted.
This addition supports real ground-light rest and persistence checks. Read-only inventory inspection additionally accepts actual party actors and their
containers. The same native inventory mutation paths now support awake nearby
party owners, as detailed below.


## Native Wait interface validation

`key` takes key w/escape/enter/left/right/up/down/home/end and optional shift.
It queues engine-local SDL key down/up events through the normal input paths,
never Windows input. Dispatch is not proof of opening or selection. Observe the
actual Gump, hours, current_minutes, result_minutes/result_text and balanced pause.
Native control actions increase/decrease/confirm/cancel activate the same revalidated
buttons as normal gameplay. `init` optionally takes scale1..4 and a read-only
keys_path for isolated layout/custom-binding tests; default configuration is unchanged.

`test_remove_fuel_receiver` is explicitly a CLI-only receiver-lifetime unit fixture,
requiring an existing registered local ground item. It calls ordinary remove_this,
drops its own shared reference and verifies weak expiry and queue pruning. It does
not register fuel or alter actors/quest/control state. This is not natural deletion
acceptance. All these CLI capabilities remain inactive without --test-cli.


`test_light_item` (SI-only, kind1 torch or kind2 sconce) is an explicitly synthetic
item fixture, initialized with controlled fuel and no registration. Tests then use
normal inventory/readied/world interactions and original game Usecode. It is not
natural acquisition evidence and never substitutes for that requirement. The real
materialization/activation contract must produce the event; the fixture cannot.


Party inventory actions use the same adapters as Avatar inventory: read-only
inspect_inventory accepts actual party actors/containers; ready_inventory infers
the existing owning party actor, requires an empty native slot and visible inventory,
and calls that actor's normal fit/readied path. interact_inventory and drop_item
require an awake, nearby party owner and a current visible hit. Foreign NPC
inventories remain refused. Keys1..8 use the standard native party inventory actions.
This extension supports the natural SI torch found in Iolo's original backpack.
