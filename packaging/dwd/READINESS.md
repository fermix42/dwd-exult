# DWD 1.0.0-rc.1 candidate readiness -- November 1, 2026

**Hosted candidate verified; awaiting owner downloader acceptance and separate
binary-publication approval.** No GitHub Release/prerelease or tag was created.
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


## Outstanding items and recommendation

- **HUMAN NOT TESTED:** interactive acceptance of the new downloader before binary
  publication. Existing owner gameplay/save/audio/idempotent acceptance stays valid
  for the builds actually tested; automated live downloads are not HUMAN PASS.
- **NOT TESTED:** fresh simplified audio-selector/MT32EMU listening (recommended)
  and clean player Windows OS/runtime prerequisite testing. No unrelated campaign,
  platform or full audio acceptance cycle was repeated.
- **Known pre-existing limitation:** explicit forward-slash `/BGDIR=C:/...` and
  `/SIDIR=C:/...` aliases cause a config rewrite and loss of formatting/comments;
  no mod reinstall or save loss. This FAIL reproduces in accepted efa7b679, outside
  the downloader change. Use canonical Windows backslashes or omit unchanged path
  options. It is reported under the owner's unrelated-failure boundary, not hidden
  or silently fixed. Full path-alias preservation is not certified.
- **Authorization still required:** exact tag, assets and GitHub Release/prerelease
  publication. No tag/release/announcement or paid service is authorized here.

Recommend owner review of this verified hosted candidate for the first prerelease,
with downloader interactive acceptance completed and the existing CLI path-alias
limitation reviewed before any unqualified config-preservation claim. Do not publish
binaries yet. The final report update is documentation-only and does not regenerate
or relabel these immutable 41a83ac4f2f4f73c080655f29d7946f7944deacb artifacts.
