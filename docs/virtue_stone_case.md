# Virtue Stone Case — native Stage A development handoff

Unreleased physical prototype. Human gameplay and artwork acceptance is pending.
This follows the owner's superseding native-Exult architecture instruction. It
works in ordinary Black Gate and original Keyring; no DWDKeyring edition,
separate mod installation, quest override, or original-game data edit is required.

## Source and ownership

- Branch: `codex/virtue-stone-case`.
- Worktree: `C:/Codex/Ultima7 Decomp/exult-virtue-native`.
- Accepted starting HEAD: `7bf0bd8ab2797b3618b41c00a85eb8c8e002a702`, containing
  accepted Orb Chart `4639563a8` and Native Wait `9b4c88054`.
- The old reference worktree `exult-virtue-case` remains clean on
  `codex/dwd-keyring-virtue-case`, scaffold commit `f535551ed`. None of its mod
  identity, packaging, or Usecode helper was merged here.
- `native_virtue_case.cc/.h` owns shape 1601 and its dedicated gump. Registration
  follows `Orb_chart::install_shape`: BG only, and refuses an occupied shape.
  It appends the new shape after ordinary/active-mod shape information loads.
- Windows, Automake and iOS source manifests include the implementation. Only
  Windows Release/x64 was built and exercised; other platforms are NOT TESTED.
- New code is GPL-2.0-or-later. Original notices, licenses, credits, Keyring
  source and distribution remain intact. No installer, release archive, public
  tag, merge or publication is part of this work.

## Physical implementation

The case is an ordinary `Container_game_object`, with a dedicated nonmodal
`Case_gump`. It can be held, stored inside ordinary containers, or dropped and
picked up in the world. A double-click opens the specialized gump. Nothing grants
the case during normal gameplay yet; commissioning is a later stage.

`Container_game_object::add` checks the case's insertion rule before taking
ownership, including closed-case drops and callers bypassing ordinary volume
checks. Only real `Virtue_stone_object`s of original shape 330 and the eight
original frames are accepted. Occupied identities reject the incoming stone;
combining is disabled. Ordinary weight, ownership, reach and recursion checks
remain in the normal drag/container paths. The case's interior capacity is
enforced by its eight unique compartments, independently of its outside volume.
Provisional item weight/volume are 10/3 in the engine's ordinary units.

The fixed layout, in two columns read top to bottom:

| Left column | Original frame / color | Right column | Original frame / color |
|---|---|---|---|
| Honesty | 2 / blue | Sacrifice | 3 / orange |
| Compassion | 0 / yellow | Honor | 5 / purple |
| Valor | 1 / red | Spirituality | 7 / white |
| Justice | 4 / green | Humility | 6 / black |

Original colors were checked directly against BG shape 330 and the original
palette. Keyring's Codex source also identifies frame 7 as white/Spirituality.

The gump renders the owner-supplied separate 28x32 seated artwork for each
original stone identity. Hit testing uses that seated image and returns the actual contained object.
The grab anchor follows `Jawbone_gump::set_to_spot`; native `Dragging_info` renders
the ordinary small object while dragging. No proxy, shape/frame substitution,
destination copying, new teleportation implementation or Remove Stone menu exists.
Dropping on any opaque part of the open case routes by identity, including over
another occupied compartment. No manual slot alignment is required.

Persistence uses existing container/stone IREG serialization, including original
destination x/y/z/map data. No save-format extension or text database was added.
Runtime pointers correctly refresh after load; tests compare stable references
only within a world, and restored physical contents/data across loads.

## Production-art presentation (unreleased)

The approved production package replaces the generated placeholder: closed case
28x20, open case 320x200, eight separate 28x32 stones, and two 56x20 physical lid
buttons with normal/depressed frames. The palette-indexed 14-frame resource is
`data/bg/virtue_case.shp`, appended at Exult BG FLEX index 18 without shifting any
existing entry. Editable PNGs, palette, contract and converter are retained in
`data/bg/virtue_case`; reproduction commands are in its README. The ordinary BG
archive build uses `data/bg/flx.in`; it is not a Keyring resource or separate mod.

The original column-major frame order stays `{2,0,1,4,3,5,7,6}`. Render anchors
are x=30/186, y=67/103/139/175, with seated origins (14,16). Both drawing and real-object
picking use the same loaded frame/mask. The ordinary small stone shape330 art,
mark destinations, serialization and drag cursor remain unchanged.

MARK/RECALL blank faces are painted at (42,13) and (222,13), with native
`SMALL_BLACK_FONT` centered on top and shifted one pixel down while pressed.
These are presentation-only controls: they consume a click without casting or
changing stone destinations. CLI action names explicitly say `_unimplemented`.
Parchment images remain blank: label editing, text storage and spell behavior
are not implemented by importing this art. The catch hit rectangle [146,192,28,8]
closes the real gump through its normal close path; no context menu or Remove
button was introduced. No global scaler/UI settings are forced.

