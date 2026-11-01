# Dark Wraith Dragon's Exult

**Unpublished 1.0.0-rc.1 candidate documentation. No DWD version has been publicly
released.** Authoritative identity and upstream provenance are in
[packaging/dwd/version.json](packaging/dwd/version.json); each candidate includes
`release-metadata.json` identifying its exact build commit.

## 0. About Dark Wraith Dragon's Exult

DWD Exult is Dark Wraith Dragon's independently maintained Exult fork. It runs
Ultima VII: The Black Gate and Ultima VII Part Two: Serpent Isle using game files
you supply. The original Exult Team created the engine on which this work depends.

## 1. Relationship to the official Exult engine

This candidate is based on Exult's **1.13.1git development version**, not an official
stable release. It preserves the familiar interface and `Exult.exe` name. The
original [README](README), [ChangeLog](ChangeLog), [AUTHORS](AUTHORS), [COPYING](COPYING),
[INSTALL](INSTALL) and [FAQ](FAQ) remain the upstream references. DWD additions below
supplement their descriptions, including the original README's audio choices.
The proposed project address is https://github.com/fermix42/dwd-exult; repository
renaming and publication have not been approved.

## 2. Installation and upgrading

The locally prepared Windows x64 ZIP is for review. Extract its entire `DWD Exult`
folder to a writable location and keep `Exult.exe`, DLLs and `data` together. This
Windows build keeps `exult.cfg` beside the executable; its configuration and save
paths must remain separate from an official installation. Configure paths to your
own BG/SI game installations using Exult's normal setup. No game files, soundfonts
or Roland ROMs are supplied. Do not extract over an official Exult installation.

The prepared Inno installer uses its own DWD application, registry, Start Menu and
uninstall identity and defaults to a writable per-user directory. **Installer
compilation and install/upgrade/uninstall behavior still require validation.** It
retains upstream path setup and optional mod/audio installation code. These options
can write to selected game/mod directories; review your paths and back up mods.

Back up your configuration and saves before upgrading. Copy saves deliberately
into a disposable profile first; do not assume that a save written by this fork
can be safely taken back to every upstream version. Uninstall must preserve player
saves and external game data; that requirement awaits an installer behavior test.

## 3. Features and enhancements

### Audio (Black Gate and Serpent Isle)

Use Audio Options to select music and effects independently. The added music
backends are opt-in; existing configurations retain their chosen settings.
Disable digital music when auditioning a synthesized music backend.

- **Original OPL2 - AdLib / Sound Blaster** plays the original game's FM music
  using its game-specific sequencing and instruments. Black Gate includes its
  introduction, ending and credits behavior; Serpent Isle has its own OPL2 path.
- **OPL3 Stereo** provides the accepted enhanced stereo arrangement rather than
  reproducing the original mono OPL2 output. Guardian introduction percussion and
  enhanced notes are included. Choose another music driver to disable it.
- **Original MT-32 - Early**, **Original MT-32 - Later**, and **Original CM-32L /
  LAPC-I** select explicit Roland models and original game music behavior. You
  must supply legally obtained, matching stock control and PCM ROMs in Exult's
  data directory. An unavailable model/bank is reported rather than silently
  substituting a different original music profile. Upstream MT32Emu remains a
  separate option; its automatic ROM preference is not explicit model selection.
- Synthesized **AdLib/FMOpl** and **MT-32/MT32Emu** sound effects use original
  game resources, including synthesized introduction effects. The effects driver
  defaults to `digital`; choose digital effects to return to sound packs. Missing
  original effects can fall back to available digital effects. Roland synthesis
  still requires suitable user-supplied ROMs.
- **Music Player** in the main menu auditions available tracks from configured
  BG/SI installations. Previous/Next, Play/Stop and Audio Options use normal audio
  settings; the player does not write a separate set of preferences. Game switching
  requires the other game to be configured. Availability depends on game banks
  and the selected backend.

Audio settings do not introduce a new save format. Mods with custom music/banks
need individual checking; original-game playback fidelity is not a promise that
every custom track uses the same instrument or sequencing assumptions.

### Video and display (both games)

Video Options adds borderless windows and Normal/Left/Right placement, with
aspect-correct borderless sizing. Borderless defaults to off and position to
Normal. These are display preferences and do not change saves or mod data.

### Gameplay and interface

- **Native Wait**, available in both games by default: Shift-W opens a scroll to
  choose 1–24 hours. Arrows/Home/End adjust, Enter/checkmark confirms, Escape/No
  cancels. It needs no watch or cheat mode. Combat, nearby threats, hunger,
  harmful conditions, cold, scripts or unsafe activity may refuse or interrupt it.
  Waiting ages supported fuel and party effects and reconciles schedules; it
  provides no sleep benefits and does not fast-forward arbitrary Usecode. Plain W
  remains the watch. This adds saved timer/phase information: back up saves before
  trying an older engine. See [game-time compatibility](docs/game_time_events.md).
