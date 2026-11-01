# DWD Windows candidate validation — November 1, 2026

Branch: `codex/dwd-release-preparation`. DWD version: **1.0.0-rc.1**, unpublished.
Upstream: Exult **1.13.1git**, incorporated revision
`97984e9e0cbb4ccacaab0249da9c51b533818f74`.
The exact final DWD revision is generated into every candidate's metadata, binary
version output and source provenance; see the final artifact evidence. No push,
tag, repository rename/visibility/default-branch change or release publication
was performed. Accepted gameplay/audio/art code remains frozen and unchanged.

## Results and evidence

| Validation gate | Result | Evidence and limits |
| --- | --- | --- |
| Ordinary Windows Release/x64 compilation | PASS | Existing MSVC solution/build facilities; Exult, expack and exconfig built. Known manifest namespace/debug-option warnings and optional DLL post-copy missing-path messages remain; explicit payload copies are hash checked. |
| Inno installer compilation | PASS | Official Inno Setup 6.7.4, SHA-256 a549929319f9120716677c685535ca4671e4b937b5fdc87e2585f32d8eb30e8c; download Authenticode signature valid, Pyrsys B.V. |
| DWD installer identity | PASS | DWD filename, version, publisher, AppId DWDExult, HKCU registry, independent uninstall key/group and {sd}\Games\DWDExult default. Exult.exe retains its name. |
| Independent installer manifest inspection | PASS | innounp 2.71.3 listed the actual compiled DWD installer, including helper, all three mod ZIPs, engine, runtime DLLs, FLX files, original/DWD docs and license notices. |
| Official distribution comparison | PASS | Actual official Exult 1.12.1-1 manifest inspected, SHA-256 90f5495e7810d1ef847191779420f2abb93e56ebb99bd198ad3a374dd74c56db. All 30 upstream nonbinary application paths are present; 33 application paths common after architecture-name normalization. Binary differences are intentional (below). |
| Fresh silent installation/file integrity | PASS | Every installed payload file checked against staging hashes; correct installed version output and independent uninstall entry. |
| Game-path configuration | PASS | Actual upstream exconfig Set/Get round-trip with supplied BG/SI paths. Existing valid config preserved. Owner reports the interactive installer worked as expected (HUMAN PASS); specific browse-click sequences were not separately reported. |
| Source-built optional mods | PASS | Keyring, SI Fixes and SourceForge Island built using ordinary UCC/expack from committed source and upstream install lists. Native --installmod installed every file, verified byte-for-byte with package contents after filename-case normalization. |
| Focused installed-engine regression | PASS | BG 8/8, SI 8/8, Keyring 9/9 and SI Fixes 9/9; save/load, embedded helpers, mod override authority, natural SI opening/conversation, zero failures and normal cleanup. No full campaign/audio/visual claim. |
| Upgrade and paths-only installation | PASS | Silent same-version reinstall and paths-only mode preserve config values, binaries and disposable save sentinels. Cross-version migration awaits a future version. |
| Uninstall and preservation | PASS | Engine and uninstall key removed; DWD install-path registry values cleaned; config, in-folder and external save sentinels preserved. All files under protected official Exult and both supplied game trees, and official registry entries, unchanged. |
| Corresponding source audit | PASS | Engine source ZIP matches build commit, includes modified code/scripts, original notices and embedded Nuked OPL3 sources; extracted metadata works without Git. External source archives SHA-512 verified, installed DLL SHA-256 and recipes match installed SPDX records, exact patches/options and full pinned vcpkg script tree included. This is provenance/source completeness verification, not a bit-identical dependency rebuild. |
| Artifact integrity | PASS | SHA-256 for installer, portable/source ZIPs and supporting assets; payload manifest; source archive commit and no proprietary ROM/game/save files staged. |
| Microsoft Defender | PASS | Custom scan of candidate installer, executable/DLLs and archives found no threats; signatures updated successfully. A malware scan is evidence for that scan, not a guarantee against all malware. |
| Authenticode signing | PASS (unsigned policy) | Publicly trusted signing is not required. Engine/installer intentionally unsigned; SHA-256 and provenance retained. No purchase/subscription initiated. |
| Workflow source assessment | PASS | Prepared standard windows-2022 MSVC/vcpkg build, parser tools, pinned verified Inno compiler, source mod builds, dependency collection, packaging, available Defender scan and restricted manual release gates. YAML/PowerShell/Python syntax checked. Hosted behavior still unverified. |
| Fresh disposable Windows VM/clean OS | NOT TESTED | No usable disposable VM was established. vmcompute was stopped; feature query required an elevated Windows token. No Windows feature/security settings were changed. Local tests were host-isolated with an already installed MSVC runtime. |
| Interactive Windows installer | HUMAN PASS (owner report) | Owner installed and operated the candidate successfully and reported the experience worked as expected. Specific browse-click sequences/appearance details were not separately reported and are not a new mandatory gate. |
| Scoped human gameplay/audio acceptance | HUMAN PASS (owner report) | BG through Orb acquisition; BG/Keyring and SI/SI Fixes save/load; BG versus Keyring save separation; Enhanced OPL3 music with synthesized OPL2 effects sounded as expected, no audible problems reported. Not a full campaign/all-backend claim. |
| Hosted GitHub Actions execution | NOT TESTED | Requires an approved push/run. No branch or tag pushed, no hosted run claimed. |
| GitHub public artifact attestations | NOT TESTED (prepared) | Dedicated public/tagged actions/attest@v4 job prepared, free for public repositories. No attestation generated; it is provenance, not Authenticode or a SmartScreen bypass. |
| Public distribution | NOT TESTED / approval gate | Owner must approve exact commit/assets and deployment operations. Scoped human acceptance is complete; remaining risks are classified in READINESS.md. |