The case is centered/movable through existing gump behavior.320x200 fills the
original logical viewport, so moving it there can clip it. Dedicated review
runs use Point UI scaling and disposable configuration only. Human visual
acceptance is still pending; integration tests are evidence of engine behavior,
not final artwork acceptance.

## Save/load lifecycle correction

`Game_window::read` now closes existing nonmodal inventory gumps before clearing
the old world, matching the destructor's ordering. Previously those gumps were
closed only after `setup_game` created replacement objects. Since `Gump::~Gump`
writes its owner's saved window position, that order could write through stale
container pointers. Open-case BG reloads exposed a reproducible crash. Closing
the gumps while their owners remain alive fixes the ownership boundary without
changing serialization, clocks, spells, combat or quest rules. Modal handling
continues to use `close_all_gumps`' existing exclusion of modal gumps.

An additional test-adapter defect was found and fixed: `move_gump` must pass the
actual native drag result to `drop_dragged`. Reporting a zero-distance move as
completed created duplicate list entries and later double closes. The focused
checks explicitly repeat positioning and assert unique gump identities.

## Verification and evidence

Artifacts are under `C:/Codex/Ultima7 Decomp/test-output/virtue-native/`.
Retained failing exploratory runs are not counted as passes.

| Real engine run | Result |
|---|---|
| `runs/bg-stage-a-verified-01` | 50 assertions, 0 failures, clean shutdown |
| `runs/keyring-stage-a-verified-01` | 50 assertions, 0 failures, clean shutdown |
| `runs/orb-bg-lifecycle-verified-01` | 19 assertions, 0 failures; Orb and Native Wait |
| `runs/orb-keyring-lifecycle-verified-01` | 19 assertions, 0 failures; Orb and Native Wait |
| `runs/third-party-collision-02` | 3 assertions, 0 failures; occupied native shape is preserved |
| `runs/si-sifixes-regression-01` | 10 assertions, 0 failures; natural SI opening, conversation, movement/save/load |
| `runs/jawbone-verified-02` | 6 assertions, 0 failures; original SI Jawbone/tooth insertion, removal, rendering and save/load; BG case unavailable in SI |

Physical coverage includes the initial one-stone insertion/removal proof, all
eight fixed compartments, original object identities and small sprites, six
representative drop positions (lid, corners, wood edges, and another occupied
slot), closed-case insertion, rejected gold/duplicate, all eight removals,
empty/filled case reloads, transfer between ordinary containers, ground placement,
ground save/load, world double-click and ordinary pickup with all contents.
Pre-marked unit fixtures include different x/y/z destinations and map 0/1 values.
They certify physical retention of that data, not real spellcasting or acquisition.

Actual engine images in each verified physical run include `empty-case.png`,
`one-stone.png`, `eight-stones.png`, `reloaded-case.png`,
`world-case-eight-stones.png`, and `manual-review-case.png`.
`case-checks.json`, `result.json`, engine logs and executable/resource hashes
identify the actual observations. No desktop windows or global input were used.

The engine's original Keyring Usecode is unchanged; the effective original and
Keyring Orb profiles were verified, including physical chart behavior and Native
Wait opening/cancellation. This is focused regression evidence, not a complete
playthrough of every Keyring quest. Accepted audio/combat implementations and
original stone/spell Usecode files are unchanged; no new listening test is claimed.

The Jawbone check uses an explicit real-item fixture after normal SI opening and
a disposable SI Fixes checkpoint. It verifies the original gump, tooth object
identity, ordinary sprite on removal, tooth-count frame changes and save/load.
It does not certify natural late-game acquisition. Original Jawbone sources and
SI content rules are unchanged.

## Build and automated reproduction

From this worktree:

```powershell
./tools/virtue_case/build.ps1
# Optional full clean rebuild, with the same ordinary configuration:
./tools/virtue_case/build.ps1 -Rebuild
$env:EXULT_TEST_EXE='C:/Codex/Ultima7 Decomp/build-exult-fork/bin/Exult.exe'
$env:EXULT_TEST_RUNTIME_DIR='C:/Codex/Ultima7 Decomp/Exult-Fork-Play'
$env:EXULT_TEST_DATA_PATH='C:/Codex/Ultima7 Decomp/build-exult-fork/bin/data'
$env:EXULT_TEST_BG_PATH='C:/GOG Games/Ultima 7'
$env:EXULT_TEST_SI_PATH='C:/GOG Games/Ultima 7 - Serpent Isle'
$env:EXULT_TEST_MODS_PATH='C:/Codex/Ultima7 Decomp/experiments/wait-phase1c-20261031/mods'
$env:EXULT_TEST_OUTPUT='C:/Codex/Ultima7 Decomp/test-output/virtue-native'
python tools/virtue_case/checks.py --run my-fresh-bg-case-run
python tools/virtue_case/checks.py --mod Keyring --run my-fresh-keyring-case-run
python tools/virtue_case/collision_check.py --run my-fresh-collision-run
python tools/virtue_case/jawbone_checks.py --run my-fresh-jawbone-run --checkpoint 'C:/Codex/Ultima7 Decomp/test-output/virtue-native/runs/si-sifixes-regression-01/saves/mods/sifixes/saves/exult00si.sav'
```

