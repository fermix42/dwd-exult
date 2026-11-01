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
The public source repository is https://github.com/fermix42/dwd-exult. The owner
renamed it and made it public; this does not announce or publish a DWD binary release.

## 2. Installation and upgrading

The Windows x64 installer defaults to `C:\Games\DWDExult` on the Windows
system drive. Keep the standard directory page to choose another writable location;
upgrades remember the previous DWD location. Installation uses ordinary-user
privileges without weakening permissions. The MSVC v14 x64 runtime is required
and is not bundled. No proprietary game files, soundfonts or Roland ROMs are supplied.

Use the wizard to browse for and validate your existing Black Gate and Serpent Isle
directories. These are read-only asset sources shared with official Exult. DWD has
its own registry/uninstall identity. Never install over official Exult or inside an
original game folder. No original assets are duplicated for separation.

Extract the portable ZIP's complete `DWDExult` folder into a writable location.
Use the accompanying installer in **Setup Paths Only** mode, targeting the extracted
folder, to select game sources without manual configuration editing.

Both distributions always use the executable directory as their root, including
Explorer, shortcuts and unrelated working directories. No `-p` is needed. A
nonwritable root reports an error without profile/CWD fallback. Owned configuration
paths use relocatable `<HOME>` values:

| Location below DWDExult | Contents |
| --- | --- |
| `exult.cfg`, root logs | Configuration and redirected diagnostics |
| `data/`, `temp/` | Resources/downloaded audio and runtime temporary files |
| `mods/<game>/` | Installed mods and their patches |
| `patches/<game>/` | Base patches and source workspace |
| `saves/<game>/`, `gamedat/<game>/` | Base saves and working state |
| `saves/<game>/mods/<id>/`, `gamedat/<game>/mods/<id>/` | Separate mod saves/state |

Games include `blackgate`, `serpentisle` and expansion/custom game identities.
Mod overrides cannot redirect writable state into original assets or mod packages.
Moving DWD preserves owned relative paths; moving original assets requires rerunning
path setup. Explicit developer CLI runs keep their disposable test-root contract.

The installer retains automatic all-in-one digital music/effects download into
`data`. Original Keyring and SourceForge Island install under `mods/blackgate`;
original SI Fixes installs under `mods/serpentisle`. These three are built from
accepted checked-in sources and also supplied as optional ZIPs under `mods`.
Original Ultima VI Remake and Glimmerscape retain their upstream download sources
and install under the respective game mod roots. Download hashes, original package
bytes, documentation and credits are preserved. Inno's transient bootstrap/download
staging uses Windows temporary storage; persistent data and engine runtime temp
belong to DWD. Extracting the ZIP does not automatically enable its optional mods.

Optional content has its own identity and version, independent of the engine.
An upgrade does **not** download or reinstall content just because its checkbox
remains selected. A new **Optional content status** page and Ready summary identify
missing packages, verified installed packages, customized/damaged or unverified
installations, newer installed packages and available updates. Selected missing
packages install normally. All existing packages are preserved by default,
including partial installations, customized mods/audio and third-party versions
that cannot be established reliably. An engine-only upgrade works offline; newly
selected missing downloads still need connectivity.

To repair or update existing content, keep its component selected, explicitly
check it on the status page, and confirm the replacement warning. The installer
backs up the existing mod folder and affected files under
`content-backups/<package-id>/<timestamp>/` before invoking native installation.
Customizations may be overwritten or removed from the active package by this
explicit operation; the backup retains them. Separate saves are not deleted.
For unattended use, supply `/REINSTALLCONTENT=<comma-separated-package-ids>` plus
`/ALLOWCONTENTREPLACEMENT=yes` to acknowledge that warning. IDs are
`audio-original`, `keyring-upstream`, `sifixes-upstream`, `sfisland-original`,
`ultima6-original`, and `glimmerscape-original`. Without acknowledgement an existing
package replacement is refused. Linked content requires manual management.
For unattended `/BGDIR` and `/SIDIR`, use canonical Windows backslash paths
(e.g. `C:\GOG Games\Ultima 7`), or omit those options when the saved paths are
unchanged. An existing command-line limitation rewrites configuration when
forward-slash aliases are supplied, losing formatting/comments. This also affects
the prior accepted installer; mod files and isolated saves remain preserved.


Verified per-file manifests and independent receipts under `content-state/` record
the offered original packages. Legacy exact matches can be recognized without
reinstallation; unknown or changed files are preserved, not guessed to be older.
A damaged package is reported for repair rather than silently replaced. Uninstall
retains saves, configuration, receipts and backups. Future accepted DWD mod editions
must use distinct package/save identities and their own version/release records;
none is created by this installer fix.