No outstanding product failure is known from the passing rerun. Two earlier failures
are retained transparently: the first upstream-derived uninstaller left a stale
DWD path registry value (fixed with tracked Inno registry entries); the original
quiz test assumed lowercase `usecode` while native Windows installation uses
`USECODE` (test-only lookup corrected). An uninstaller test log also collided with
the active uninstall log; command output now has a separate filename. None changed
gameplay/audio. Failed test logs remain as historical evidence.

## Artifact/source differences from upstream

- Current incorporated 1.13.1git uses SDL3 and the accepted MSVC Release/x64 toolchain;
  official stable 1.12.1-1 uses SDL2/MinGW and supplies both x86/x64. DLL names and
  dependency sets therefore differ. DWD only claims local x64 evidence.
- All original documentation/resource categories are retained. DWD adds its README,
  changelog, independent metadata, INSTALL, dependency inventory and component notices.
- Installer uses per-user DWD identity and portable beside-executable configuration.
  Registry changes are tracked for uninstall. Official Exult is not overwritten.
- Mod ZIPs come from checked-in accepted sources rather than copying player mods.
  They are embedded as upstream expects; portable users also receive optional ZIPs.
  Actual audio/third-party downloads use pinned hashes and owned destinations.
- Source ZIP additionally includes external dependency source archives, installed
  SPDX/ABI inventory, exact vcpkg port patches/build options and the complete pinned
  vcpkg source/scripts. Ordinary versioned registry rebuilding uses Git, Visual
  Studio v143, Python and Win Flex/Bison; no offline or bit-identical rebuild claim.
- Non-system redistributed components: SDL3 3.4.12, FluidSynth 2.5.5,
  mt32emu 2.8.3, libpng 1.6.58, Ogg 1.3.6#1, Vorbis 1.3.7#4, zlib 1.3.2#1;
  compiled headers gcem 1.18.0 and dirent 1.26, plus in-tree Nuked OPL3. Their
  respective licenses and sources are retained; dynamic DLLs remain replaceable.
