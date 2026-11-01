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
