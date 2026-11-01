# Local candidate validation — November 1, 2026

Preparation branch: `codex/dwd-release-preparation`. No remote, repository visibility,
default branch, public release or tag was changed. No player/game data was edited.
The parent workspace's unrelated changes were left untouched.

## Completed checks

- Ordinary existing Windows Release/x64 MSVC build: PASS for Exult, expack and
  exconfig. Existing manifest namespace/compiler warnings and optional vcpkg DLL
  post-copy missing-path messages remain; packaging copies explicit verified DLLs.
- Binary Windows identity: PASS, Dark Wraith Dragon's Exult, `1.0.0-rc.1`, original
  filename `Exult.exe`. Generated version output embeds exact DWD/upstream commits.
- Packaging checks: PASS, clean-commit provenance, original plus supplemental docs,
  source archive, curated release notes, generated installer identity isolation,
  SHA-256 file verification and payload path/extension inspection.
- Source archive inspection: no ROM/save/private-key/executable/DLL filename
  candidates. Only committed files are archived. Upstream-owned README, ChangeLog,
  COPYING, AUTHORS, INSTALL and FAQ are unchanged by preparation.
- Extracted candidate runtime (initial source commit `65e19c81c`): BG 8/8, SI 8/8,
  Keyring 9/9 and SI Fixes 9/9 focused assertions PASS, zero failures, clean native
  shutdown. Checks cover effective embedded quizzes, unrelated helpers, mod override
  authority, save/reload and natural SI opening/companion conversation. Existing
  feature acceptance records remain independent; no full campaign claim is made.
- PE imports inspected: MSVC v14 runtime is required and not bundled. Existing
  runtime DLLs loaded successfully on this host; clean-machine validation is open.
- Frozen gameplay/audio/art implementations: unchanged by release preparation.
- Inherited workflows preserved as `.yml.disabled`; only manual candidate review is
  active. GitHub execution has not been tested.

The final candidate output's `release-metadata.json`, `SHA256SUMS.txt`,
`payload-manifest.json` and runtime evidence identify the exact final build.
Untracked review artifacts/evidence live under `../test-output/dwd-release/`.
Do not redistribute these candidates as a validated stable release.

## Open gates

No installer EXE was produced: Inno Setup 6 is absent, and the installer still needs
three source-built optional mod ZIPs (the ordinary source-built exconfig is now
available). Generated full upstream-derived Inno input is retained for inspection.
Fresh install, upgrades, paths-only mode, optional mods/downloads and uninstall
remain NOT TESTED. An actual official-release installer has not been extracted;
comparison so far is to the exact upstream distribution recipes.

The source ZIP covers the engine commit, not a complete archive of the external
vcpkg runtime sources. Matching dependency sources, package versions/patches,
build recipes, redistribution terms and relinking availability require review
before publication. No complete licensing-compliance certification is claimed.

Also pending: clean-machine runtime prerequisite handling, signing decision,
human final-package visual/listening/gameplay review, hosted Actions execution,
protected environment reviewers and an approved final tag/default-branch/repository
publication plan. Existing accepted gameplay test limitations remain documented in
the feature records and README. See RELEASE.md for the complete publication gate.
