# DWD release preparation and validation

The first [public prerelease](https://github.com/fermix42/dwd-exult/releases/tag/dwd-v1.0.0-rc.1)
was published with explicit owner authorization at 2026-11-02T01:13:28Z (November 1, 2026
America/Phoenix). Tag `dwd-v1.0.0-rc.1` targets exact verified binary source
`41a83ac4f2f4f73c080655f29d7946f7944deacb`. It is not stable or latest. Current
reporting commits are newer documentation only; accepted files/metadata and the tag
are immutable. Other publication/settings changes still require owner approval.

## Source selection

Preparation starts at `4639563a84df7c6cd4860f4fb94c019cde864c8b`, the clean
`codex/orb-paper-map` checkout. Its incorporated upstream tip is
`97984e9e0cbb4ccacaab0249da9c51b533818f74` (Exult 1.13.1git). The 21 fork commits
contain window/profile changes, accepted synthesis and music, unified CLI, Native
Wait, quiz defaults and Orb charts. Feature acceptance/evidence is retained in
AGENTS.md, docs/quiz_sliders.md, docs/orb_chart_art.md and docs/game_time_events.md,
and the audio acceptance commit e3ff08e1e. No dormant combat/context branch is
merged. No other checkout, save, configuration or game directory is a source input.

`origin` is https://github.com/fermix42/dwd-exult.git. With explicit owner
authorization the audited `codex/dwd-release-preparation` branch was pushed without
force and made the public default, preserving unrelated placeholder `main` at
`6fbda9a8d887c5ddfced99412502a36ae48665e2` and `codex/window-options` unchanged.
No unrelated roots were merged and no upstream history was rewritten. Candidate
build identity and subsequent documentation-only revisions are recorded separately
in READINESS.md and VALIDATION.md. Tags/Releases require separate approval.

## Build and stage

The recipe below is retained for future builds. For this accepted 1.0.0-rc.1, use
the unchanged hosted files and publication plan at the end; do not rebuild, rerun
validation or dispatch Actions solely for the unsigned-policy documentation.

`version.json` is authoritative. MSVC generates an ignored identity header before
compilation. The upstream engine VERSION remains separate for compatibility.
Other build systems must run `python packaging/dwd/release.py metadata` before
compilation to include DWD console identity; their builds are not validated here.
Dirty-tree candidates are refused. A published revision cannot embed its own future
commit SHA in tracked documentation: exact DWD provenance is generated at build time.

Local ordinary build: use AGENTS.md's existing build command; do not create another
engine build. The hosted script uses the upstream MSVC solution and pinned vcpkg
baseline. Candidate workflow runs only manually, with read-only build permissions.
Its first approved exact-SHA run is recorded in READINESS.md. It uploads
inspection artifacts, never a GitHub Release. The candidate workflow has no release attachment/publication job or contents-write
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
| Mod support | Source-built optional mod ZIPs and exconfig | Original three mods are pinned official HTTPS downloads; no installable mod payload in binary packages; source retained |
| Platforms | Multiple upstream builds | Local and hosted Windows Release/x64 evidence; no ARM/x86/Linux/macOS support claim |
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
  retain the clean-machine MSVC runtime prerequisite check as a NOT TESTED
  recommendation, not an automatic publication blocker for this accepted candidate.
- Distribute the generated matching source ZIP beside binaries, including external
  dependency archives/recipes and component licenses. DLLs remain replaceable.
  Windows API/UCRT and the MSVC v14 runtime are system/toolchain prerequisites,
  not GPL dependency-source archives. No app-local Microsoft CRT binaries are bundled.
  Public source is available at https://github.com/fermix42/dwd-exult; the matching
  dependency-inclusive source ZIP must accompany the approved binaries.
- Run focused semantic CLI runtime checks on the exact packaged binary, plus human
  visual/audio/gameplay review as appropriate. Do not reopen accepted limitations
  or claim full campaign/platform testing from focused historical evidence.
- Build/inspect the unsigned installer, review final curated notes and checksums, record
  tag/commit identity and approve exact assets before public publication.

No candidate workflow job creates tags, releases or attachments. It does not turn
local gameplay results into hosted PASS fields. Review actual hosted outputs before
separately authorizing any distribution.

## Zero-budget follow-up

The owner's final November 1, 2026 decision is to distribute the installer and
executable unsigned. Unknown publisher is an accepted consequence; Authenticode
signing is not a prerequisite, defect, failed test or release blocker. No paid
certificates/services are authorized. Do not pursue SignPath Foundation or other
signing arrangements, or add signing workflow steps. Future reconsideration requires
a separate owner decision. Existing checksum, public-source, hosted-build,
provenance and Defender verification remain intact.

The public untagged candidate already has verified GitHub provenance, recorded in
READINESS.md/VALIDATION.md. Attestations bind artifacts to their source/workflow;
they do not authenticate a Windows publisher through Authenticode, suppress
SmartScreen or guarantee safety. The README and published notes below explain
Unknown publisher separately from SmartScreen reputation and provide PowerShell
checksum instructions. No global Windows security disabling is recommended.
The exact prerelease was owner-authorized and is now published as recorded below.
Other release creation/attachment requires separate approval.

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
clean-OS prerequisites remain a NOT TESTED recommendation; hosted execution and
artifact verification passed as recorded in READINESS.md. Do not repeat completed
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
free; larger runners and private overages are not authorized. The approved public
untagged run generated verified free attestations. Private runs do not use this
public-attestation path. Do not enable billable resources to make a run succeed.


Hosted upstream MSBuild currently places its installed triplet at
`msvcstuff/vs2019/vcpkg_installed/x64-windows/x64-windows`: the first x64-windows
is the integration's install root, the second is vcpkg's triplet. Collection and
runtime/notices packaging must read that exact path; local AGENTS.md overrides use
a different explicitly supplied installation. See VALIDATION.md for the first
failed path lookup and the successful corrected run. Native MSBuild output is
preserved separately from PowerShell transcripts. Hosted candidate identity,
attestations and pending human checks are in READINESS.md; later documentation-only
updates never change the source revision embedded in existing artifacts.


## First prerelease publication record -- executed and verified

Published identity:

- Title: **Dark Wraith Dragon's Exult 1.0.0-rc.1**.
- Version: `1.0.0-rc.1`; type: GitHub **prerelease**, not stable or latest.
- Annotated tag: `dwd-v1.0.0-rc.1`, pointing exactly to
  `41a83ac4f2f4f73c080655f29d7946f7944deacb`.
- The final owner decision follows AGENTS.md's `dwd-vMAJOR.MINOR.PATCH`
  convention. The earlier unprefixed proposal was superseded before any tag was
  created. Do not change version.json, regenerate metadata or rebuild the files.
- Later acceptance/policy documentation commits are not the binary build revision.
  Release notes link current branch documentation separately from exact source.

On November 1, 2026, GitHub reported the accepted candidate artifact (ID 13106221647)
available, unexpired until November 8 at 23:43:30 UTC. It was downloaded again;
all three package hashes matched the owner's baseline AND previous local copies.
Byte-for-byte preserved publication files are outside Git at
`C:\Codex\Ultima7 Decomp\release-candidates\DWD-Exult-1.0.0-rc.1-41a83ac4f`.
No accepted artifact was rebuilt, repacked, signed or modified. The original
740-byte `SHA256SUMS.txt` is preserved unchanged too; its own SHA-256 is
`fc1241bb336e2ff1562052f66d990ffabcfc14399bd4e69281de4c699a0edbc0`.
Its eight entries include five supporting Actions metadata files that are not among
the four published release assets. Player instructions compare the matching package
entry individually rather than bulk-check nonexistent metadata downloads.

| Published release asset | Bytes | SHA-256 |
| --- | ---: | --- |
| `DWD-Exult-1.0.0-rc.1-Setup.exe` | 12090878 | `561a1bd2ea4bebdb0fd1710dd6533f1e14fa0f13305d68bde813d1b1eb7d1fa9` |
| `DWD-Exult-1.0.0-rc.1-Windows-x64.zip` | 13174126 | `5c9716f2fab539df70bd85f59b725f682aabbbff96e622b3b9dce2198c45fce1` |
| `DWD-Exult-1.0.0-rc.1-Source.zip` | 57157239 | `860e747db1ed343a4ed1cb282b673aa9641bfb4ad3de16c5866d3c696646f584` |
| `SHA256SUMS.txt` (original workflow manifest) | 740 | `fc1241bb336e2ff1562052f66d990ffabcfc14399bd4e69281de4c699a0edbc0` |

Before publication, local/remote tag and GitHub Release checks found no conflict.
The annotated tag and public prerelease now exist at the exact approved identity;
do not overwrite or move them.
[GitHub's asset requirements](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)
allow up to 1,000 assets, each under 2 GiB; all four existing files fit. The
[GitHub CLI release documentation](https://cli.github.com/manual/gh_release_create)
supports uploading existing files, an existing verified tag, prerelease status,
curated notes and explicitly not marking it latest. No rebuild is required.
GitHub's automatic source zip/tar links are separate from the uploaded matching
Source.zip, which includes corresponding dependency source/recipes and must remain
an explicit release asset.

The owner explicitly authorized this exact prerelease. Completed publication sequence:

1. Recheck tag/release availability and the preserved file hashes. If anything is
   missing or mismatched, stop and report it; do not rebuild/substitute silently.
2. Create and push only the annotated `dwd-v1.0.0-rc.1` tag at the full verified build
   SHA. Verify the remote tag peels to that SHA; never use the newer default tip.
3. Create a prerelease draft for the existing tag with `--verify-tag`,
   `--prerelease`, `--latest=false`, and a notes file containing exactly the reviewed
   player-facing section below. Attach only the four listed existing files, with
   no clobber/replacement. Verify actual downloaded draft assets before publishing.
4. Verify draft target, prerelease flag, notes, asset names/sizes/hashes and existing
   provenance. Publish that approved prerelease, then download/check the uploaded
   files and verify that their hashes still match. No rebuild or signing step.

The owner explicitly authorized tag creation/push, draft creation, exactly four
asset uploads and public prerelease publication once draft verification passes.
No further confirmation is required. Do not dispatch Actions, change branch
settings, rebuild binaries or make external announcements.

## Published player-facing prerelease notes

<!-- BEGIN PROPOSED PRERELEASE NOTES -->

Dark Wraith Dragon's Exult **1.0.0-rc.1** is the first Windows x64 release candidate
of an independent fork of the open-source [Exult project](https://exult.info/).
It is based on Exult **1.13.1git**, upstream revision
`97984e9e0cbb4ccacaab0249da9c51b533818f74`; it is not an official Exult Team release
or a stable DWD release.

### What is included

- Borderless windows, left/right placement and aspect-correct sizing.
- Enhanced OPL3 Stereo music, original OPL2 (SB/ADLIB) playback, independent
  synthesized AdLib/Roland effects and a main-menu Music Player. Music and effects
  are selected separately; MT32EMU remains the single emulated Roland music choice
  and requires compatible, legally obtained user-supplied ROMs.
- Native Wait in BG/SI, correct initial numeric quiz-slider answers, and an Orb of
  the Moons paper chart for supported Black Gate/Keyring layouts. See the README
  for controls, compatibility and save precautions.

### Install or use the portable ZIP

Use `DWD-Exult-1.0.0-rc.1-Setup.exe`, or extract the complete `DWDExult` folder from
`DWD-Exult-1.0.0-rc.1-Windows-x64.zip` into a writable location. The installer's
Setup Paths Only mode can configure an extracted folder. The MSVC v14 x64 runtime
is required and is not bundled. Supply your original Ultima VII: The Black Gate
and/or Serpent Isle installations; the Windows binary packages do not contain the
commercial games or Roland ROMs.

DWD has an independent installation identity. Configuration, mods and separate
base/mod saves stay beside its executable. Do not install inside an original game
folder or over official Exult. Back up saves/configuration before upgrading.

The installer offers original Marzo's Keyring, Serpent Isle Fixes and SourceForge
Island as official downloads when selected and needed; these payloads are not
bundled in either Windows binary package. New optional downloads need internet.
Engine upgrades preserve installed optional content, including customized or
unverified packages; existing content requires explicit acknowledged repair before
replacement. Saves remain separate. A failed optional download is reported and
skipped so the engine can finish installing.

### Unsigned Windows downloads and verification

The installer and executable are **unsigned**, by the owner's accepted release
policy. Windows may show **Unknown publisher** because it cannot authenticate the
publisher through a trusted Authenticode signature. Microsoft Defender SmartScreen
may separately show a reputation warning. These warnings alone do not establish
that the application contains malware. See Microsoft's
[SmartScreen explanation](https://learn.microsoft.com/windows/apps/package-and-deploy/smartscreen-reputation).
Keep Windows security protections enabled.

Obtain DWD Exult only from [the official GitHub repository](https://github.com/fermix42/dwd-exult).
In PowerShell, open your download folder and run:

```powershell
Get-FileHash -LiteralPath '.\DWD-Exult-1.0.0-rc.1-Setup.exe' -Algorithm SHA256
```

Compare the full hash with the matching filename below or in `SHA256SUMS.txt`.
Use the ZIP filename instead when checking a ZIP; letter case is irrelevant. Do
not use a file whose hash differs. The unchanged workflow checksum file also
lists supporting metadata; compare the entry for your downloaded package.

| File | SHA-256 |
| --- | --- |
| `DWD-Exult-1.0.0-rc.1-Setup.exe` | `561a1bd2ea4bebdb0fd1710dd6533f1e14fa0f13305d68bde813d1b1eb7d1fa9` |
| `DWD-Exult-1.0.0-rc.1-Windows-x64.zip` | `5c9716f2fab539df70bd85f59b725f682aabbbff96e622b3b9dce2198c45fce1` |
| `DWD-Exult-1.0.0-rc.1-Source.zip` | `860e747db1ed343a4ed1cb282b673aa9641bfb4ad3de16c5866d3c696646f584` |

Build source: `41a83ac4f2f4f73c080655f29d7946f7944deacb`.
[Hosted build](https://github.com/fermix42/dwd-exult/actions/runs/41362896916) and
[GitHub provenance](https://github.com/fermix42/dwd-exult/attestations/62439805)
associate the files with that exact source and workflow. With the optional GitHub
CLI installed, verify a downloaded package with:

```powershell
gh attestation verify '.\DWD-Exult-1.0.0-rc.1-Setup.exe' --repo fermix42/dwd-exult
```

Checksums, provenance and prior Defender scans are not an Authenticode signature
or a guarantee of safety.

### Source, acceptance and limits

The engine retains Exult's GPL licensing, original authorship and credits;
component and mod notices remain applicable. The matching `Source.zip` includes
upstream/DWD source and corresponding dependency sources/recipes. GitHub's automatic
source links do not replace that dependency-inclusive archive. Thanks to the Exult
Team, Marzo Sette Torres Junior, mod contributors and all component authors.

Focused automated checks and prior human gameplay/save/audio acceptance are
recorded in the [current validation report](https://github.com/fermix42/dwd-exult/blob/codex/dwd-release-preparation/packaging/dwd/VALIDATION.md).
The owner also accepted all three downloads, launching Exult and finding the mods
in the menu; no new gameplay/save-load acceptance of those fresh downloads is
implied. Fresh simplified-audio-selector listening and clean Windows/runtime
validation remain NOT TESTED recommendations, not automatically release blockers.
Other platforms, full campaigns and every mod/language combination are not certified.
For unattended `/BGDIR` and `/SIDIR`, use canonical Windows backslashes or omit
unchanged path options: forward-slash aliases can rewrite config formatting.

[Current README](https://github.com/fermix42/dwd-exult/blob/codex/dwd-release-preparation/README-DWD.md) ·
[DWD changelog](https://github.com/fermix42/dwd-exult/blob/codex/dwd-release-preparation/ChangeLog-DWD.txt) ·
[Readiness](https://github.com/fermix42/dwd-exult/blob/codex/dwd-release-preparation/packaging/dwd/READINESS.md) ·
[Exact build source](https://github.com/fermix42/dwd-exult/tree/41a83ac4f2f4f73c080655f29d7946f7944deacb)

<!-- END PROPOSED PRERELEASE NOTES -->


## Publication verification record

Public prerelease: [Dark Wraith Dragon's Exult 1.0.0-rc.1](https://github.com/fermix42/dwd-exult/releases/tag/dwd-v1.0.0-rc.1).
GitHub release ID 430974718, published at 2026-11-02T01:13:28Z (November 1, 2026,
America/Phoenix). Annotated tag `dwd-v1.0.0-rc.1` has object
`b280519e0b7cef59592a49ce462d8a4f7cfffe78` and peels to exact binary build
`41a83ac4f2f4f73c080655f29d7946f7944deacb`; documentation tip is separate.

The draft had exactly the four approved assets above, unchanged approved player
notes, correct title, target and prerelease flag. Actual draft downloads matched
all four original sizes/SHA-256 hashes before publication. Existing provenance for
the three packages verified against the original source SHA/workflow/run.
GitHub's by-tag draft read returned 404; authenticated release-list/ID access found
the correct existing unpublished draft and asset-ID downloads worked. No duplicate
was created, no content failed verification and no asset was replaced.

After publication, unauthenticated API/page access and direct public downloads of
all four assets succeeded. Each downloaded size and SHA-256 matched the original
accepted hosted-candidate files and unchanged checksum manifest. Source ZIP commit
comment/source-provenance remained correct. GitHub CLI attestations for all three
publicly downloaded packages verified, with certificate identity binding source
41a83ac4f and workflow run 41362896916. SHA256SUMS.txt is integrity-checked against
the approved original; it is not claimed to have its own build attestation.

GitHub reports draft=false and prerelease=true. Publication explicitly used
make_latest=false; the latest-stable API returns 404 because this repository has
no stable/latest release. GitHub's automatic source zip/tar links remain separate
from the four uploaded assets; our dependency-inclusive Source.zip is included.
The newest Actions run IDs remained 41362896916, 41361780387 and 41095228129: no
unexpected tag/release-triggered build or external automation occurred.

Local evidence: `../test-output/dwd-publication/` holds approved file manifest,
exact notes, draft/public metadata, downloaded files, SHA/size verification results,
verified provenance and workflow inventory. Documentation tag correction was
committed before publication as db9c5f68a. Post-publication housekeeping is another
documentation-only commit; it never changes the binary build SHA or release tag.
No rebuild, gameplay/audio retest, code signing, branch-setting change, paid service
or external announcement was performed. All previously accepted classifications,
optional NOT TESTED recommendations and the known path-alias limitation remain.