Back up configuration and saves before upgrading. Reinstallation, mod updates and
mod removal preserve separate saves. Uninstall retains configuration and saves.
Legacy external writable overrides are rebased; old saves are not automatically
copied. Import backups deliberately into a disposable installation and evaluate
compatibility. Original game directories are never migration targets.

## 3. Features and enhancements

### Audio (Black Gate and Serpent Isle)

Use Audio Options to select music and effects independently. The added music
backends are opt-in; existing configurations retain their chosen settings.
Disable digital music when auditioning a synthesized music backend.

- **OPL2 (SB/ADLIB)** plays the original game's FM music
  using its game-specific sequencing and instruments. Black Gate includes its
  introduction, ending and credits behavior; Serpent Isle has its own OPL2 path.
- **Enhanced OPL3 Stereo** provides the accepted enhanced stereo arrangement rather than
  reproducing the original mono OPL2 output. Guardian introduction percussion and
  enhanced notes are included. Choose another music driver to disable it.
- **MT32EMU** is the single emulated Roland music option, retaining Exult's
  MT32Emu playback implementation. Supply legally obtained compatible control
  and PCM ROMs in the data directory using `MT32_CONTROL.ROM`/`MT32_PCM.ROM`
  or `CM32L_CONTROL.ROM`/`CM32L_PCM.ROM`. The existing CM-32L-first automatic
  preference and split `MT32A.BIN`/`MT32B.BIN` control-ROM loading remain available.
  Unpublished legacy DWD Roland selections map to this option on startup.
  Missing/incompatible ROMs retain the existing initialization/fallback behavior;
  the Audio Options warning identifies failure to activate MT32Emu.
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
| `audio/midi/driver` | `OriginalOPL2` (OPL2), `OPL3Stereo` (Enhanced OPL3 Stereo), `MT32Emu` (MT32EMU), or an existing upstream driver; retained IDs/defaults stay stable |
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
testing. The installer offers Marzo's Keyring 0.11.25, SI Fixes 0.15.15 and
SourceForge Island as downloads from the official Exult distribution. New optional
installations need internet access. Neither the installer nor the portable binary
ZIP contains these three archives or their installable payloads. Portable users
can obtain the official ZIPs from https://exult.info/download.html and use the
ordinary `Exult.exe --installmod <ZIP>` workflow with their intended configuration.
Upstream mod source trees remain in Git and the corresponding-source ZIP; that
source distribution includes `content/islefaq/patch/equip.dat`.

Downloads are pinned to locally computed SHA-256 hashes of verified official HTTPS
archives, not publisher-issued checksum claims. A changed remote archive is rejected.
Existing, customized, damaged or version-unknown content is preserved during engine
upgrades, including when its checkbox remains selected or is deselected. Repair
requires the separate explicit replacement acknowledgement; backups are retained
and isolated saves are never deleted. Without new optional content, installation
works offline. A failed optional download is reported and skipped while the engine
installation continues. An extraction/manifest failure removes partial content and
restores the prior package backup when present; no success receipt is written.
Retry a missing package by selecting it, or request explicit repair for an existing
package. All mod authorship, credits and original editions are preserved.
Current Keyring and SI Fixes remain original editions. Future DWD editions
require independent acceptance, versions, identities and save compatibility review.
The planned Virtue Stone case is not implemented. No mod is merged into original game data by this release preparation.

## 7. Known limitations

Only Windows Release/x64 has local build evidence. Other platform projects are
source compatibility work, not verified DWD platform support. This candidate is
unsigned, as permitted by this free project’s zero-budget policy. Windows may show
SmartScreen or unknown-publisher warnings. Verify SHA-256 before running it.
Free GitHub provenance attestations are prepared for approved public releases;
they are not Windows Authenticode signatures and do not prevent SmartScreen warnings. The corresponding-source ZIP includes matching dependency archives, patches and
vcpkg recipes. A clean Windows VM test remains an additional confidence item.
The owner completed scoped installer, BG/Keyring, SI/SI Fixes and Enhanced OPL3
with synthesized OPL2 effects acceptance; see [validation](packaging/dwd/VALIDATION.md). Native quiz encounters, some late-game Orb story states and
natural SI Fixes Batlin vision have the unperformed-test limits recorded in the
feature documents. Owner acceptance applies to the tested local candidate and those observations; it
does not certify every backend, gameplay feature, platform or installation scenario.

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