- Windows API/UCRT and MSVC v14 runtime are system/toolchain prerequisites. No
  Microsoft runtime DLL is bundled. A clean-machine test of the documented MSVC
  runtime prerequisite remains NOT TESTED. Inno's applicable license is included;
  its BSD-style compiler runtime is distinct from GPL engine/dependency sources.

## Reproduction and responsibility

Fully automated locally: build/stage, source/dependency/hash audit, silent installer
checks, path helper, native mod installs, focused installed runtime, upgrade,
paths-only, uninstall, preservation comparisons and Defender scan. The first full
passing installer run recorded **167 installer assertions plus 34 runtime
assertions** at source snapshot b7f12dba2 (with the test-only filename/log fixes).
The final committed snapshot is rebuilt and its exact results are retained in the
final evidence files rather than embedding a future self-referential SHA here.

Additional environment confidence: clean Windows VM with documented runtime
prerequisite remains NOT TESTED. Scoped owner acceptance below supersedes earlier
blanket human-testing gaps; do not request the completed observations again. Signing is intentionally not a gate. Approval-dependent GitHub work:
push/run prepared workflow, inspect hosted outputs, generate free attestations
once public/tagged and approved, then separately
approve final publication. No paid service is required or authorized.

Evidence root: `../test-output/dwd-release-validation/` (not committed), including
build/package logs, actual installer manifests, upstream comparison, source audits,
Defender logs and each isolated install/runtime run. Final artifacts carry their
own `release-metadata.json`, `source-provenance.json`, `payload-manifest.json`,
`dependency-sources.json`, curated notes and SHA256SUMS.txt.

