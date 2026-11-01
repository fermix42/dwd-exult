# Repository instructions

These instructions govern development in this checkout. Preserve existing source
and user work; do not overwrite newer changes or edit generated
`docs/exult_intrinsics.md` directly. Change its website backend and regenerate it.
Protect accepted audio (including Enhanced OPL3 Stereo), personal configurations,
saves, installed game/mod resources, and isolated combat/execution-context/Native
Wait experiments. Testing a feature does not authorize implementing it, integrating
other experiments, or resuming paused work.

## Default automated gameplay testing

Use the integrated unified CLI by default for BG/SI, Keyring/SI Fixes and other
supported conventional mods: Usecode/scripts, NPCs/schedules/world behavior,
movement/pathfinding/collision, combat/targeting, inventory/object interactions,
conversations/dismissal, Gumps/modals, save/load and gameplay regressions.

- Prefer structured engine observations for identities, terrain, obstacles,
  reachability, combat relationships, dialogue and controls. Do not derive normal
  gameplay decisions from screenshots; reserve captures for genuinely visual tests
  of rendering, artwork, UI appearance, animation and scaling.
- Inspect current input ownership and legal actions. Use documented `continue`,
  `choose`, `dismiss` and native control activation as appropriate; different UI
  owners do not share a universal dismissal operation. Verify resulting state.
- Do not use Computer Use, desktop mouse automation, global keyboard injection or
  foreground-window manipulation for routine gameplay tests. Never silently take
  control of the desktop. Visual/interactive Computer Use requires a genuine need
  and explicit user authorization. If a required CLI operation is missing, report
  the gap and prefer a small reusable engine adapter within the authorized scope.
- Execute real engine actions. Dispatch/`ok` and clean process exit alone do not
  establish completion or PASS. Verify actual arrival, dialogue/UI ownership and
  assertions; distinguish PASS, FAIL and NOT TESTED. Cancellation is not success.
- Respect stale references after load/world replacement and bounded navigation
  uncertainty. Report unsupported operations rather than guessing. Never force
  quest/control flags, teleport actors or fabricate gameplay progress to pass a
  test. BG fast starts with `skip_intro=yes` do not certify natural opening progress.
- Use fresh disposable runs; preserve normal configs/saves and read-only resources.
  Rebuild changed components through the ordinary workflow, then run the smallest
  relevant checks. Retain accepted evidence; do not repeat full suites needlessly.

Exult has **one ordinary executable and build workflow**. The same compiled
`Exult.exe` provides normal gameplay and explicitly activated `--test-cli` mode.
Automation must stay inactive in normal gameplay. Do not create/maintain a separate
`exult-cli.exe`, second engine build, duplicate object-compilation tree, test-specific
engine compilation configuration, separate game rules, or network control service.
Isolated runtime configurations are required; they are not alternate engine builds.
CLI `wait`/`run_until` polls normal engine progression, not the future native Ultima
VII Wait gameplay action.

## Verified local build and launch

Run these commands from `C:\Codex\Ultima7 Decomp\exult-fork`. Detailed schemas,
completion semantics, supported scenarios and limitations are maintained in
[docs/test_cli.md](docs/test_cli.md) and
[docs/test_cli_protocol.md](docs/test_cli_protocol.md).

Ordinary Release/x64 build: `../build-exult-fork/build.ps1` defines the accepted
workflow. Its verified equivalent below uses forward-slash trailing directories to
avoid legacy Windows PowerShell quoting problems. Do not create a second build.

```powershell
$root = Split-Path (Get-Location) -Parent
$vs = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$oldCL = $env:_CL_
try {
    $env:_CL_ = ("$oldCL /Z7 /DPORTABLE_EXULT_WIN32").Trim()
    & "$vs/MSBuild/Current/Bin/MSBuild.exe" "$root/exult-fork/msvcstuff/vs2019/Exult.sln" '/t:Exult;expack' /m:8 /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 "/p:OutDir=$root/build-exult-fork/bin/" "/p:VcpkgRoot=$root/tools/vcpkg/" "/p:VcpkgInstalledDir=$root/build-exult/deps/" /p:VcpkgManifestInstall=false /p:VcpkgLocalAppDataDisabled=true "/p:DirectoryBuildPropsPath=$root/build-exult-fork/dependencies.props" "/p:DirectoryBuildTargetsPath=$root/build-exult-fork/dependencies.targets"
    if ($LASTEXITCODE) { throw "Exult build failed: $LASTEXITCODE" }
} finally { $env:_CL_ = $oldCL }
```

Configure the client for the verified current resource/output paths (adjust only
when installations move). The mod directory contains conventional compiled
snapshots; it is an input path, not a source/build dependency.

