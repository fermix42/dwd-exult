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
