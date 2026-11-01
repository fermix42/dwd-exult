# DWD release preparation and validation

This is an unpublished candidate. Do not push tags, publish, rename the repository,
change visibility or merge a default branch without the owner's explicit approval.

## Source selection

Preparation starts at `4639563a84df7c6cd4860f4fb94c019cde864c8b`, the clean
`codex/orb-paper-map` checkout. Its incorporated upstream tip is
`97984e9e0cbb4ccacaab0249da9c51b533818f74` (Exult 1.13.1git). The 21 fork commits
contain window/profile changes, accepted synthesis and music, unified CLI, Native
Wait, quiz defaults and Orb charts. Feature acceptance/evidence is retained in
AGENTS.md, docs/quiz_sliders.md, docs/orb_chart_art.md and docs/game_time_events.md,
and the audio acceptance commit e3ff08e1e. No dormant combat/context branch is
merged. No other checkout, save, configuration or game directory is a source input.

`origin` remains fermix42/exult-custom. Its `main` (6fbda9a8d) has unrelated history;
`origin/codex/window-options` (8000aec02) lacks recent local improvements. Use this
ancestry-preserving release branch as the eventual new default branch after review:
push it as a new branch and change GitHub's default branch explicitly. Retain the
old main/archive reference; do not force-push or merge unrelated roots. Renaming to
fermix42/dwd-exult is a separate approved operation. Remote refs were inspected
locally, not refreshed; recheck remote state before any approved push.

## Build and stage

`version.json` is authoritative. MSVC generates an ignored identity header before
compilation. The upstream engine VERSION remains separate for compatibility.
Other build systems must run `python packaging/dwd/release.py metadata` before
compilation to include DWD console identity; their builds are not validated here.
Dirty-tree candidates are refused. A published revision cannot embed its own future
commit SHA in tracked documentation: exact DWD provenance is generated at build time.

Local ordinary build: use AGENTS.md's existing build command; do not create another
engine build. The hosted script uses the upstream MSVC solution and pinned vcpkg
baseline. Candidate workflow runs only manually, with read-only build permissions.
It has not been run on GitHub. It uploads inspection artifacts, never a public
release. The candidate workflow has no release attachment/publication job or contents-write
permission. Public release creation/attachment is a separate owner-approved action.

Example (use fresh output directory):

```powershell
python packaging/dwd/check.py
python packaging/dwd/release.py package --binary-dir ../build-exult-fork/bin --runtime-dir ../build-exult-fork/bin --dependency-share ../build-exult/deps/x64-windows/share --output ../test-output/dwd-release/candidate-1
```

For installer compilation additionally supply `--installer-tools <directory>` with
source-built `exconfig.exe` only, plus `--iscc <Inno Setup 6 ISCC.exe>` and
`--dependency-sources <verified source cache>`. Do not stage mod ZIPs or player
content. `downloads.json` pins official optional archives, while committed
`content-manifests` supply independent installed-content identities. The three
original upstream mods are downloaded only when selected and needed; they are
absent from both Windows binary packages. Their source remains in Git and the
corresponding-source ZIP. The generated installer retains the upstream game-path
wizard and native extraction with DWD preservation/repair and failure handling.
Do not claim a ZIP is an installer.

## Comparison with upstream packaging

Baseline: this exact upstream revision's Makefile.mingw `dist`/`disttest`,
win32/exult_installer.iss, archs_include.iss and docs/release.txt. The actual official 1.12.1-1 installer manifest was inspected with innounp
2.71.3, alongside the exact upstream recipe comparison. All 30 upstream nonbinary
application paths are present in DWD; binary/toolchain differences below are intentional.

| Area | Upstream recipe | DWD candidate |
| --- | --- | --- |
| Engine | Exult.exe and architecture DLLs | Same executable name; MSVC x64 only |
| Resources | exult, exult_bg, exult_si FLX | Same three generated engine archives |
| Docs | Original text, HTML, images, keys, licenses | Preserved; adds DWD README/history, INSTALL, metadata, notices |
| Installer | Inno, architecture macros, paths, optional mods/downloads, uninstall | Generated from full upstream script; separate AppId, HKCU registry, group, ordinary-user {sd}\Games\DWDExult default |
| Profile | Official profile | Executable-root storage mandatory; owned paths relocatable |
| Mod support | Source-built optional mod ZIPs and exconfig | All three built from committed sources; embedded in installer and supplied under portable mods/ |
| Platforms | Multiple upstream builds | Only local Windows Release/x64 evidence; no ARM/x86/Linux/macOS support claim |
| Publishing | Exult infrastructure, snapshots, notifications | Inherited YAML renamed .disabled; only manual DWD review workflow active |
| Source | Upstream source distribution | Exact clean engine source, Git-free metadata, verified external dependency sources, SPDX inventory, patches and vcpkg recipes |

All 16 inherited workflows were disabled intact, including CI owner gates,
scheduled snapshots, SourceForge publishing, Discord notifications, Coverity,
VirusTotal and release handlers. Their original source remains reviewable; no
Exult Team credentials or infrastructure are needed by the DWD workflow.

## Publication gates

