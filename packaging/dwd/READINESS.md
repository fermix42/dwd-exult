# DWD 1.0.0-rc.2 -- published and independently verified

[Dark Wraith Dragon's Exult 1.0.0-rc.2](https://github.com/fermix42/dwd-exult/releases/tag/dwd-v1.0.0-rc.2)
was published **2026-11-05T15:53:36Z**, November 5, 2026 America/Phoenix.
GitHub confirms a public prerelease, not stable/latest. An unauthenticated request
returned HTTP200. All nine public assets were downloaded and independently hashed.

## Source and preserved history

- Release branch: `codex/dwd-release-1.0.0-rc.2`; started from accepted integration
  `1a66066db6185d3993c87ef6b7919a1e277b3524`. The release baseline had no divergent
  packaging changes: histories reconciled through their existing ancestry, with no
  duplicate cherry-picks or discarded commits. rc.1's binary source remains an ancestor.
- Exact clean binary/source/tag commit: **`9a71267288ec2f3be1c62a79a32515fddc33bd2a`**.
  Annotated remote tag `dwd-v1.0.0-rc.2` was independently peeled to that commit.
- Preparation commits, in order:
  - `b1f15d6ec10e414fe80f83e95174b9053ee58b13`: Prepare DWD Exult 1.0.0-rc.2 prerelease source and version-aware checks
  - `4ec3ec16397c673e2bf8325c3208a45fd0147caf`: Document rc.2 features and make release verification version-aware
  - `f520d6f874751c63778debb99eff44540bd344af`: Refresh official mod archive pins after identical-payload metadata repack audit
  - `9a71267288ec2f3be1c62a79a32515fddc33bd2a`: Bind verified metadata-repacked archives to unchanged installed-content manifests

- No engine/gameplay/art/audio/rendering changes were made during release execution.
  The accepted case, quest, four natural remarks, global duration, Native Wait,
  Orb chart, combat and audio implementations remain unchanged from the integration.
  The fresh-checkout bootstrap correction and required manifests are included.
- Protected release baseline/default remains `7bf0bd8ab2797b3618b41c00a85eb8c8e002a702`;
  placeholder main remains `6fbda9a8d887c5ddfced99412502a36ae48665e2`.
  Accepted feature/integration branches and local milestone were retained.
  No changes to ultima7-modern, original game installations or official Exult.
- This is a separate post-publication reporting commit. Never move the binary tag
  to include it. Archived metadata correctly captures prepublication status and
  packaging-only test scope; this report records the subsequently completed gates.

## Build and verification

| Gate | Observed result |
| --- | --- |
| Local ordinary Release x64 | Clean Rebuild PASS, exact source/dirty=false; one ordinary executable/build tree |
| [Hosted run42054960446](https://github.com/fermix42/dwd-exult/actions/runs/42054960446) | build SUCCESS; attest SUCCESS, exact `9a71267288ec2f3be1c62a79a32515fddc33bd2a` |
| Installer and portable contents | All58 payload files match each other and manifests; no optional installable mod payloads embedded |
| Corresponding source | Archive commit/comment, full source manifest, dependency SHA512 sources and runtime SHA256 mappings PASS |
| Actual rc.1 Setup → hosted rc.2 | 30 assertions PASS: config/paths, BG/Keyring/SI saves, original/customized mods and identity preserved; zero optional HTTP requests scheduled |
| Actual rc.1 portable ZIP → hosted rc.2 overlay | 17 assertions PASS: same-root replacement, config, old saves and custom mod preservation |
| Ordinary portable paths | 39 assertions PASS: unrelated CWD, relocation, isolated base/mod state, malformed archives, protected original/official trees, nonwritable-root refusal/ACL cleanup |
| Hosted installer real official HTTPS downloads | 7 assertions PASS, strict archive hashes and original extracted file manifests |
| Packaged BG labels | 79 assertions PASS: real case/stones, SDL text/key event queue, editing, real MARK/RECALL interoperability and normal save/load note ownership |
| Packaged Keyring spells | 42 assertions PASS: original effective Usecode, spellbook/mana/reagents, valid targeting and normal spell execution |
| Packaged original BG/Keyring commissioning | 26 +26 assertions PASS: original NPC functions, optional eligibility/dialogue and persistent earned commission state |
| Packaged SI | 4 presence/default checks PASS: no granted case/reward and default Normal |
| Global speech-duration packaged runtime | BG default25, SI Triple65 and SI restart persistence65 assertions PASS |
| Existing rc.1-era saves | 9 checks PASS: actual BG/Keyring/SI saves load, Normal without new key, no stale case targeting |
| Available malware scans | Hosted and local Microsoft Defender: scan finished, no threats |
| GitHub provenance | Downloaded installer/portable/source and exact packaged Exult.exe verified against source SHA, expected workflow and GitHub-hosted runner; public primary downloads reverified |
| Public artifact integrity | All9 files byte-identical to candidate; 8 checksum entries plus checksum file agree with GitHub API asset digests and sizes |
| Public startup/source inspection | Public ZIP extracted executable --version PASS; public Source.zip and dependency manifests audited again |
| Prior release/protected branches | rc.1 release record and every asset unchanged (excluding download counters); rc.1 tag not moved; default/main preserved |

Focused packaged gameplay totals **332** assertions, plus9 old-save checks. Do not
add them to historical feature totals as if all previous suites were rerun. Prior
human acceptance and accepted StageF/integration evidence remain valid; no new
open-ended human review, audio changes or experimental combat work occurred.

The first older-save harness attempt asked SI to load while its initial protected
opening owned control; the real handler correctly refused. The corrected verification
acknowledged the original opening dialogue, then loaded the old save successfully.
No gameplay flags or bypasses were introduced.

## Investigated download discrepancy and superseded candidates

All three official snapshots had changed archive bytes. Direct official HTTPS
comparison proved **84 of84 extracted files identical** to the original approved
archives; only ZIP timestamps/extra metadata changed. The explicit audit and old/new
digests are in [download-pin-review-20261105.json](download-pin-review-20261105.json).
Only archive pins and corresponding catalogue headers changed; per-file hashes,
package IDs, mod versions, official URLs, integrity policy and strict checks stayed
intact. No unverified mirror or integrity bypass was used.

The rc.1 upgrade fixture used the immutable original rc.1 installer and its approved
cached archives through rc.1's native `--installmod` handler, simulating already-downloaded user
content. rc.1's old live pins reject today's repacks; rc.1 was not modified. A fresh
rc.2 installation separately used real current official HTTPS downloads successfully.

- Initial hosted42050911090 (`4ec3ec16397c673e2bf8325c3208a45fd0147caf`):
  build/attest passed but superseded after live download checks rejected the repacks.
- Hosted42054052804 (`f520d6f874751c63778debb99eff44540bd344af`): cancelled
  after local strict catalogue validation found archive-header hashes also needed
  updating. Corrected only those headers; all84 per-file hashes stayed identical.
- Final42054960446 (`9a71267288ec2f3be1c62a79a32515fddc33bd2a`): passed all mandatory gates;
  **only these hosted artifacts were published**. No older successful artifacts substituted.

## Published assets

All sizes are bytes; names are unchanged workflow output. Supporting metadata is
the original immutable build snapshot, not a claim that the packaging script ran
the subsequent gameplay/upgrade checks. [Publication JSON](PUBLICATION-1.0.0-rc.2.json)
contains direct asset URLs and machine-readable results.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `dependency-sources.json` | 5329 | `8185679992cab62994310751fb695678afba5e16d080f11c680f73bd3bafeaf6` |
| `DWD-Exult-1.0.0-rc.2-Setup.exe` | 12132723 | `1cde5990acaa1bad2d6f54b1384b98a42f0d2ca3cf2cfe948b31ec61ed3150b4` |
| `DWD-Exult-1.0.0-rc.2-Source.zip` | 60070217 | `16f9062cd582efc42f7b3f0b957f3ac5c214812ad39a191513b68ac4d671e318` |
| `DWD-Exult-1.0.0-rc.2-Windows-x64.zip` | 13229203 | `7bd6afed6c12c14104cf24ac24a8fee483ea2be0a40dded765053cee1dae17d4` |
| `optional-downloads.json` | 2712 | `b585cfddd07559f7b713ada5e0f87231b5252975564ed88885c4f0b5c899ef35` |
| `payload-manifest.json` | 5498 | `589e771a93eee2caade730f811946306fae1e3bf73bdc87f6e5c9fd1e472cc9f` |
| `release-notes.txt` | 1973 | `e49061e9cd1e3e64d5ffc06c57e1e5507a86dfc70e78a68ead9180012e03cdb9` |
| `SHA256SUMS.txt` | 740 | `7ef37b1ce6065ae38bf4ef63b9c044b6c751e3432863d9334c0d966cb1505324` |
| `validation.json` | 661 | `bde3ad3709de34b2284ec92b7ebce810d1983cd0479b96e0d2cd49ca72f16b4d` |

## Security, redistribution and retained limits

Setup and Exult.exe report **NotSigned**. Unsigned/Unknown publisher is explicitly
accepted; no paid signing or signing-service change. Checksums/attestations and
Defender findings do not establish an Authenticode signature or guarantee safety.
The dependency-inclusive source package retains GPL sources/recipes, component
notices, Exult credits, upstream history and the owner's previously documented
inherited-source retention decisions. No newly introduced proprietary game data,
ROM, soundfont, credential, private configuration, save or test fixture is packaged.

Fresh clean-Windows/runtime validation and simplified audio-selector/MT32EMU
listening remain **NOT TESTED**, as previously accepted; no new listening claim.
Other platforms, full campaigns and every mod/language combination are not certified.
For unattended BGDIR/SIDIR use canonical Windows backslashes or omit unchanged
paths: the existing forward-slash alias can rewrite config formatting. Advanced
repair/overwrite/backup/rollback policy/code is unchanged; prior53-assertion
idempotence evidence was reused rather than replayed in full. Actual rc.1 upgrades
and live-download failure/integrity checks were freshly exercised as recorded above.

Local detailed evidence: `C:\Codex\Ultima7 Decomp\test-output\dwd-rc2-20261105`. No publication issue requiring owner action remains.

---

# DWD 1.0.0-rc.1 candidate readiness -- November 1, 2026

**First public prerelease published and independently verified.**
[Dark Wraith Dragon's Exult 1.0.0-rc.1](https://github.com/fermix42/dwd-exult/releases/tag/dwd-v1.0.0-rc.1)
was published at 2026-11-02T01:13:28Z (November 1, 2026 America/Phoenix), tagged
`dwd-v1.0.0-rc.1` at the verified build SHA. It is a prerelease, not stable/latest.
A pre-existing command-line path-alias limitation remains explicitly recorded.

| Identity/result | Verified value |
| --- | --- |
| Public repository | https://github.com/fermix42/dwd-exult |
| Default branch | `codex/dwd-release-preparation` |
| Published tag | `dwd-v1.0.0-rc.1` |
| Public prerelease | https://github.com/fermix42/dwd-exult/releases/tag/dwd-v1.0.0-rc.1 |
| Immutable successful build SHA | `41a83ac4f2f4f73c080655f29d7946f7944deacb` |
| Incorporated upstream | Exult 1.13.1git, `97984e9e0cbb4ccacaab0249da9c51b533818f74` |
| Hosted run | [DWD candidate review](https://github.com/fermix42/dwd-exult/actions/runs/41362896916): build PASS, attest PASS |
| Preserved unrelated placeholder main | `6fbda9a8d887c5ddfced99412502a36ae48665e2` |
| Later reporting changes | Documentation-only; do not substitute the newer branch tip for the artifact build SHA |

The first [attempt](https://github.com/fermix42/dwd-exult/actions/runs/41361780387)
compiled successfully at 78d1537, then stopped before packaging because downstream
steps used the parent of the actual nested vcpkg triplet directory. The successful
retry fixes that CI path and diagnostic capture only. There was one successful
candidate generation and one failed preparatory attempt, not two accepted builds.
The source was pushed normally without tags/force or unrelated history merges.
Inherited scheduled/release/external-upload workflows are .disabled on the prepared
default; the manual candidate workflow uses free standard public runners and
attestation permissions, with no release job or contents-write build permission.

## Distribution and upstream retention

Keyring 0.11.25, SI Fixes 0.15.15 and SourceForge Island are independently selected,
official HTTPS on-demand downloads. Their archives and installable payloads are
absent from installer and portable ZIP. New selections require connectivity;
existing/customized/damaged/unverified content is preserved by default independently
of engine updates. Explicit repair requires overwrite acknowledgement and backups;
isolated saves remain protected. Download/checksum failure skips that package and
allows the engine to complete; extraction/manifest failure removes partial files
and restores prior backups. Native loopback failure tests and live URLs passed.

The official Exult [download page](https://exult.info/download.html) links
[Keyring.zip](https://exult.sourceforge.io/snapshots/Keyring.zip),
[Sifixes.zip](https://exult.sourceforge.io/snapshots/Sifixes.zip) and
[SFisland.zip](https://exult.sourceforge.io/snapshots/SFisland.zip).
Pins and per-file manifests are committed in downloads.json/content-manifests;
SHA-256 provenance is locally computed from verified official downloads, not a
publisher-issued checksum. Compiled mod bytes match accepted original packages;
only text line endings differ. No mod gameplay code or frozen engine/audio changed.

The owner expressly accepted retaining inherited `docs/usecode.dis` (added by
`d55f5ac46`), historical `docs/usedis.zip` (added `0cefe13a8`, tree removal
`dd815f983`) and `content/islefaq/patch/equip.dat` (added `a30022f59`). These predate
DWD and retain upstream notices, attribution and history. This decision supersedes
the prior audit hold without asserting that upstream publication establishes every
redistribution right. Current usecode.dis/equip.dat remain in corresponding-source;
usedis.zip remains only in reachable Git history. Binary-package absence must not
be misrepresented as absence from every source/history distribution.

The whole-history inventory/434 asset and ROM comparisons were retained; final
Gitleaks findings were four independently verified package-member SHA-256 values,
not credentials. No new independent material source-publication risk was found.
No history was rewritten, inherited source stripped, or ROM introduced by DWD.

## Verification

Clean local delivery implementation: 53 instrumented native installer assertions,
39 portable/path assertions and seven real official HTTPS installer assertions
passed. The retry's matching local package passed source/hash/startup audit, live
seven-assertion check and Defender. The CI-only correction did not change policy,
engine, mod or audio code.

Downloaded successful hosted artifacts: eight checksum entries, 58 payload files,
exact metadata/source manifest, nine runtime DLLs and corresponding sources/notices,
installer catalog, absent mod payloads and packaged startup PASS. Actual hosted
installer passed 23 canonical-path assertions; hosted ZIP passed 39 path assertions.
Original game files AND directories, isolated saves and customized files stayed
intact. All compiled FLX resources match local output; compiler/runtime hashes,
SPDX/ABI metadata and one license's checkout line endings differ as documented.
Byte-identical binary reproducibility is not claimed. Hosted Defender: no threats.

[Provenance verified for all four files](https://github.com/fermix42/dwd-exult/attestations/62439805)
bind the installer, portable/source ZIPs and Exult.exe to 41a83ac4f2f4f73c080655f29d7946f7944deacb and https://github.com/fermix42/dwd-exult/actions/runs/41362896916.
These are free provenance attestations, not Authenticode signing. Candidate and
diagnostic Actions artifacts expire after seven days (November 8, 2026); the four
approved files are now also published as permanent GitHub prerelease assets. Full inventories, all hashes and
scoped evidence are in [VALIDATION.md](VALIDATION.md).

| Hosted package | SHA-256 |
| --- | --- |
| `DWD-Exult-1.0.0-rc.1-Setup.exe` | `561a1bd2ea4bebdb0fd1710dd6533f1e14fa0f13305d68bde813d1b1eb7d1fa9` |
| `DWD-Exult-1.0.0-rc.1-Source.zip` | `860e747db1ed343a4ed1cb282b673aa9641bfb4ad3de16c5866d3c696646f584` |
| `DWD-Exult-1.0.0-rc.1-Windows-x64.zip` | `5c9716f2fab539df70bd85f59b725f682aabbbff96e622b3b9dce2198c45fce1` |


## Human downloader acceptance -- November 1, 2026

The owner reports running the GitHub-hosted installer from immutable build
`41a83ac4f2f4f73c080655f29d7946f7944deacb`, successfully downloading all three
optional mods, launching Exult afterward and seeing all three in its game-selection
menu.

| Owner observation | Result |
| --- | --- |
| Marzo's Black Gate Keyring optional download | HUMAN PASS |
| Marzo's Serpent Isle Fixes optional download | HUMAN PASS |
| SourceForge Island optional download | HUMAN PASS |
| Exult launches after installation | HUMAN PASS |
| All three mods appear in Exult's game-selection menu | HUMAN PASS |

This closes the interactive downloader installation/menu-discovery requirement.
It does not add human gameplay/save/load testing of the freshly downloaded mods,
repeat-installation or download-failure testing, manual checksum verification,
or owner inspection of logs, request counts, internal manifests or rollback.
Existing automated and prior human evidence keeps its original scope; see
[VALIDATION.md](VALIDATION.md). No tests, builds or workflows were repeated and no
replacement artifacts were generated for this documentation-only update.

## Outstanding items and recommendation

- **NOT TESTED recommendations:** fresh simplified audio-selector/MT32Emu listening
  and clean player Windows environment/runtime-prerequisite validation. No unrelated campaign,
  platform or full audio acceptance cycle was repeated.
- **Known pre-existing limitation:** explicit forward-slash `/BGDIR=C:/...` and
  `/SIDIR=C:/...` aliases cause a config rewrite and loss of formatting/comments;
  no mod reinstall or save loss. This FAIL reproduces in accepted efa7b679, outside
  the downloader change. Use canonical Windows backslashes or omit unchanged path
  options. It is reported under the owner's unrelated-failure boundary, not hidden
  or silently fixed. Full path-alias preservation is not certified.
- **Publication completed:** the exact tag, four assets and prerelease were
  explicitly owner-authorized and published. Future versions/settings changes and
  external announcements are outside this authorization; no paid service is allowed.

The verified hosted candidate and human downloader installation/menu acceptance
now support the published prerelease. Recommended listening/clean-Windows checks
remain optional NOT TESTED items, not newly imposed blockers. The existing CLI
path-alias limitation remains documented and unchanged. Current reporting commits
are newer documentation only; immutable source and artifacts remain
`41a83ac4f2f4f73c080655f29d7946f7944deacb`. No next development cycle or stable
release is authorized by this publication.


## Final unsigned policy and publication -- November 1, 2026

The owner explicitly accepts unsigned installer/executable distribution and Unknown
publisher as its consequence. Signing is not a requirement, defect, failed test or
release blocker. No paid certificate/service is authorized. SignPath Foundation
and other signing arrangements are not being pursued; any future reconsideration
requires a separate owner decision. Checksums, public source, GitHub-hosted builds,
verified provenance and existing Defender evidence remain the verification approach,
not Authenticode or a safety guarantee. No signing workflow step is introduced.
README/release notes explain the separate SmartScreen reputation warning and
built-in PowerShell checksum verification without disabling Windows protections.

Released title: **Dark Wraith Dragon's Exult 1.0.0-rc.1**; tag
`dwd-v1.0.0-rc.1`; type **prerelease**, not stable/latest. Anchor the tag to exact binary
build `41a83ac4f2f4f73c080655f29d7946f7944deacb`, never the newer documentation tip.
The final owner tag decision follows the established dwd-v convention. The earlier
unprefixed proposal is superseded; no packaging code or metadata is changed. Prepublication checks found no conflict;
the approved tag/Release now exist and must not be overwritten.

Hosted artifact 13106221647 remains available (expires November 8, 2026 at
23:43:30 UTC). A fresh download, prior local files and separately preserved copies
of all three packages matched every recorded SHA-256. Four publication files are
preserved byte-for-byte at
`C:\Codex\Ultima7 Decomp\release-candidates\DWD-Exult-1.0.0-rc.1-41a83ac4f`:
installer, portable ZIP, dependency-inclusive source ZIP and original unchanged
`SHA256SUMS.txt`. The latter also lists supporting Actions metadata; player
instructions compare the appropriate package entry. No verified artifact was
rebuilt, regenerated, signed, substituted or modified. Original checksum-file
SHA-256: `fc1241bb336e2ff1562052f66d990ffabcfc14399bd4e69281de4c699a0edbc0`.

[RELEASE.md](RELEASE.md#first-prerelease-publication-record----executed-and-verified)
contains the exact four-file manifest, reviewed GitHub asset requirements,
completed publication sequence and complete published player-facing notes.
All four files fit GitHub's per-asset limit; attaching existing files needs no build.

All prior automated results (53/39/7 local and 23/39 hosted), build/provenance PASS,
Defender results and scoped HUMAN PASS evidence are preserved. VALIDATION.md is
unchanged; no gameplay/audio/installer tests or Actions runs were repeated.
Fresh simplified-audio-selector/MT32Emu listening and clean Windows/runtime
validation remain optional NOT TESTED recommendations. The documented command-line
path-alias limitation is unchanged, not reopened.

Publication is complete and verified within the accepted scope. The owner-approved
four files were uploaded unchanged and published after draft verification, without
rebuild, signing, test reruns or external announcements. Repository/workflow settings
are unchanged. New documentation SHAs do not replace immutable source/provenance.


## Final publication authorization -- November 1, 2026

The owner explicitly authorized the first public prerelease, titled
Dark Wraith Dragon's Exult 1.0.0-rc.1, tagged `dwd-v1.0.0-rc.1` at
`41a83ac4f2f4f73c080655f29d7946f7944deacb`, prerelease=true and latest=false.
This supersedes the earlier pending-approval statements and unprefixed tag proposal.
Local/remote conflict checks found no tag/Release; the public repository and default
branch, remote source commit and sufficient permissions were verified. Only manual
workflow_dispatch is enabled in the tag's source; inherited automation is disabled.
All four approved local files and manifest package entries match the owner's hashes.
Publication used those files unchanged, verified the exact four draft downloads,
then published under this authorization without another confirmation.
No rebuild, signing, gameplay/audio retest, settings change or external announcement.
The immutable binary source remains distinct from these documentation-only commits.


## Independent post-publication verification

Published at 2026-11-02T01:13:28Z (November 1, 2026 America/Phoenix): https://github.com/fermix42/dwd-exult/releases/tag/dwd-v1.0.0-rc.1.
Anonymous GitHub API/page access and all four direct public downloads: PASS.
Exact four-file inventory, sizes and SHA-256 values against the approved originals:
PASS, both before publication from the draft and after publication publicly.
Unchanged checksum manifest: 740 bytes, SHA-256
`fc1241bb336e2ff1562052f66d990ffabcfc14399bd4e69281de4c699a0edbc0`.
Source ZIP commit/provenance and remote annotated-tag dereference: PASS at
`41a83ac4f2f4f73c080655f29d7946f7944deacb`.
Publicly downloaded installer/portable/source ZIP GitHub attestations: PASS, bound
to the original exact source SHA and successful workflow run 41362896916.
GitHub reports prerelease=true, draft=false; make_latest=false was explicit and
latest-stable API returns 404. No unexpected Actions run or automated release.
Automatic source archives are additional GitHub links, not replacements for our
uploaded corresponding-source ZIP. All four approved assets are present.

No unexpected publication defect or file mismatch occurred. The unpublished draft
by-tag REST read returned 404; authenticated ID-based access resolved this before
verification/publication, with no duplicate draft, failed content or replacement.
The chronological DWD changelog now records this first publication. VALIDATION.md
historical test results remain unchanged; no accepted tests were repeated.
Current docs may be newer than the release tag; source commit and assets remain
unchanged. The first unsigned DWD prerelease is successfully published and verified.
