# DWD 1.0.0-rc.1 release readiness — November 1, 2026

Ready for the first **owner-approved hosted candidate build**, subject to dispatch
registration and zero-cost preflight below. No known local product defect blocks
that build. This report is evidence and a deployment plan; AGENTS.md remains the
authoritative governance. Version remains 1.0.0-rc.1; no engine, gameplay, audio,
portable storage or third-party mod implementation changed in this pass.

## Source and accepted candidate

Branch: `codex/dwd-release-preparation`. Resolve the current readiness commit with
`git rev-parse HEAD`; the final handoff and generated local readiness evidence
record its full SHA. The previously built and human-accepted local candidate is
exactly `7c07eab7906da69477678d656a3bb049b673d9ab`. These are different identities:
this documentation/workflow follow-up does not relabel the accepted binaries or
rewrite their corresponding source/checksums. A hosted build will identify the new
dispatched commit and generate its own matching source ZIP and checksums.

Owner HUMAN PASS covers successful interactive Windows installation/operation, BG
play through Orb acquisition, BG and Keyring save/load with separate saves, SI and
SI Fixes save/load, and Enhanced OPL3 music with synthesized OPL2 effects sounding
as expected with no audible problems reported. Provenance is the owner's explicit
sign-off in this chat, recorded in VALIDATION.md; the exact session time was not
supplied. These observations are completed and will not be requested again absent
a relevant change. They do not certify all backends/features/platforms/campaigns.

Prior accepted automated evidence remains **287 PASS**: 188 installer, 34 runtime,
39 portable, 16 default/shortcut, 10 real-download assertions; zero failures,
Defender no threats and passing source/dependency/payload/checksum audits. The
completeness review covers validated game selection, independent HKCU/AppId and
uninstall identity, ZIP relocation, isolated base/mod saves/config, all optional
audio/mod installations, preserved user state on uninstall, and unchanged original
game/official Exult trees. The full suite was not rerun in this pass.

New verification: ACL-enforced disposable copies passed 19 fixture checks and 34
focused runtime assertions across BG/SI and Keyring/SI Fixes. Write access was
actually denied; hashes and directory inventories were unchanged. Owner source
permissions were never modified. The new hosted artifact audit passed locally on
the accepted packages (19 checksums, 61 payload files, source/dependency integrity,
packaged --version startup). actionlint 1.7.12, workflow policy checks and Python/
PowerShell/YAML parsing passed. Evidence: `../test-output/dwd-readiness/` relative to
the repository root; historical packages remain in `../test-output/dwd-paths/final-accepted/`.

## Outstanding items and risk classification

| Class | Item | Decision / remaining evidence |
| --- | --- | --- |
| BLOCKER before hosted dispatch | Owner approval, workflow registration, verified no-spend private usage | No remote operation is authorized yet. Confirm Actions access/policy and default-branch presence of this workflow. Verify account-specific free compute/storage headroom with paid usage blocked; quota is not known locally. |
| BLOCKER before public distribution of hosted assets | First hosted build and inspection | Hosted toolchain, parser installation, vcpkg fetch/build, source downloads, compiler installation, upload/download and actual hosted artifacts are NOT TESTED. A failed build or mismatched source/payload must be resolved; no hosted PASS is claimed from lint. |
| BLOCKER before public distribution | Exact source/assets and separate publication approval | Review hosted source ZIP, licenses, checksums and build identity; ensure the final source URL works after approved rename/visibility changes. Approve the actual tag/commit/assets and curated notes. Never publish a stale local checksum file with hosted binaries. |
| RECOMMENDED | Clean player Windows OS and runtime prerequisite | NOT TESTED. No usable VM/Sandbox available: vmcompute stopped; Get-VM/WindowsSandbox absent; feature query requires elevation. No host feature was enabled. MSVC v14 x64 runtime remains documented prerequisite. Hosted VS image startup is not a clean-player-OS test. |
| RECOMMENDED | Public free attestations | NOT TESTED. Prepared for an explicitly approved existing version tag at the exact public source revision. Verify with gh attestation verify after generation. No paid private-attestation plan is required or authorized. |
| OPTIONAL | Detailed browse-click/shortcut appearance or full visible gameplay from every launch surface | Owner installer acceptance plus prior shell-verifier tests suffice for this candidate. Exact browse sequences and every graphical launch surface were not individually reported. No repeated owner acceptance is required. |
| OPTIONAL | Broader campaign/backend/platform/language coverage and cross-version save migration | NOT TESTED beyond existing feature-specific evidence and scoped acceptance. No expanded support or migration claim; future modified DWD mod editions and Virtue Stone case remain outside this release. |

There is no outstanding observed engine/package failure. The BLOCKER labels above
describe deployment prerequisites, not invented additional human QA requirements.
Unsigned Authenticode status is accepted zero-budget policy, not a blocker.

## Hosted workflow audit

The workflow is manual-only, with no release creation/attachment or contents-write
job. Full approved `expected_revision` must equal both checked-out HEAD and
GITHUB_SHA. `tag` may be blank; nonblank tags are verified against the DWD version
and exact commit by release.py. Checkout credentials are not persisted. Build uses
read-only contents permission; only public/tagged attestation receives scoped
OIDC/attestation/metadata writes. No Exult Team credentials/services are used.