- **Quiz slider defaults**, always enabled in BG and released SI: the built-in
  numeric quizzes initially select their correct answers. You can still choose a
  wrong answer; questions, validation, retries and consequences remain intact.
  No configuration switch or separate mod is needed. Normal mod overrides retain
  precedence. SI beta is excluded. See [verified scope](docs/quiz_sliders.md).
- **Orb of the Moons paper chart**, original BG and the verified English Keyring
  layout: Lord British gives the chart with the Orb and offers an “Orb chart”
  replacement afterwards. Double-click it, then select a printed destination or
  rune to cast the possessed Orb in that direction. Center compass/Escape cancels;
  without an Orb you can still read it. Manual Orb targeting remains available.
  The chart is a saved inventory object using reserved BG shape 1600. Existing
  saves can obtain one from Lord British. Unknown bytecode or conflicting shapes
  are guarded; unsupported layouts retain manual activation. SI has no chart.
  No external artwork or game-data replacement is installed. See
  [layout and compatibility details](docs/orb_chart_art.md).

### Developer automation (both games)

The same ordinary executable contains explicitly activated `--test-cli` automation.
It is inactive during normal play and requires a disposable run profile. It is not
an additional player mode or the Native Wait action. See [test CLI](docs/test_cli.md).
Dormant combat/execution-context research branches are excluded from this candidate.

## 4. Configuration options and controls

Prefer the normal Audio and Video Options dialogs. Equivalent `exult.cfg` values:

| Path under `config` | Values / default |
| --- | --- |
| `video/borderless` | `yes`, `no` (default `no`) |
| `video/window_position` | `normal` (default), `left`, `right` |
| `audio/midi/driver` | `OriginalOPL2`, `OPL3Stereo`, `MT32EmuEarly`, `MT32EmuLater`, `MT32EmuCM32L`, or an existing driver; existing/default selection retained |
| `audio/effects/driver` | `digital` (default), `FMOpl`, `MT32Emu` |

Custom key files can bind `WAIT` to replace Shift-W. `ORB_CHART` is initially
unbound; for example `w ORB_CHART` replaces the normal W binding and requires a
chart in party inventory, including nested containers. No DWD release preparation
changes your existing personal settings.

## 5. Gameplay differences and compatibility

Native Wait, quiz defaults and BG's chart are the deliberate gameplay/interface
differences in this candidate. Original game files are read as inputs; embedded
compatibility code loads before normal mod patches. Keep backups for save rollback,
especially timer metadata and the chart object. Do not use chart-bearing saves in
mods that assign BG shape 1600 to another purpose.

## 6. Compatibility with existing Exult mods

Normal mod selection and installation remain available. Existing evidence covers
English BG/Forge of Virtue, SI/Silver Seed, Keyring 0.11.25 and SI Fixes 0.15.15 for
the focused scenarios recorded in the linked feature documents. This is not a full
campaign certification. Mods can override embedded quiz/light helpers; unknown Orb
layouts are guarded. Other languages, mod versions and custom game rules require
testing. No mod is merged into original game data by this release preparation.

## 7. Known limitations

Only Windows Release/x64 has local build evidence. Other platform projects are
source compatibility work, not verified DWD platform support. This candidate is
unsigned. Installer behavior and matching third-party source distribution remain
publication gates. Native quiz encounters, some late-game Orb story states and
natural SI Fixes Batlin vision have the unperformed-test limits recorded in the
feature documents. Fresh listening and human gameplay review of the final package
remain necessary; prior acceptance is not a new test of this exact binary.

## 8. Reporting bugs and contributing

Until publication, report issues to the maintainer through the existing private
project channel. Include `release-metadata.json`, game/mod version, reproduction
steps and relevant logs. Keep ROMs, proprietary game files, personal paths and
credentials out of reports. Proposed future issues URL: the project's `/issues`.
Contributors should read [AGENTS.md](AGENTS.md) and preserve accepted implementations.

## 9. Credits and acknowledgments

Thanks to the Exult Team and all authors listed in AUTHORS, Marzo Sette Torres
Junior and mod contributors, and the authors of SDL, Munt/mt32emu, FluidSynth,
Nuked OPL3, Ogg/Vorbis, libpng, zlib and other components. DWD maintenance is by Dark
Wraith Dragon. Retain all individual source notices and bundled dependency licenses.
The engine is distributed under the upstream GPL terms in COPYING; component
licenses remain applicable. Ultima VII and Roland resources remain user supplied.

## 10. Version history reference

See [ChangeLog-DWD.txt](ChangeLog-DWD.txt) for DWD release history and [ChangeLog](ChangeLog)
for independent upstream history. The README describes this candidate's behavior;
it does not duplicate the chronological release record.