```powershell
$root = Split-Path (Get-Location) -Parent
$env:EXULT_TEST_EXE = "$root/build-exult-fork/bin/Exult.exe"
$env:EXULT_TEST_RUNTIME_DIR = "$root/Exult-Fork-Play"
$env:EXULT_TEST_DATA_PATH = "$root/Exult-Fork-Play/data"
$env:EXULT_TEST_BG_PATH = 'C:/GOG Games/Ultima 7'
$env:EXULT_TEST_SI_PATH = 'C:/GOG Games/Ultima 7 - Serpent Isle'
$env:EXULT_TEST_MODS_PATH = "$root/experiments/wait-phase1c-20261031/mods"
# Optional artifact root; otherwise the client uses OS temp/exult-test-cli:
$env:EXULT_TEST_OUTPUT = "$root/test-output/exult-cli"
python tools/test_cli/driver.py                       # Interactive JSON Lines
python tools/test_cli/driver.py --script scenario.jsonl
python tools/test_cli/navigation_checks.py --run my-fresh-navigation-run
# Select only scenarios relevant to the change; see docs/test_cli.md for others.
```

Start a client session with e.g.
`{"cmd":"init","game":"bg","run":"my-fresh-bg-run"}`. The frontend supplies the
isolated run directory, configuration and `.exult-test-run` marker and launches the
same executable with its required arguments. Never launch bare `--test-cli` as a
working test session, reuse an existing run name, or use player save/config paths.
Artifacts go under `EXULT_TEST_OUTPUT/runs/<name>`; do not commit them.

Only for explicitly authorized visible verification, the same executable can use
the existing disposable normal-smoke config, without the CLI flag:

```powershell
$env:PATH = "$env:EXULT_TEST_RUNTIME_DIR;" + $env:PATH
& $env:EXULT_TEST_EXE -p --bg --nomenu -c "$root/experiments/unified-cli/normal-smoke/exult.cfg"
```

Do not launch this visible test automatically. It has audio disabled and skips the
BG opening; it is not an audio test or proof that initial NPC story scripts ran.

## DWD release governance

- Dark Wraith Dragon's Exult (DWD Exult) is an independent fork, never an official
  Exult Team release. Keep `Exult.exe`, upstream history, credits, licenses and
  upstream-owned README, ChangeLog, COPYING, AUTHORS, INSTALL and FAQ intact.
- `packaging/dwd/version.json` is the authoritative DWD SemVer and exact incorporated
  upstream revision. Generate binary/installer/artifact metadata from it and the
  exact build commit; never label upstream development versions as stable releases.
  Use `dwd-vMAJOR.MINOR.PATCH` (including prerelease suffixes). Compatible fixes bump
  patch, compatible features minor, intentional breaks major; upstream updates do
  not automatically dictate a DWD increment.
- `README-DWD.md` describes features, configuration and behavior of the currently
  released DWD Exult version (explicitly mark unpublished candidates).
  `ChangeLog-DWD.txt` permanently records chronological changes between DWD releases.
  The README explains current behavior and use; the changelog records what changed,
  when and in which version. Review and update both whenever a release changes
  documented functionality. Neither substitutes for the other. Preserve original
  Exult documentation and changelog independently. Correct historical mistakes
  transparently; do not silently rewrite release history.
- Release only verified, accepted functionality. Preserve frozen gameplay/audio/art;
  exclude dormant experiments, private configs, saves, build products, backups,
  credentials, proprietary Ultima data and unlicensed ROMs from source and payloads.
- Prefer upstream build/distribution targets and Inno architecture. Use independent
  installer, directory, registry and uninstall identity; preserve mod workflows.
  Include original and DWD docs, exact source provenance, component notices and
  corresponding source. Stage from explicit file lists, never a player directory.
- Before release: build the exact clean tagged commit; run relevant automated and
  available isolated runtime checks; inspect archive and installer contents,
  dependencies, metadata, checksums, installation/upgrade/uninstall and source
  completeness. Compare with upstream packaging and record intentional differences,
  unperformed checks and required human gameplay/listening/signing checks. Existing
  accepted evidence remains valid; packaging work does not reopen frozen features.
- Never publish, rename the remote, change visibility, push final tags, merge the
  public default branch or rewrite shared history without explicit user approval.
  Manual candidate workflows upload review artifacts only. Publication requires
  separate approval of the exact commit, tag, assets and curated changelog notes.

### Zero-budget Windows release policy

DWD Exult is a free, noncommercial fan project with no release budget. Never buy
certificates, signing services, cloud subscriptions or other release services.
Unsigned Windows executables/installers are acceptable; publicly trusted
Authenticode signing is not a release requirement. Document unsigned downloads
accurately and retain SHA-256 checksums, malware checks and exact build provenance.
Prepare free GitHub artifact attestations for approved public-repository releases
where supported. Attestations establish provenance; they are not Authenticode
signatures and do not prevent Windows SmartScreen warnings. Do not publish or push
to obtain an attestation without the owner's explicit publication approval.

### DWD installation, portable paths and mod distribution

