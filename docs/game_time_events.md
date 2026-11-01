# Native Wait and engine-owned game-time events

Native Wait is a built-in BG/SI gameplay feature. Shift-W opens a shared native
scroll showing current time, a 1..24-hour selection and resulting time/next day.
The checkmark/Enter confirms; No/Escape cancels; arrows and Home/End adjust.
It requires no watch, cheat mode, feature toggle or separate mod. Bind the normal
`WAIT` action in a custom key file to change the shortcut; plain W uses the watch.
Unsafe activity, hunger, harmful conditions or cold can refuse or interrupt it.
Waiting applies calendar maintenance and fuel/effect aging, without granting sleep
benefits. Original bed rest remains its own Usecode operation.

The scheduler provides a typed fuel capability, not permission to execute Usecode
during Wait. The Gump and Unified CLI call the same `Native_wait::advance` service.

## Clock and ownership

`Game_time::now()` is total calendar minutes * 25 plus fractional clock progress.
`game_time.cc` uses the existing `Time_queue` container with these units. It never
substitutes SDL time or drains the active-world queue. One live owner/purpose pair
has one fuel event. Weak object ownership, validated receiver class/shape and world
clear protect removal, transformation and load. A map owns queue nodes; raw map
keys are identity keys only. Dispatch is bounded to 4096 deliveries.

`Game_clock` dispatches this queue during ordinary calendar progression.
`Native_wait::advance` splits at minute/event deadlines, processes calendar
maintenance and ages supported party timers, then checks eligibility again. Exact
elapsed duration is returned in 1/25-minute units. NPC schedule actions are deferred
and coalesced to the final calendar schedule on the next ordinary clock event.
Scripted schedule boundaries stop advancement. No intermediate NPC movement or
arbitrary schedule-constructor Usecode executes during Wait.

## Usecode capability

Intrinsic 0xe6 (`schedule_game_time_fuel`) takes:

    owner, period_clock_steps, spent_shape, spent_quality, sound, legacy_function

Example BG torch: `(@0xe6)(obj, 50, 595, 255, 106, 0x600)`.
The return value reports admission. Period is 1..3600 nominal clock steps, scaled
by the configured time rate into calendar units. Receiver must be a live quality
IREG object, not actor/container/egg. Output must be a plain object shape. Quality
and sound ranges are checked before mutation. A delivery can only decrement the
receiver's positive quality, perform its declared spent transformation and optional
sound, and refresh gear/light/palette. It cannot call Usecode, move actors, change
maps, take input or launch conversations. Repeating deadlines use intended delivery
time, not wall-clock lateness. Invalid/cancelled owners are pruned. halt_scheduled
also cancels native fuel. This is the only public registered event capability now;
other capabilities require their own enforceable implementation.

The final parameter explicitly declares an old owned delayed call for one-time
save reconstruction. Only the exact single-call delayed envelope (optional nohalt)
is adopted; its remaining phase is retained and executable continuation exhausted.
This limited migration is not event identity or arbitrary-script safety analysis.
Legacy delay_minutes/delay_hours and unrelated scripts retain their semantics.
Party-owned executable sequences block Wait; nonparty background scripts remain
pending. Neither nohalt nor object shape alone declares safety.

## Built-in compatibility

`data/game_time_bg.uc` and `data/game_time_si.uc` are compiled by the ordinary UCC
into `usecode/game_time_compat.h`, loaded after original Usecode and before normal
mod patches. Installed assets are untouched. Mods can override these entries through
normal precedence. BG startLight 0x905 registers torch/plain light/sconce; SI 0x950
registers torch/plain light and its distinct sconce recipe (435 -> 481 with
frame0/1 ->2/3). The native descriptor validates that completion before admission.
The retained active Keyring build inherits original light routines and needs no
source change; dormant items/torch.uc and misc/rest_functions.uc are not included
by its root. SI Fixes story sequences are unchanged.

The generated header is checked in so normal engine builds do not depend on a
locally installed UCC or experiment directory. After changing either adapter,
regenerate with `python tools/compile_resources.py --ucc /path/to/ucc` using the
ordinary Usecode compiler (from `usecode/compiler`), then commit source and header
together. Generation uses temporary files and does not rebuild or alter mods.

