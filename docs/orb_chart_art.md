# Orb of the Moons chart

The original BG and Keyring artwork is visually approved. Native and 2x previews
are actual engine renderer captures in disposable CLI runs. The rune artwork,
parchment, fonts, compass and directional composition are frozen; subsequent
interaction verification changes only invisible hit margins when needed.

## Using the chart

Lord British supplies his folded chart with his Orb. After the Orb has been given,
his ordinary conversation offers a free “Orb chart” replacement, without another
Orb. Double-click the chart to read it; select a printed name or rune to cast the
possessed Orb in that direction. The center compass and Escape cancel. Without an
Orb, the chart remains readable and cannot travel. The Orb’s own double-click
retains manual targeting.

The optional, initially unbound `ORB_CHART` action uses existing Exult key bindings
(e.g. `w ORB_CHART`) and requires the chart in party inventory, including nested
containers. A reachable ground chart can be read directly. The chart is an ordinary
saved inventory object (reserved BG shape 1600, frame 0); no save migration or
external assets are needed. Existing saves acquire it through Lord British.

Unknown Orb/helper bytecode retains manual activation. Layout detection uses the
effective loaded functions, including patch overrides, rather than a mod name.
The two supported English data sets are original BG and the locally verified
Keyring build. A conflicting existing shape 1600 is never overwritten.

## Authoritative assets

The eight engraved runes come from original BG `static/shapes.vga`, shape 877,
frames 0–7. Their associations are confirmed by
`content/bgkeyring/src/items/shrines.uc`; `bg_shapes.uc` independently identifies
Honor as frame 6. These are the game engravings, including their original pixel
proportions, not a transcription of symbols in the supplied composition reference.

| Virtue | Original frame |
| --- | --- |
| Honesty | 7 |
| Compassion | 5 |
| Valor | 4 |
| Justice | 1 |
| Sacrifice | 0 |
| Honor | 6 |
| Spirituality | 3 |
| Humility | 2 |

Extracted shape 877 SHA-256:
`d316b421453bdef5cf06df1cf16335d097eaa30b3998c9216abec2c7707b2b02`.

`native_orb_chart.cc` crops engraved pixels from the stones and renders them in
BG's brown ink ramp. Retail marks use integer 3x rune pixels; Keyring's inner and
outer marks use 1x and 2x. Stone backgrounds and outlines are excluded. Original
BG font 0 supplies the heading; the native small black font supplies all location
and Virtue names. The parchment is original BG scroll gump 55, widened between
cropped endposts and recolored with BG palette entries 134–144.

The central eight-point rose now uses filled compass points around a circular ankh hub.
Keyring town names are single-line, high-contrast BG lettering with separate small
rune stamps; two faint distance circuits distinguish towns from outer shrines.
Retail retains its spacing, with restrained bearing lines around the rose. There
are no cell borders, illustrations, external fonts, animations, or new effects.
The folded inventory sprite is unchanged.

## Layout and gameplay boundaries

The retail compass remains 3x3, with Moonglow permanently northwest. Keyring
retains its 5x5 directional arrangement: eight inner towns, eight outer shrines.
The outer labels use Virtue names under the heading “Shrines of Virtue”.
Spirituality remains printed regardless of story state. Town rune associations
follow the actual towns, including Keyring's switched Skara Brae/New Magincia
positions. The sample image supplies composition direction, not destination data.

This revision changes presentation and its printed-mark hit regions. The item,
ownership, save/load, Orb activation, target handoff, compatibility checks and
Lord British integration are unchanged. The source from `Target_scope` through
the end of `native_orb_chart.cc` has the same SHA-256 before and after this visual
revision: `e2bfed22d2975ee00401610a65a4f85b6a18012ec43ad2d0f3938c28c8987541`.

## Final verification — 2026-11-01

Ordinary Release/x64 build passed. No Native Wait, game-time or audio source was
changed. MSVC emitted the existing debug-option, signedness and manifest warnings;
its optional dependency-copy steps still report two missing paths. The ordinary
executable launches and completes the isolated runs normally with runtime DLLs.

