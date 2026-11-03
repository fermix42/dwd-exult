# Virtue Stone Case — Stage E implementation candidate

**PENDING HUMAN EDITORIAL ACCEPTANCE — November 3, 2026.**
Branch: `codex/virtue-stone-case`; accepted baseline: `786409bd587408c050164b03bd14592122a63459`.
This candidate does not reopen Stage D acceptance or authorize a release.
Exact writing is grouped by speaker in [the editorial review file](virtue_case_breadcrumb_dialogue.txt).

## Eligibility and delivery

A latches after the active party holds at least three different original Virtue
Stone colors (shape 330, frames 0–7). Three objects of only two colors do not qualify.
B uses the owner's expressly approved approximation: two distinct currently
party-held native Virtue Stone objects with valid saved MARK destinations suffice,
including existing saves. Re-marking one object cannot count twice; two physical
objects of the same color can qualify. This is gameplay-state evidence, not a
reconstruction of every historical MARK cast. Former stones no longer held are
not reconstructed, and no per-stone history or identity metadata is added.

Recursive inventory scans include the Avatar, active companions and their nested
containers, including real cases. A set deduplicates object references. B rejects
uninitialized/default x=y=0, invalid coordinates/lift/map numbers, and destinations
whose mapped U7MAP file does not exist in the current game/mod. Existing native
stone destinations and serialization are unchanged. Eligibility is observed on
relevant successful container additions, party recruitment, successful native MARK,
post-load party reconstruction, and saving. There is no per-frame or repeated
minute inventory scan. Once latched, pending survives removal of every stone.

A prefers Iolo (NPC 1), then Shamino (3). B prefers Shamino (3), then Dupre (4).
Speakers must be living, active party members able to act and speak, on the
Avatar's map within eight tiles. Absent or temporarily unsuitable speakers defer
rather than consume the hint. Native `Actor::say` overhead speech is used, with
no new topics, portrait or dialogue window. Original conversations are unchanged.

Delivery is attempted on ordinary active game-minute transitions. It reuses the
existing read-only Native Wait eligibility gate, which rejects combat, UI ownership,
paused time, conversations, remote camera, transitions, stopped time, nearby capable
hostiles, party actions/deferred scripts and incapacity. Additional guards defer
while the palette is faded, sprite effects or overhead text are active, or the
Avatar is casting. Native Wait itself is unchanged. These conservative checks can
delay hints; the player need not hear them to discover or commission the case.
A has priority when both can speak. A saved five-game-minute minimum cooldown and
existing-text guard separate the two remarks. Each is completed only once.

## Persistence and compatibility

A versioned optional 16-byte `VSCH` trailer in `usecode.var` stores the two hints'
pending/completed bits and the next allowed absolute game minute. It follows the
unchanged optional 20-byte Stage D `VSCQ` record when present. Invalid or absent
Stage E records reset Stage E only. The reader independently reads both optional
records from their original starting offset. Old saves acquire eligibility after
native inventory and party restoration, before ordinary player input; no new MARK
is required for the approved existing-state approximation. Loading a pre-eligibility
save naturally restores that earlier playthrough state. Completion and pending
state persist in subsequent saves. Other games are excluded by the BG gate;
Keyring uses the same native implementation with its actual map and Usecode inputs.
No mod source or DWDKeyring dependency is introduced.

## Verification

All runs use the same ordinary Release/x64 `Exult.exe` through explicitly activated
`--test-cli`, fresh isolated run roots and real installed BG/Keyring Usecode. Build:
`tools/virtue_case/build.ps1`; final build log: external `test-output/virtue-stage-e/build-8.log`.
Fixtures supply actual unmarked stones, empty cases and normal casting supplies.
The owner separately approved Avatar-only starts near original companions and
Minoc. These do not establish travel progress or alter NPC locations/quest flags.
Actual pickup/drop/insertion, original recruitment/dismissal dialogue, real MARK,
spell payment and save/load are executed through native gameplay handlers.
No desktop control, GUI automation or desktop input injection is used.

Final-source results (normal engine shutdown, zero assertion failures):

