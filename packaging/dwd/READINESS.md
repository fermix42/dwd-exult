# DWD 1.0.0-rc.1 hosted-build readiness — November 1, 2026

**BLOCKED before push: original game content remains in the proposed source and
its reachable history.** The owner explicitly requested stopping on a material
publication risk. No push or repository setting change was performed in this pass.
This report is evidence; AGENTS.md remains the authoritative governance.

## Repository and exact source identities

- Public repository: https://github.com/fermix42/dwd-exult. The owner already renamed
  it and made it public; neither operation was repeated.
- Local prepared branch: `codex/dwd-release-preparation`. Prior accepted functional
  source/installer: `efa7b679fa73f34bd129e4df1a6ebbf71ea97e24`.
- Current public default, verified read-only through GitHub: `main`, commit
  `6fbda9a8d887c5ddfced99412502a36ae48665e2`. It was not changed. The proposed
  nondestructive default remains the prepared branch, keeping placeholder main intact.
- Exact pushed DWD source SHA: **NONE in this task**. The local final documentation
  commit is reported by the handoff and generated local audit summary; resolve it
  with `git rev-parse HEAD`. It is not a hosted build source or artifact revision.
- Hosted run URL/result: **NOT RUN — blocked by pre-push audit**, not a runner failure.
- Hosted artifact names/checksums/provenance/attestations: **NOT GENERATED**.
  Prior local candidate artifacts and checksums retain their accepted source SHA;
  they were not rebuilt or relabeled in this documentation/audit pass.

## Completed acceptance retained

The owner's latest explicit sign-off is **HUMAN PASS** for installed optional-content
recognition, existing audio/mod detection, no repeated downloads of unchanged
packages, explicit reinstall override, and the in-place installation experience.
This supplements accepted Windows installation, BG play through Orb acquisition,
BG/Keyring and SI/SI Fixes save/load, independent saves, and Enhanced OPL3 music
with synthesized OPL2 effects. Do not request those completed tests again absent a
relevant implementation change.

The owner's observed absence of downloads is not a claim of instrumented zero
external network requests. Existing **46 targeted automated PASS** (36 upgrade/
repair/offline plus 10 live-download assertions), older 287 PASS, audio regressions,
73 audio-startup/configuration assertions, and read-only-copy evidence remain scoped
historical evidence. No gameplay/audio/installer suite was rerun merely for hosting.

## Whole-history publication audit findings

All 10,372 reachable commits / 84,329 objects / 47,959 blobs were inventoried.
Gitleaks 8.30.1 produced three findings, all checked and identified as manifest
SHA-256 values, not credentials. Whole-file Git hashes were compared with 434
original static asset/ROM files. The following inherited content blocks clearance:

| Finding | Evidence | Why the requested push is stopped |
| --- | --- | --- |
| `docs/usecode.dis` at HEAD | 6,382,277-byte Git blob; added by `d55f5ac46`; all 17,098 checked long dialogue chunks match original BG Usecode | Original game code/dialogue is present; its redistribution permission was not established in reviewed notices. |
| `content/islefaq/patch/equip.dat` at HEAD | 961 bytes, identical to original BG EQUIP.DAT; added by `a30022f59` | Original game asset match conflicts with the requested no-proprietary-assets clearance; the original mod was not altered. |
| Historical `docs/usedis.zip` | Reachable deleted ZIP contains older 6,374,693-byte disassembly and the same matching dialogue; added `0cefe13a8`, tree removal `dd815f983` | Removing only current files would not clean the history being pushed. |

The findings predate DWD release preparation. Upstream public availability and the
preserved engine GPL notices do not establish the missing permission needed to
clear these particular game-content findings against the owner's requirement.
No ROM matches or confirmed secrets were found by the performed checks. Because
material findings already block publication, this report does not claim exhaustive
license/source clearance or a completed review of every remaining historical asset.

Do not delete a licensed third-party package component, rewrite history, push a
sanitized tip while retaining offending ancestors, or discard placeholder history
silently. Resolve the identified rights/provenance, or obtain approval for a concrete
reviewed publication-history strategy that satisfies the source/history boundaries.
No source/mod cleanup or remote remediation was attempted here.

## Remaining items classified

| Class | Item | Status |
| --- | --- | --- |
| BLOCKER | Source/history publication clearance | Resolve the three linked original-game-content findings before any prepared-branch push/default change/build dispatch. |
| BLOCKER for accepting hosted assets | Hosted build and artifact verification | NOT RUN because of the audit stop. No compile, package, hosted checksum or attestation success is claimed. |
| BLOCKER for binary publication | Separate exact-tag/asset/release approval | Still not granted. No final tag, GitHub Release/prerelease or binary announcement is allowed. |
| RECOMMENDED | Fresh simplified audio-selector/MT32EMU listening and visual review | NOT TESTED beyond existing scoped acceptance. No broad repeat of completed listening/gameplay tests. |
| RECOMMENDED | Clean player Windows OS/runtime prerequisite | NOT TESTED; no usable disposable environment was established previously. A VS runner would not be a clean player OS test. |
| OPTIONAL | Other platforms/backends/languages, full campaign and cross-version migration | No expanded certification or new migration claims; retain previously documented limits. |

## Authorized continuation after resolving the audit hold

The current owner authorization covers pushing the complete audited release branch
without force, preserving placeholder main, establishing the appropriate default,
registering/running one immutable-SHA candidate workflow, downloading/inspecting
Actions artifacts and updating/pushing validation documentation. Do not ask again
merely for those already-authorized operations. The material audit hold must first
be resolved; no current action is implied by this plan.

Use the prepared manual workflow and standard public GitHub-hosted runners only.
[GitHub's billing policy](https://docs.github.com/en/billing/concepts/product-billing/github-actions)
confirms standard public runner compute is free; no paid runner, cache service,
commercial signing or billable-resource change is authorized. Public attestations
are [available on free plans](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations).
The currently prepared attestation job is tag-gated: before an approved untagged
candidate run, adjust that workflow condition to allow public immutable-commit
attestations rather than create an unauthorized release tag. No workflow edit or
run was necessary while the publication audit is blocked.

After a cleared push, verify the GitHub branch SHA and default, dispatch against
that exact expected_revision with tag blank, retain run/image/tool logs and match
all hosted checksum entries, version/upstream/source metadata, payload/docs/licenses,
audio identifiers/aliases, portable roots, upgrade manifests, optional mod packaging
and corresponding dependency sources. Compare local/hosted inventories and record
compiler/timestamp differences rather than promise byte-identical builds. Reuse
accepted human evidence. Record the actual run URL, artifact names/digests and
verified attestations here before recommending a prerelease. No hosted result
can be inferred from the prior local package or workflow lint.

Full scoped evidence and provenance are in VALIDATION.md. Local audit evidence is
under `../test-output/dwd-hosted/`; no additional governance system was created.