Official checkout v6, setup-python v6, upload-artifact v6, download-artifact v7 and
attest v4 are pinned to immutable upstream tag commit SHAs. Standard windows-2022
(v143), Python 3.12, public Win Flex/Bison, source-pinned vcpkg, and independently
hash/signature-verified Inno 6.7.4 provide the build chain. The ordinary solution
generates UCC/embedded resources/FLX; mods are source-built; dependency sources are
collected and verified. No workstation-only directory, proprietary game data,
ROM, save or local dependency cache is an input.

Final artifact audit checks installer presence, portable payloads/docs/licenses/
resources, source commit/manifest, dependency hashes/DLL mapping, checksums and
startup from a disposable extraction. Available Defender scanning remains enabled.
Step failures have explicit messages; build transcript and Defender logs are
uploaded even on failure, with other diagnostics in GitHub step logs. Assets and
diagnostics retain seven days; download before expiry. Runtime gameplay remains
the separately accepted local evidence, not a fabricated hosted test.

## Zero-cost requirements

[GitHub billing documentation](https://docs.github.com/en/billing/concepts/product-billing/github-actions)
confirms standard public hosted runners are free; private jobs/storage draw on the
owner's included allowance and may otherwise incur overages. This workflow uses
standard runners, no paid services/larger runners, no paid signing and no cache
service. Private allocation is skipped unless the owner explicitly sets
`private_free_usage_confirmed=true`. This acknowledgement cannot inspect billing.

Before setting it, verify remaining included Windows compute and artifact storage,
account-wide concurrent consumption and paid-usage blocking. Review applicable
[budget stop-usage controls](https://docs.github.com/en/billing/how-tos/set-up-budgets);
an alert alone is not a spending block. One build has a 90-minute timeout, at most
one 10-minute public attestation job, and seven-day asset retention. These bound
usage but do not guarantee it fits a private allowance; no spend is authorized.
If free allowance is insufficient, wait for reset or separately approve public
visibility before execution rather than buy capacity. Free public attestations
are supported by [GitHub's documented availability](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations).

## Staged deployment sequence — plan only, no remote actions performed

1. **Local:** review the clean readiness commit, scoped acceptance, exact accepted
   candidate evidence and lint results. No full suite repeat is justified by these
   tooling/documentation changes. Recheck only changes made to fix actual defects.
2. **APPROVAL REQUIRED:** push this exact prepared branch to
   `fermix42/exult-custom` without force-push, tags, merge or publication. Verify
   remote branch HEAD equals the approved full SHA after pushing.
3. **APPROVAL REQUIRED if necessary:** register this workflow on the default
   branch. GitHub requires [default-branch presence for manual dispatch](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow).
   The prepared approach is to set the default to `codex/dwd-release-preparation`
   after its push, keeping old `main` intact. Do not merge unrelated roots or change
   the default implicitly. If the owner prefers another registration method,
   prepare that narrow alternative before seeking approval for its mutation.
4. **APPROVAL REQUIRED:** after zero-cost preflight and Actions policy checks,
   dispatch `dwd-candidate.yml` on that branch with `expected_revision=<approved
   full SHA>`, `tag` blank, and private free-usage acknowledgement only if verified.
   This builds review artifacts, not a GitHub Release. Confirm the run head SHA.
5. **Read-only inspection after approved run:** download `dwd-candidate-<SHA>` and
   diagnostics, verify all SHA256SUMS entries, inspect installer manifest and
   source/dependency records, and retain logs/run URL/image/tool versions. Inspect
   hosted outputs without proprietary fixtures in GitHub. Diagnose actual failures.
6. **Read-only comparison:** compare normalized archive/installer file lists,
   documentation/licenses, source revision, mod source trees and dependency
   inventory against local accepted artifacts. Engine/toolchain binaries, PE/ZIP
   timestamps, generated identity, documentation and compressed mod bytes can
   differ because the source commit/toolchain/environment differs; record each
   expected difference. Do not promise byte-identical builds. Unexpected resources,
   missing licenses/runtime files, source mismatch or private data block publication.
7. **SEPARATE APPROVAL REQUIRED:** rename repository to `fermix42/dwd-exult` and
   verify the new source links, redirects and local origin URL deliberately.
8. **SEPARATE APPROVAL REQUIRED:** make the reviewed repository public after checking
   its full history/settings for private material. This review does not claim a
   full remote-history disclosure audit. Existing inherited automation stays disabled.
9. **SEPARATE APPROVAL REQUIRED:** create the exact approved `dwd-v1.0.0-rc.1` tag
   without moving an existing tag, run/review a public tagged candidate and free
   attestations if chosen, and prepare a draft prerelease with its matching source,
   checksums, notices and curated changelog. Inspect its exact assets/provenance.
   Creation of even a draft needs approval; public prerelease publication is a
   further explicit approval. Do not silently substitute new hosted artifacts for
   previously reviewed ones. Completed owner acceptance is retained unless a
   relevant implementation change invalidates its scope.