Primary tooling references:
[Inno downloads](https://jrsoftware.org/isdl.php),
[official Exult downloads](https://exult.info/download.html),
[GitHub attestations](https://github.com/actions/attest),
[Microsoft runtime deployment](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files).

## Executable-root follow-up evidence

Evidence root: `../test-output/dwd-paths/`. Exact final revision, rebuilt assets and
final reruns are recorded in final-review.json beside the final assets. Earlier
runs: installer-1 passed 186 assertions plus all 34 runtime assertions; portable-3
passed 39; default-3 passed 16; download-install-2 passed 10 actual network-download
assertions. The subsequent committed-source run passed **188 installer assertions, 34 runtime
assertions and 39 portable assertions**. Final reruns supersede earlier snapshots.

The installer regression retains the previous 167 checks and adds owned paths,
remembered upgrades, package bytes, mod update/removal save preservation and obsolete
file removal. Portable tests use no -p and cover unrelated CWD, relocation, external
assets, mod override rebasing, all optional package bytes, traversal rejection,
overlap rejection before writes and a disposable deny-write ACL fixture.
Default tests install at C:\Games\DWDExult without /DIR, launch the actual Start
Menu shortcut with temporary verifier arguments and unrelated working directory,
restore it, then test desktop-style and Explorer-style shell launches. These run
the ordinary executable's --verify-files; visible gameplay from these surfaces
remains NOT TESTED. Actual Inno downloads include all-in-one audio, original
Ultima VI Remake and Glimmerscape, compared against every original package file.

Protected original games, official engine and official profiles are compared by
files AND directories. Official registry/uninstall state is preserved. Original
game-tree ACLs were not changed. The later read-only disposable-copy verification
below closes the ACL-enforced source-read-only gap without changing owner permissions. Inno's own transient Windows bootstrap and
download staging is outside the selected root; persistent data/runtime temp is owned.

Clean-machine runtime prerequisites, hosted Actions and public attestations remain
NOT TESTED. Scoped owner acceptance is recorded below. Unsigned status is accepted.
No push/publication occurred. Earlier failures retained in logs: inherited native
installation skipped Ultima VI Remake's root changelog (now preserved unchanged);
an overly broad disposable ACL prevented Windows loading the executable (fixture
corrected to deny creation only). Neither is reported as a passing run.

## Owner human acceptance recorded November 1, 2026

Provenance: the project owner's explicit sign-off supplied in the request titled
"DWD Exult — Human Acceptance Sign-Off and GitHub Release Readiness" in this chat.
The owner identifies the locally built candidate accepted at
`7c07eab7906da69477678d656a3bb049b673d9ab`. The precise test time and instrumented
session logs were not supplied; these are owner observations, not agent-executed
assertions. Record date is not an invented execution timestamp.

| Scope | Result | Owner observation |
| --- | --- | --- |
| Windows installer | HUMAN PASS | Installed and operated successfully; installation experience worked as expected. |
| Unmodded Black Gate | HUMAN PASS | Launched, played through acquisition of the Orb of the Moons, saved and reloaded. Acquisition alone is not a claim that every Orb chart interaction was tested. |
| Marzo's Keyring | HUMAN PASS | Saved and reloaded; unmodded BG and Keyring saves stored separately. |
| Unmodded Serpent Isle | HUMAN PASS | Saved and loaded successfully. |
| SI Fixes | HUMAN PASS | Saved and loaded successfully; tested configurations worked correctly. Owner did not separately report SI versus SI Fixes directory inspection. |
| Enhanced OPL3 music + synthesized OPL2 effects | HUMAN PASS | Sounded as expected during gameplay; no audible problems reported in that test. |

These completed tests must not be requested again for a checklist. They do not
certify every audio backend, gameplay feature, language, platform, full campaign or
installation scenario. No human acceptance entry was added to the public changelog.

## Evidence preservation and newly performed verification

The accepted `../test-output/dwd-paths/final-accepted/final-review.json` records
**188 installer + 34 runtime + 39 portable + 16 default/shortcut + 10 real-download
= 287 PASS**, zero failures, Defender no threats, and passing source/dependency/
payload/checksum audits at the accepted candidate revision. These are preserved
historical results, not 287 newly executed assertions. The 19 entries in its
SHA256SUMS were rechecked now and match. Accepted artifacts were not rewritten.

New evidence lives under `../test-output/dwd-readiness/`:

- `readonly-assets/summary.json`: PASS, **19 fixture checks and 34 newly executed
  focused runtime assertions**. `readonly-check.py` copied both original static
  asset trees into fresh disposable roots, used inherited granular NTFS denies for
  create/write/delete (not merely ReadOnly attributes), confirmed write opens were
  denied, configured the accepted ordinary executable, installed original Keyring
  and SI Fixes, and ran base/mod save/load scenarios. Before/after file hashes AND
  directory inventories matched in the copies and owner's games. Only copied-tree
  permissions were changed and those deny entries were removed afterward.
- `candidate-audit.log`: PASS on the accepted packages, 19 artifact checksums and
  61 payload files, complete source manifest/dependency hashes/portable contents,
  and `--version` startup from a fresh extraction. This is the new hosted audit
  exercised locally, not a clean-OS or gameplay claim.
- `actionlint.log`: actionlint **1.7.12 PASS**, official release ZIP verified against
  its published SHA-256 `6e7241b51e6817ea6a047693d8e6fed13b31819c9a0dd6c5a726e1592d22f6e9`.
  Workflow policy, YAML, Python and PowerShell parsing also checked. A deliberate
  missing-vcpkg prerequisite test exited nonzero and retained the build transcript
  before any integration/compilation (`build-failure-fixture.log`).
- Clean-environment availability rechecked: vmcompute stopped, no Get-VM command,
  no WindowsSandbox.exe or detected VMware/VirtualBox installation; optional-feature
  query requires an elevated OS token. No VM established and no host settings changed.
  Hosted developer images with Visual Studio are not equivalent to a clean player OS.

Only documentation, workflow and release-validation tooling changed in this pass.
No engine/audio/gameplay/storage/mod source or package recipe changed. No full
installer/download/portable suite or Defender scan was repeated; their accepted
results remain applicable to the unchanged local candidate. Hosted script execution,
remote downloads/tool availability and the first newly compiled hosted binaries
remain to be verified after approval. See [READINESS.md](READINESS.md) for classified
remaining risks and the exact staged approval sequence.

## Audio selection simplification before first publication

This follow-up supersedes the unpublished experimental Roland selections only.
UI music labels are **Enhanced OPL3 Stereo**, **OPL2 (SB/ADLIB)** and **MT32EMU**;
other upstream backends/external MIDI, Digital Music and separate sound effects
remain available. Retained persistent IDs remain `OPL3Stereo`, `OriginalOPL2` and
`MT32Emu`. The three retired IDs (`MT32EmuEarly`, `MT32EmuLater`, `MT32EmuCM32L`)
are case-insensitive aliases to MT32Emu in startup, selection and driver creation.
They are not registry entries or selectable presets. Existing unrelated keys/IDs
remain unchanged. No migration framework, new gameplay or save format is involved.

Inspection established that these were presets on one emulator with separate
ROM-model filtering and original-score handling, not independent synthesizers.
The preset descriptors/factories/model enum, exclusive model discovery/filtering,
unused original-score policy, MyMidiPlayer profile hooks, and corresponding
experimental-only tests were removed. The retained automatic MT32Emu synthesis,
Munt settings, named CM32L/MT32 preference and interleaved MT32A/B loading remain.
A discovered loader bug was fixed by flushing the merged control file before
reopening it. This does not change synthesis or add another ROM selection policy.

Protected Enhanced OPL3, authentic BG/SI OPL2, independent OriginalSfx and shared
XMidiSequence implementations were compared to the preceding accepted commit and
remain byte-identical. XMidiFile lost only the removed presets' note-pairing branch;
its OPL3 and OriginalOPL2 conditions remain intact. Upstream original text documents
were untouched. Only DWD additions in docs.xml were revised for current choices.

Evidence root: `../test-output/dwd-audio-selection/`. The final `audio-review.json`
records exact committed source, builds, final artifacts, executed results and
hashes; do not apply older candidate checksums to these new binaries. Audio-only
checks use the ordinary build's objects, not a second engine build. Retained Munt
PCM checks use owner-supplied ROMs in ignored disposable copies; no ROM/game/sample
assets enter a source or distribution ZIP. Normal startup checks use SDL dummy
video/audio, observe completed initialization and persisted configuration, then
terminate only for cleanup; they do not claim graceful-exit or human listening PASS.

The preceding 287 installer/runtime/portable/shortcut/download assertions, 19
read-only fixture checks and 34 read-only runtime assertions remain historical
accepted evidence. Unrelated gameplay/save/load tests and the complete optional
network-download suite are not repeated for this audio-only change. Relevant
packaging/source/hash/startup checks are repeated on the rebuilt artifacts.

Prior owner HUMAN PASS for Enhanced OPL3 plus independent OPL2 effects is retained;
it is not relabeled as a fresh listening test of the rebuilt binary. New human
inspection/listening of the simplified selector and retained MT32EMU choice is
NOT TESTED and recommended, limited to the changed selection surface. No broad
repeat of completed campaign, installer or other audio acceptance is required.
Clean player Windows/hosted execution/attestations remain unperformed as previously
classified. No source was pushed, tagged or published; version stays 1.0.0-rc.1.

Executed audio regression: **PASS**, `audio-regression-8.log`. This includes exact
registry labels, absence of preset entries, case-insensitive aliases and persisted
configuration, real retained MT32EMU PCM/reinitialization using all three supplied
ROM families, compatible split-ROM initialization, independent BG/SI OPL2 SFX PCM,
existing OPL3 voice/pan/Guardian/score/ending and BG/SI OPL2 reference checks, and a
matching legacy sequencer trace. Protected production FM/SFX files are unchanged.
The split-ROM case exposed the missing flush described above. Earlier failed or
incomplete runs are retained: test notes dispatched before normal queued setup,
a factory used without its mixer pump, cross-game cached assets, and an absent
standalone configuration. These fixture failures and the pre-fix split-ROM failure
are not counted as PASS. No unrelated gameplay acceptance was rerun.