- Windows installer default: `{sd}\Games\DWDExult` (normally
  `C:\Games\DWDExult`). Keep directory selection enabled, preserve an existing DWD
  location on upgrade and never inherit official Exult identity/location. Prefer
  ordinary-user installation into a writable root; never loosen system-directory
  permissions. If the selected root is not writable, report the error.
- Both installed and ZIP builds are mandatory portable applications: executable
  directory is the normal application root regardless of CWD, shortcut or `-p`.
  Config/logs/data downloads/mods/patches/saves/gamedat/generated files stay beneath
  that root with relocatable owned paths. No user-profile/CWD fallback. Preserve the
  explicitly activated CLI's separate disposable run-root contract for tests.
- Original Ultima VII directories are read-only asset inputs shared with official
  Exult. Preserve the validated BG/SI browse wizard. Never put DWD files there,
  including through upstream defaults, mod-supplied paths or installer fallbacks.
  Base and mod saves/gamedat belong in separate root-owned trees, outside mod
  install/patch directories; mod update/removal and uninstall preserve saves/config.
- Preserve automated optional audio (including all-in-one), Keyring, SI Fixes,
  SourceForge Island, Ultima VI Remake and Glimmerscape installation/downloads,
  integrity/error handling and original attribution. All destinations are DWD-owned.
- Current Keyring/SI Fixes packages remain the original approved upstream editions.
  Future DWD editions require independently modified, accepted, documented and
  versioned releases, distinct package identities and isolated saves. Recommend
  actual accepted DWD editions by default when available without overwriting an
  original edition; evaluate compatibility before save migration. Do not relabel
  upstream mods, alter other third-party mods, or assume the uninvestigated Virtue
  Stone case needs engine/mod changes.
- For path/packaging releases test default/alternative/remembered installation,
  wizard validation, optional downloads, unrelated-CWD startup, relocation,
  nonwritable roots, base/mod save separation, updates/removal and coexistence.
  Compare files AND directories before/after in original game/official profile/
  mod/patch/install trees; use read-only resources where practical. Rerun existing
  installer/runtime regressions, report actual totals, rebuild exact-source assets
  and new checksums, retain Defender/provenance and genuine NOT TESTED gates.

### Accepted DWD audio selection policy

Player music selections include Enhanced OPL3 Stereo (`OPL3Stereo`),
OPL2 (SB/ADLIB) (`OriginalOPL2`) and MT32EMU (`MT32Emu`). Keep retained IDs stable.
The unpublished Early/Later MT-32 and CM-32L/LAPC-I music presets are retired;
accept their old IDs as aliases of MT32Emu, not additional selectable drivers.
Preserve retained MT32Emu synthesis and compatible conventional/split ROM loading,
independent synthesized SFX/music settings, upstream backends and external MIDI.
This scoped policy supersedes the experimental Roland preset choices only; all
other accepted audio implementations and Enhanced OPL3 freeze remain protected.

### Idempotent optional-content upgrades

Engine versions, DLL/resource refreshes and leftover component selections must
never imply optional mod/audio reinstallation. Detect installed content with
independent stable IDs, versions/releases and verified manifests/receipts.
Preserve unchanged, customized, partial/damaged, newer or unverified existing
content by default; do not guess that unknown third-party mods need replacement.
Offer status and updates clearly. Install only selected absent content automatically.
Repair/replacement/update of existing content requires an explicit request and
custom-file overwrite warning/acknowledgement; back up affected content and never
silently delete saves. Preserve existing config bytes when paths did not change.
An engine-only upgrade with installed optional content must work without network
requests. Future DWD Keyring/SI Fixes use independent versions and distinct original/
DWD package/save identities once actually accepted; no new editions are implied.
Test fresh selection, repeated/newer-engine upgrades, no-op HTTP and file/timestamp
preservation, new selections, unknown/custom/damaged content, explicit repair and
backups, offline operation, original assets and config/save preservation. Reuse
release facilities and retain exact-source artifacts/checksums; no unrelated
retesting or changes to protected engine/audio/mod functionality.

### Approved optional-download distribution and upstream retention

Keyring, SI Fixes and SourceForge Island must remain official HTTPS on-demand
installer downloads, never embedded archives or installable payloads in the Windows
installer or portable binary ZIP. Keep independently pinned archive/file manifests,
record checksum provenance, preserve selection/idempotent upgrades, and require
explicit acknowledged repair for existing content. Failed downloads must preserve
existing packages; failed extraction must remove partial content/restore backups
without deleting isolated saves. Engine-only installs must remain offline.
Keep corresponding mod source and upstream history, licenses and attribution.
The owner explicitly accepted retention of upstream `docs/usecode.dis`, historical
`docs/usedis.zip` and `content/islefaq/patch/equip.dat` (November 1, 2026). Record
provenance in READINESS.md; do not delete these files/rewrite history or repeat the
same publication hold. This acceptance does not establish every redistribution
right. Continue auditing independently substantiated secrets/private/proprietary
material newly introduced by DWD and stop on a new material publication risk.
