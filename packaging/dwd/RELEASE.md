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
release. Its optional attachment job only accepts an existing draft, requires a
protected environment and refuses open validation gates. Configure required
reviewers on `dwd-release-approval` before enabling attachments.

Example (use fresh output directory):

```powershell
python packaging/dwd/check.py
python packaging/dwd/release.py package --binary-dir ../build-exult-fork/bin --runtime-dir ../build-exult-fork/bin --dependency-share ../build-exult/deps/x64-windows/share --output ../test-output/dwd-release/candidate-1
```

For installer compilation additionally supply `--installer-tools <directory>` with
source-built `exconfig.exe`, `bgkeyring.zip`, `islefaq.zip`, `sifixes.zip`, and
`--iscc <Inno Setup 6 ISCC.exe>` and `--dependency-sources <verified source cache>`. These mod snapshots must match recorded source;
do not take arbitrary player-installed packages. The generated installer input
retains upstream mod/download/path code. Do not claim a ZIP is an installer.

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
| Installer | Inno, architecture macros, paths, optional mods/downloads, uninstall | Generated from full upstream script; separate AppId, HKCU registry, group, writable per-user directory |
| Profile | Official profile | Accepted portable beside-executable behavior retained |
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

The attachment job deliberately refuses unresolved gates. No workflow automatically
sets validation fields to PASS. Add verified evidence and an explicit reviewed
publication procedure before enabling distribution.

## Zero-budget follow-up

Unsigned Windows builds are approved by permanent project policy. Authenticode is
not a gate, and no paid service is required. SHA-256, exact source provenance and
Defender scanning remain required. The optional public/tagged GitHub attestation
job uses actions/attest@v4 with a dedicated OIDC/attestation permission scope. It is
free for public repositories and is not a Windows signature or a SmartScreen bypass.
No attestation has been generated locally; hosted execution requires approval to
push and run the branch. Configure environment reviewers before release attachment.

`build-mods.py` follows the checked-in upstream Makefile.mingw install file lists,
using ordinary UCC/expack in disposable staging. It emits required directory ZIP
entries for native --installmod. `collect-dependencies.py` checks installed DLLs and
recipes against their installed SPDX hashes, then fetches exact SHA-512-verified
archives. The source ZIP includes these inputs and the full pinned vcpkg script tree.
For ordinary versioned-registry rebuilding, clone vcpkg at the recorded revision;
the included source archive is not a Git repository. No offline build is claimed.

The local install regression uses `installer-check.py`, same installed executable,
disposable mods/configs/saves and protected-file hashes. It is a host-isolated test,
not a fresh Windows VM. Interactive controls/downloads, clean-OS prerequisites and
hosted Actions remain separately recorded as NOT TESTED. See VALIDATION.md.
