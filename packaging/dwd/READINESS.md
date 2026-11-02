# DWD 1.0.0-rc.1 candidate readiness -- November 1, 2026

**Hosted candidate verified; owner downloader installation and menu discovery
accepted. Awaiting separate binary-publication approval.** No GitHub Release/prerelease or tag was created.
A pre-existing command-line path-alias limitation remains explicitly recorded.

| Identity/result | Verified value |
| --- | --- |
| Public repository | https://github.com/fermix42/dwd-exult |
| Default branch | `codex/dwd-release-preparation` |
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
diagnostic Actions artifacts expire after seven days (November 8, 2026); no
permanent release assets have been published. Full inventories, all hashes and
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
- **Authorization still required:** exact tag, assets and GitHub Release/prerelease
  publication. No tag/release/announcement or paid service is authorized here.

The verified hosted candidate and reported human downloader installation/menu
acceptance are ready for the owner's first public prerelease publication decision.
The remaining recommended tests are optional owner decisions, not newly imposed
mandatory release blockers. The existing CLI path-alias limitation remains
documented and unchanged; full path-alias preservation is not certified. Public
binary publication still requires separate explicit approval of the tag/assets and
Release/prerelease. This reporting update is documentation-only and does not
regenerate or relabel the immutable
`41a83ac4f2f4f73c080655f29d7946f7944deacb` artifacts; its documentation commit has a
different SHA.


## Final unsigned policy and prepared publication -- November 1, 2026

The owner explicitly accepts unsigned installer/executable distribution and Unknown
publisher as its consequence. Signing is not a requirement, defect, failed test or
release blocker. No paid certificate/service is authorized. SignPath Foundation
and other signing arrangements are not being pursued; any future reconsideration
requires a separate owner decision. Checksums, public source, GitHub-hosted builds,
verified provenance and existing Defender evidence remain the verification approach,
not Authenticode or a safety guarantee. No signing workflow step is introduced.
README/release notes explain the separate SmartScreen reputation warning and
built-in PowerShell checksum verification without disabling Windows protections.

Prepared title: **Dark Wraith Dragon's Exult 1.0.0-rc.1**; proposed tag
`dwd-v1.0.0-rc.1`; type **prerelease**, not stable/latest. Anchor the tag to exact binary
build `41a83ac4f2f4f73c080655f29d7946f7944deacb`, never the newer documentation tip.
The final owner tag decision follows the established dwd-v convention. The earlier
unprefixed proposal is superseded; no packaging code or metadata is changed. No tag or release identity conflict was
found in local/remote checks; GitHub has no Releases. Recheck at publication time.

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

[RELEASE.md](RELEASE.md#first-prerelease-publication-plan----prepared-not-executed)
contains the exact four-file manifest, reviewed GitHub asset requirements,
post-approval publication sequence and complete proposed player-facing notes.
All four files fit GitHub's per-asset limit; attaching existing files needs no build.

All prior automated results (53/39/7 local and 23/39 hosted), build/provenance PASS,
Defender results and scoped HUMAN PASS evidence are preserved. VALIDATION.md is
unchanged; no gameplay/audio/installer tests or Actions runs were repeated.
Fresh simplified-audio-selector/MT32Emu listening and clean Windows/runtime
validation remain optional NOT TESTED recommendations. The documented command-line
path-alias limitation is unchanged, not reopened.

No technical preparation blocker remains on the recorded scope. The first unsigned
prerelease is prepared for the owner's final publication decision. Approval is
still required to create/push the exact tag, create the prerelease/draft, upload
these four files and publish the reviewed notes. Nothing has been tagged, uploaded,
published or announced by this task; repository/workflow settings are unchanged.
Only documentation is committed/pushed. New documentation SHAs do not replace the
immutable binary build SHA or existing provenance.


## Final publication authorization -- November 1, 2026

The owner explicitly authorized the first public prerelease, titled
Dark Wraith Dragon's Exult 1.0.0-rc.1, tagged `dwd-v1.0.0-rc.1` at
`41a83ac4f2f4f73c080655f29d7946f7944deacb`, prerelease=true and latest=false.
This supersedes the earlier pending-approval statements and unprefixed tag proposal.
Local/remote conflict checks found no tag/Release; the public repository and default
branch, remote source commit and sufficient permissions were verified. Only manual
workflow_dispatch is enabled in the tag's source; inherited automation is disabled.
All four approved local files and manifest package entries match the owner's hashes.
Publication will use those files unchanged, verify a draft by downloading its exact
four assets, then publish without another confirmation if all checks pass.
No rebuild, signing, gameplay/audio retest, settings change or external announcement.
The immutable binary source remains distinct from these documentation-only commits.