| Check | Result / retained run evidence under `test-output/orb-chart/runs` |
| --- | --- |
| Native and 2x presentation | PASS, 204 assertions: `final-bg-native-2`, `final-bg-scaled-2`, `final-keyring-native-2`, `final-keyring-scaled-2` |
| Whole Keyring compass region | PASS: `final-keyring-compass-pixels-1`; all 625 points cancel |
| Inventory, nested backpack, physical double-click, hotkey, ground reading, save/load | PASS, 19 assertions each: `final-bg-inventory-3`, `final-keyring-inventory-3` |
| Actual Lord British handover and chart-only recovery | PASS, 13 assertions each: `final-bg-handover-recovery-1`, `final-keyring-handover-recovery-1` |
| Original BG destinations | All eight actual arrivals: N/NE/E in `final-bg-travel-open-1`, remaining five in `final-bg-routes-2`; N also passed separately in `final-bg-britain-arrival-1` |
| Keyring destinations | PASS, 95 assertions: `final-keyring-routes-2`, all eight towns and eight shrines, actual gate qualities and native near/far distances |
| Unknown override / manual targeting | PASS, six assertions: `final-unknown-manual-1`; four helper-override assertions: `final-unknown-keyring-helper-1` |
| Loaded bytecode, both Time Lord states and pre-target plot guards | PASS, 33 BG + 49 Keyring assertions: `final-bg-plot-unit-2`, `final-keyring-plot-unit-2` |

Every final native/scaled PNG is byte-for-byte identical to its visually approved
`composition-*-review` capture. The only final usability change was a wider
invisible hit margin on Keyring town names and small runes. Ink, fonts, guide-line
masking, parchment and folded sprite are unchanged. Rune and name rectangles,
including transparent interior spaces and two-pixel town margins, resolve to the
intended native controls. Both are actually clicked through normal SDL/Gump input.

The user's original `exult02bg.sav`, titled “Talk to Lord British”, was copied into
isolated runs and remained byte-for-byte unchanged. It was loaded through native
save support under original BG and the supported Keyring resources. This verifies
the effective Keyring Orb/Lord British code on a progressed BG checkpoint; it is
not a playthrough of the whole Keyring campaign.

Initial throne-room and town-position casts encountered original clearance/path
failures. Those failed test setups are retained, not counted as arrivals. The
focused route harness now retries from nearby positions reached by ordinary native
walking; it never moves a gate, changes flags, creates an Orb, or teleports an
actor. All 24 destinations subsequently produced real arrivals. Original injury,
blocked-placement and path-failure behavior remains in the active Usecode. Every
observed success and refusal restored the chart's temporary target interception.
Keyring's unrepaired Spirituality cast actually arrived at Skara Brae, while
Moonglow remained independently reachable.

`tools/orb_chart/usecode_checks.py` fetches the effective functions from the running
engine, evaluates only pure destination helpers and pre-target guards with local
unit inputs, and verifies that real story/inventory state remains unchanged. Its
region-membership input is a unit boundary; the region helper itself remains the
fingerprinted original function. It never simulates gate placement or teleportation.
The repaired Time Lord destinations and broken-sphere/initial-Trinsic guards passed
these bytecode checks. **A naturally progressed repaired-Time-Lord or broken-sphere
save was not available; those late-game states were not played live.**

The initially unbound hotkey and Escape use existing key/Gump handling. There are
no new directional keyboard shortcuts. Existing accepted Native Wait evidence was
reused, with only its opening/cancellation checked during inventory integration;
no broad Wait or audio suite was repeated. Non-Windows build wiring is included
but was not compiled on this Windows host.

## Source and asset packaging

`native_orb_chart.cc/.h` contains the physical item, approved rendering, native
Gump buttons and scoped target handoff. Object activation/naming, shape-info
initialization, the unbound key action and two narrow Lord British conversation
hooks connect it to existing systems. The chart calls the possessed Orb's actual
`activate(double_click)` with the selected relative target; it contains no gate
creation or destination coordinates. Effective original/Keyring function
fingerprints prevent unsupported overrides from using the selector.

All required new art is generated at runtime from the player's original BG gump,
font and rune resources, plus the compiled folded-paper pixels. There are no new
external PNG/shape files to install or copyrighted game-data extracts to commit.
The automated scripts use the ordinary integrated executable and documented CLI
environment; disposable configurations, logs, captures and saves are not source.

The final compatibility review also includes Keyring's direct outer-container
helper (`0x945`) in its supported signature set. A valid override with an extra
unreachable return instruction is refused, while the actual supported Keyring
profile continues to open and save/load normally. This changes only unsupported
variant detection; the approved rendering and travel handoff are unchanged.