| Suite | Ordinary BG | Keyring |
| --- | --- | --- |
| Breadcrumb gameplay | 81 PASS (`stage-e-final-bg`) | 84 PASS (`stage-e-final-keyring-04`) |
| Accepted Stage D introduction/acceptance | 26 PASS | 26 PASS |
| MARK/RECALL requirements and effects | 42 PASS | 42 PASS |
| Case notes, SDL input and physical persistence | 79 PASS | 79 PASS |

Evidence is retained outside Git under
`C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e/runs/`. Regression run names
are `stage-e-final-d-{bg,keyring}`, `stage-e-final-spells-{bg,keyring}`, and
`stage-e-final-labels-{bg,keyring}`. Each has `result.json` and engine logs;
breadcrumb runs additionally retain every semantic command/assertion in
`breadcrumb-checks.json`. The Keyring total includes three verified ordinary food-use checks required by
normal waiting. No player save directory was used.

Reproduction with the repository's documented CLI environment set:

```powershell
python tools/virtue_case/breadcrumb_checks.py --run fresh-stage-e-bg
python tools/virtue_case/breadcrumb_checks.py --run fresh-stage-e-keyring --mod Keyring
```

Use a new run name each time; the suite resolves its retained pre-Stage-E legacy
fixture from the existing Stage B/C evidence tree. It needs those local archives
for migration tests; they are proprietary-data-bearing test artifacts, not Git
payload. Spell and label suites use the same existing archives via `--checkpoint`.
The test helper's video settings apply only to its disposable CLI config.

The breadcrumb suite tests threshold/deduplication, primary/fallback/absent speakers,
combat/UI/conversation deferral, pending after complete inventory removal, pending
and completed save/load, real repeated and second-object MARK with normal costs,
same-color distinct objects, both pending with persisted separation, old save
migration, and independent real commission acceptance with both optional records.
The old-save archive contains retained Stage A native destination fixtures; it is
not claimed as proof of historical player casting. Separate live MARKs establish
successful-casting behavior. Unmarked objects establish default destination rejection.

Earlier exploratory runs exposed test-adapter gaps (inventory visibility, original
answer capitalization, item pickup reach, sleep/recruitment timing, and save-slot
limits). They remain in external evidence as failures or incomplete runs, not as
passes. The final Keyring first attempt stopped at sleeping Shamino's original
conversation. Its second attempt correctly deferred B because normal waiting had
left the Avatar hungry. Subsequent attempts established that Native Wait can
stop early for hunger; the harness checks completion, feeds normally and resumes
the remaining interval. It uses actual food interaction/targeting before waiting
for a quiet opportunity, never granting membership or bypassing hunger. No hint
implementation or original dialogue changed for these retries.

The final-source regression suites cover original spell failures/costs/effects,
native SDL text input/Backspace/Enter/Escape, all eight note persistence, physical
stone and case movement, successful/cancelled MARK, real RECALL, original quest
introduction/acceptance and the three accepted editorial corrections. Source diff
also confirms accepted artwork, Stage D module/dialogue/delivery, native case and
spell implementation are unchanged apart from observer calls in shared integration.

## Review limits and remaining human checks

Automated checks verify native dispatch and exact strings, not editorial approval
or subjective readability/timing. Human review should assess overhead line wrapping,
duration and character voice at ordinary play resolutions, and whether quiet-moment
placement feels natural. Physical keyboard/IME generation remains a human check;
SDL event-handler input regressions are automated. Cutscene/transition/hostile and
incapacity guards were inspected in source; not every possible protected sequence
has a dedicated live scenario. No claim of exhaustive cutscene certification is made.
Map-existence and numeric destination rejection are source-verified; no artificial
invalid-destination setter was added for testing. The approximation cannot recover
previously marked stones no longer in the party. Stage E remains a writing and
implementation candidate; Stage D remains human accepted and unchanged.

Protected-file checks confirm all 399 tracked BG resource/Keyring/Native Wait/Orb
Chart files match the accepted Git blobs. Stage D and native-case source/dialogue
checks also match after normal checkout newline conversion. Hash evidence is in
external `protected-baseline-sha256.json`; the protected `exult-fork` worktree is
clean at `7bf0bd8ab`.

No merge, tag, publication, packaging or final project acceptance is performed.