- Compare the actual built installer against an appropriate official release,
  extract/inspect all files and exercise fresh install, upgrade, paths-only,
  optional downloads/mod installation and uninstall in an isolated environment.
- Verify installed launch/config/save ownership and preservation of official Exult,
  proprietary resources and saves. Check DLL imports, runtime redistribution and
  the MSVC runtime prerequisite on a clean machine.
- Distribute the generated matching source ZIP beside binaries, including external
  dependency archives/recipes and component licenses. DLLs remain replaceable.
  Windows API/UCRT and the MSVC v14 runtime are system/toolchain prerequisites,
  not GPL dependency-source archives. No app-local Microsoft CRT binaries are bundled.
  The proposed source URL is not currently a published availability promise.
- Run focused semantic CLI runtime checks on the exact packaged binary, plus human
  visual/audio/gameplay review as appropriate. Do not reopen accepted limitations
  or claim full campaign/platform testing from focused historical evidence.
- Build/inspect the unsigned installer, review final curated notes and checksums, record
  tag/commit identity and approve exact assets before public publication.

No candidate workflow job creates tags, releases or attachments. It does not turn
local gameplay results into hosted PASS fields. Review actual hosted outputs before
separately authorizing any distribution.

## Zero-budget follow-up

Unsigned Windows builds are approved by permanent project policy. Authenticode is
not a gate, and no paid service is required. SHA-256, exact source provenance and
Defender scanning remain required. The optional public/tagged GitHub attestation
job uses actions/attest@v4 with a dedicated OIDC/attestation permission scope. It is
free for public repositories and is not a Windows signature or a SmartScreen bypass.
No attestation has been generated locally; hosted execution requires approval to
push and run the branch. Release creation/attachment requires separate approval.

`build-mods.py` follows the checked-in upstream Makefile.mingw install file lists,
using ordinary UCC/expack in disposable staging. It emits required directory ZIP
entries for native --installmod. `collect-dependencies.py` checks installed DLLs and
recipes against their installed SPDX hashes, then fetches exact SHA-512-verified
archives. The source ZIP includes these inputs and the full pinned vcpkg script tree.
For ordinary versioned-registry rebuilding, clone vcpkg at the recorded revision;
the included source archive is not a Git repository. No offline build is claimed.

The local install regression uses `installer-check.py`, same installed executable,
disposable mods/configs/saves and protected-file hashes. It is a host-isolated test,
not a fresh Windows VM. Scoped owner human acceptance is recorded in VALIDATION.md;
clean-OS prerequisites and hosted execution remain untested. Do not repeat completed
owner tests absent a relevant implementation change.

## Mandatory storage and additional tests

Normal operation anchors storage to the physical executable directory before any
writes. Preflight rejects roots inside original game trees and asset sources
inside managed subtrees. Original game/static roots are protected read-only inputs.
Installer and configuration helper share validation. Mod saves/gamedat use separate
owned game/ID directories. Nonwritable roots fail without fallback. Inno itself
uses Windows-managed transient bootstrap/download staging; persistent application
content and engine runtime temp belong to DWD. System permissions are not loosened.

Run portable-check.py for ordinary startup, relocation, package bytes, overlap and
nonwritable-root checks; download-check.py for actual installer downloads; and
default-shortcuts-check.ps1 for no-/DIR default and actual shell shortcut launches.
Use fresh work folders and protect both game trees, official install and profiles.
Run installer/download/default tests sequentially (shared DWD registry identity).
Use --downloads with verified archive cache for byte comparison, not substitution
for actual Inno downloads. Protected snapshots include files AND directories.

## First hosted candidate and approval staging

Follow [READINESS.md](READINESS.md). The workflow is manual-only and checks the
required `expected_revision` input against both checkout HEAD and GITHUB_SHA.
Leave `tag` blank for the first review. The checked-out SHA also identifies binary
metadata, matching source ZIP, generated checksums and artifact name. The workflow
uses immutable official Node 24 action revisions, a supported standard windows-2022
runner, Python 3.12, the source-pinned vcpkg baseline and hash/signature-verified Inno.
All build inputs are committed or downloaded from public upstream sources; owner
Ultima assets, private ROMs and local machine caches are not workflow dependencies.
Quiz UCC and FLX resources come from the ordinary solution dependency graph.

`audit-candidate.py` validates final checksums, portable payload bytes and identity,
source commit/manifest, dependency archive hashes/DLL mappings, and ordinary
packaged --version startup in a disposable extraction. It does not mark gameplay
PASS. Build transcript and available Defender logs upload on failure; GitHub's
per-step logs retain other tool/download diagnostics. Candidate assets are retained
seven days. No paid cache/service or purchased signing is required.

Private jobs are skipped unless the owner explicitly confirms sufficient free
compute AND artifact storage with spending blocked via
`private_free_usage_confirmed=true`. This input is an acknowledgement, not a billing
API check or guarantee; verify account usage first. Standard public runners are
free; larger runners and private overages are not authorized. Public/tagged runs
can generate free attestations after independent approval; the first private,
untagged run cannot. Do not enable billable resources to make a run succeed.