The build wrapper uses the existing compiler configuration, binary output and
intermediate tree, preserving distinct per-project directories. Do not build
another worktree concurrently into that tree. Its `QuizBuildDir` also reuses the
ordinary tree. No separate test engine or alternate game rules were introduced.
Known upstream warnings and the existing postbuild DLL-copy path warning remain;
the verified runtime DLL path is supplied explicitly above.

Native source changes are `native_virtue_case.cc/.h`, `shapeid.cc`,
`objs/contain.cc`, `objs/objnames.cc`, and the four-line lifecycle correction in
`gamewin.cc`. Build manifests are `Makefile.am`,
`msvcstuff/vs2019/Exult.vcxproj`, and `ios/Exult.xcodeproj/project.pbxproj`.
CLI support is in `cli_world.cc`, `cli_driver.cc` and
`tools/test_cli/driver.py`; focused build/tests live in `tools/virtue_case`.

## Manual review and stopping point

The isolated review runtime is
`C:/Codex/Ultima7 Decomp/test-output/virtue-native/human-review/`.
Run `Play Stage A.cmd` yourself, then load slot 3 (`CLI disposable checkpoint`).
The save contains an explicitly synthetic physical-case fixture, not commission
progress. Its test destinations are synthetic; this review is for physical
handling and visuals, not spellcasting. All configuration, gamedat and saves
stay in this review root; original game directories remain asset inputs.

Open the Avatar's inventory and the backpack in the free hand. It contains the
case. Double-click the case, drag any seated gem out into a visible container,
then drop it back on the lid or an unrelated recess. Check that it returns to its
own compartment. Try dropping an unrelated item or a duplicate stone on the case.
Move the case between containers, drop/pick it up in the world, and save/reload.
Ordinary mouse dragging is the product interface; the CLI is only a test harness.

STOP at Stage A. There are no MARK/RECALL controls, text-entry/label editing,
commissioning dialogue, materials/payment/timer, workbench handoff, companion
breadcrumbs or final approved artwork. Static blank parchment is decorative.
No human acceptance, release approval or migration of existing player saves is
implied. Immediate next step: human review of physical handling and provisional
artwork; later-stage implementation requires the separately scoped approval.

## Production-art integration validation

The focused `tools/virtue_case/checks.py` suite now uses the 320x200 geometry
and a 640x400 logical viewport so real source/destination containers stay visible.
It verifies 50 assertions each in ordinary BG and original Keyring with synthetic
pre-marked fixtures: insertion/removal, masks returning actual objects, all eight
slots, duplicate/nonstone rejection, empty/filled reload, nested containers,
world drop/reload/open/pickup, and unchanged original destination data.

`tools/virtue_case/presentation.py` copies the resulting disposable fixture save
into a new isolated run and captures the actual renderer. It checks three native
controls, no-op MARK/RECALL preserving identities/destinations, catch close/reopen,
display dimensions and exact integer nearest-neighbor pixel blocks. Review
profiles: BG 320x200 display 1x,640x400 display 2x,960x600 display 3x;960x400 ultrawide
Fit; Keyring640x400 display 2x. All use logical game/UI 320x200 and Point/Fit.

Correction to the earlier authoring-package scale advice: this branch's
`eff_ui_scale()` returns 1; plain `Centre` is 1x, and a `ui/scale` XML field does
not set that scale. Use `Fit` with exact integer display multiples for these
verified profiles. No user/global configuration was modified. Initial review
runs using Centre at larger windows tested controls but were not 2x/3x evidence;
they are superseded by corrected Fit captures. A mistyped Keyring checkpoint
path was also corrected before that presentation run started its engine.

Resource checks independently reconvert all 14 PNGs byte-identically to the
supplied SHP, and compare all 18 pre-existing BG FLEX entries byte-for-byte.
A collision fixture verifies an occupied third-party shape 1601 is not claimed.
Original game directories remain read-only inputs. Real spellcasting, label
editing, quest acquisition, held-button screenshots and human art acceptance
are not certified by this presentation integration.