## Persistence and compatibility

Optional gwindat magic 0x47544d32 records phase, rate, 64-bit world metadata epoch
and optional pending-schedule bits. Optional IREG special type 5, version 2,
length 25 records lit shape, period, spent shape, spent quality+1, sound+1, rate,
absolute deadline and epoch. Owner is the enclosing IREG object. Restoring after
readied initialization reconciles one node and preserves phase. Invalid/stale
records are ignored. Actor duration attributes use exult.effect.v=2, low/high
16-bit remaining+1 fields and four epoch words. A legacy load creates a fresh epoch;
this rejects stale records surviving unvisited chunks or old-engine resaves.
No raw addresses are serialized. Original saves retain legacy reconstruction.

Ordinary non-lag cadence is nominally preserved: 50 steps = 10 seconds at the
standard 200ms step, or 2 game minutes at rate 1. Rate changes rescale remaining
phase. Under frame stalls the game calendar can progress more slowly than SDL;
calendar fuel then burns more slowly than legacy fuel. This is a demonstrated
compatibility decision, not a globally corrected clock. Legacy clock jumps shift
native deadlines to avoid duplicating rest's own reconciliation. BG bed rest and NPC reconciliation now have initialized-engine evidence; natural
SI Fixes Batlin vision remains untested.


## Legacy materialization correction and native interface

At the end of each IREG object (after all special records), including lazily read
chunks and nested inventories, `ensure_fuel` uses the explicit built-in light
participant. The Usecode loader remembers the actual adapter instance; a normal
mod override of that entry disables this automatic compatibility path. No arbitrary
Usecode runs during reconstruction. Recognized active states preserve any valid
native event. Declared supported legacy envelopes are adopted without duplicate
execution. Otherwise missing/stale metadata starts one full normal interval at
current game time and preserves saved fuel/object state. Historical discarded
countdown phase is deliberately unavailable and not inferred. No format/epoch
change is needed. Unsupported frames, extinguished/spent/zero-fuel objects and
unrelated decorative lights receive no fresh registration.

Shift-W invokes normal action WAIT. Shared Wait_gump uses original BG55/SI49 scroll
art, native small black font, arrows, checkmark and No button. Selection is local,
1..24 hours. Native modal pause/resume completes before the existing advancement
service revalidates and executes. Escape/No cancel. Ordinary W is unchanged.
Default key files and runtime key help include WAIT; normal custom mappings win.
The SI sconce descriptor (game SI, lit435, spent481) also applies the original
frame+2 completion, validated to active0/1 and spent2/3 frames. Existing v2 fields
already identify that recipe and retain current orientation; no persistence field
or migration epoch changes. Runtime SI lighting fixtures exercise original item
interactions and this completion; they are not natural item-acquisition evidence.

## Regression coverage and limits

With the environment in `docs/test_cli.md`, run:

    python tools/test_cli/native_wait_checks.py --run wait-bg --game bg --checkpoint /path/to/exult00bg.sav
    python tools/test_cli/native_wait_checks.py --run wait-si --game si

The BG input must be a legitimate slot-zero save with a nearby burning ground
torch with more than30 fuel and an idle, healthy, fed party. It is copied into a fresh disposable run;
the original is read-only. The SI test drives the real opening to player control.
Use `--mod Keyring` with a corresponding BG checkpoint or `--mod sifixes` for SI.
The check covers native selection/cancellation, clock freeze, confirmation through
the shared service, fuel where present, refusal/ownership, partial hunger
interruption, and save/load with continued control. It uses semantic observations
and engine-local actions; no visible game or desktop input is launched.
See `test_cli_protocol.md` for command details and labelled unit fixtures.

Executed milestone evidence covers BG/Keyring/SI/SI Fixes, natural SI party torches,
legacy downgrade/resave recovery, stale chunk fixtures, effects, bed rest and
deferred schedules. Human acceptance covers the BG/SI visual design and reported
normal-mode Shift-W operation, not every duration or cancellation path.
Natural SI Fixes Batlin far-sight, natural time-stop, a distant unvisited-chunk
journey, independent natural receiver destruction and SI sconce orientation1->3
remain untested. Source/controlled-unit safety does not claim those natural scenes.
Non-Windows toolchains also require platform validation.
