# Virtue Stone Case — Stage E implementation candidate

**PENDING HUMAN EDITORIAL AND GAMEPLAY ACCEPTANCE � November 4, 2026.**
Human gameplay testing demonstrated that the previously accepted remarks were too
long for normal overhead speech. The owner authorized shorter lines and more
contextual timing. The earlier November 3 editorial acceptance is historical;
these four new candidates require fresh editorial and gameplay review.
Stage D remains accepted. Branch: `codex/virtue-stone-case`; renderer baseline:
`85a4a4010fe6d1700831a056b2e2ffd63c832d02`. No release is authorized.
Exact lines and trigger details: [editorial review](virtue_case_breadcrumb_dialogue.txt).

## Contextual opportunity refinement

Eligibility below remains latched and persistent. Delivery additionally requires a
recent individual real-stone inventory addition (native pickup or inventory drag),
or successful MARK. A accepts either; B requires the handled stone to have a valid
saved destination. Moving a whole container observes eligibility but supplies no
opportunity. Failed MARK, recruitment, load, unrelated inventory operations and
elapsed cooldown never provide context. Double-clicking a stone alone is not
intercepted; native drag/pickup and successful MARK are the small reliable hooks.

Each opportunity expires after 15 seconds of normal engine ticks, allowing brief
safe deferral while preventing stale chatter. It is not serialized: loading always
requires a fresh action, including old saves already satisfying the threshold.
Pending/completed bits and the saved five-game-minute minimum remain unchanged in
VSCH version 1. Expiration never discards eligibility. A delivery consumes both
current opportunities, so one handling action cannot queue both remarks; renew
context with another relevant action after the five-game-minute separation.
No repeated spell is required: moving a marked stone is sufficient.

Only the breadcrumb module/header and the existing successful native MARK hook
change production behavior. No broad interaction interceptor, frame scan, new
per-stone metadata, conversation system, renderer exception or duration change.
The shared corrected renderer is byte-identical to the baseline. Original Stage D,
case art/interactions, spell requirements, loose-stone activation and labels remain
unchanged. Fresh BG/Keyring real-engine evidence and preservation details are in
`docs/virtue_case_chatter_refinement_report.txt`.

## Eligibility and delivery

Before successful case delivery, A latches after the active party holds at least
three different original Virtue
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

## Post-delivery suppression — VERIFIED

The previous candidate did **not** guarantee suppression after delivery. The narrow
correction exposes the quest's existing persistent `DELIVERED` state through a
read-only accessor. Breadcrumb eligibility observation and delivery attempts now
return early when that state is terminal, clearing only pending bits. Spoken
completion bits are preserved. Pending hints become permanently ineligible at the
successful tabletop handoff; their pending bits are cleared on the next relevant
observation, ordinary minute, save or post-load observation. There is no opportunity
for speech between handoff and cleanup because every delivery attempt checks the
same terminal state. No new fields, trailer versions or per-case/per-stone records.

Commission acceptance, partial requirements, construction and ready-for-collection
are deliberately not suppression states. Merely possessing a fixture case also
does not count as quest delivery. Xanthia must actually place the completed reward
through the existing actor action. The saved terminal quest state survives case
pickup, transfer, dropping and later absence. Older Stage D delivered saves with no
Stage E trailer suppress eligibility immediately after normal load reconstruction.
An earlier save from before handoff retains its own earlier quest state normally.

Focused checks used earned quest archives, two actual MARKs and a real third-color
pickup. Both hints stayed pending in stages 1, 2, 3 and 4; a collection request also
left them eligible until physical handoff. Xanthia's actual walking/placement action
completed. Both pending hints were canceled, save/load and later Shamino recruitment
stayed silent, and dropping the picked-up earned reward did not reactivate either.
A legacy delivered save was loaded, both thresholds satisfied again with real
casts/pickup, then saved/reloaded without late hints. In another legitimate branch,
both hints were first spoken normally; successful delivery and save/load preserved
both completed bits. The normal spell costs were asserted throughout.

Early new focused runs failed on harness setup/navigation (Iolo not locally
reachable, searching outside the 32-tile CLI radius, and attempting one oversized
walking query). Final runs reload the earned tabletop checkpoint, then approach and
pick up the reward normally. These failed runs remain as failures in the evidence;
no production rule was relaxed. A first review smoke assertion incorrectly assumed
base BG had zero Usecode overrides; native BG has three. The corrected check
recognizes the actual native BG versus installed Keyring paths and overrides.
Another smoke attempt passed all 14 engine state checks but its final byte-hash
comparison rejected native configuration reformatting; parsed configuration values
were unchanged. The final check compares all configuration values instead.

## Human gameplay review

New isolated folder:
`C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e-final/human-review`.
Double-click **Play BG.cmd** or **Play Keyring.cmd** and choose one of seven states.
[Human instructions](../tools/virtue_case/HUMAN-BREADCRUMB-TEST.md) describe each.
Prepared slot 8 and matching gamedat restore the same state. Selecting a checkpoint
resets only this private profile; **C** continues it. The previous review folders,
player saves/configurations and original installed resources are preserved.

The four speaker states and two timing/deferral states were exported from successful
real-engine breadcrumb runs. The delivered-silent state came from the focused run's
actual handoff, reward pickup and original Shamino recruitment, with the temporary
casting-fixture case dropped separately. It contains the earned reward in the worn
backpack and no eligible hint. Provenance and archive hashes are in the review
folder's `fixture-provenance.json`; copied executable identity is recorded separately.
The same ordinary executable is copied for private normal launch, not rebuilt as an
alternate engine. `prepare_breadcrumb_review.py` creates a new folder only, refusing
to overwrite an existing review. No review artifacts or proprietary saves are committed.

Readability, natural timing/presentation and the final in-game experience remain
**PENDING HUMAN GAMEPLAY ACCEPTANCE**. All four lines and the approved trigger design
are **ACCEPTED**. Automated post-delivery behavior, BG/Keyring compatibility and the
listed regression scenarios are **VERIFIED**. No desktop GUI testing is claimed.

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
`tools/virtue_case/build.ps1`; post-delivery build log: external `test-output/virtue-stage-e/post-delivery-build.log`.
Fixtures supply actual unmarked stones, empty cases and normal casting supplies.
The owner separately approved Avatar-only starts near original companions and
Minoc. These do not establish travel progress or alter NPC locations/quest flags.
Actual pickup/drop/insertion, original recruitment/dismissal dialogue, real MARK,
spell payment and save/load are executed through native gameplay handlers.
No desktop control, GUI automation or desktop input injection is used.

Final post-editorial source results (normal engine shutdown, zero assertion failures):

| Suite | Ordinary BG | Keyring |
| --- | --- | --- |
| Breadcrumb gameplay and six review checkpoint assertions | 87 PASS | 89 PASS |
| Post-delivery suppression | 50 PASS | 50 PASS |
| Eligibility during commission/materials/construction | 48 PASS | 48 PASS |
| Accepted Stage D introduction/acceptance | 26 PASS | 26 PASS |
| MARK/RECALL requirements and effects | 42 PASS | 42 PASS |
| Case notes, SDL input and physical persistence | 79 PASS | 79 PASS |
| Exact private review launcher paths and prepared states | 29 PASS | 29 PASS |

The prior `fc391bb48` evidence (81/84 breadcrumb assertions, 26/26 Stage D,
42/42 spells, 79/79 labels) is preserved under `test-output/virtue-stage-e`.
Current evidence is separate: `C:/Codex/Ultima7 Decomp/test-output/virtue-stage-e-final/runs`.
Run names are `final-breadcrumb-{bg,keyring}`, `final-delivery-{bg,keyring}-04`,
`final-pre-delivery-{bg,keyring}`, `final-stage-d-{bg,keyring}`,
`final-spells-{bg,keyring}`, and `final-labels-{bg,keyring}`. Each retains
`result.json`, engine logs and semantic command/assertion records. Relative to the
previous breadcrumb totals, six new checks validate the captured review states;
Keyring needed two rather than three conditional food-use checks in this run.
Every previous named gameplay branch remains covered. No original suite assertions
were removed or weakened. The 14 final smoke runs are named
`review-final-02-{bg,keyring}-{checkpoint}`; together they have 58 passing assertions
with normal engine cleanup and unchanged configuration values. No player save
directory was used.

Reproduction with the documented CLI environment and a fresh output/run name:

```powershell
python tools/virtue_case/breadcrumb_checks.py --run fresh-stage-e-bg --capture-review
python tools/virtue_case/breadcrumb_checks.py --run fresh-stage-e-keyring --mod Keyring --capture-review
python tools/virtue_case/pre_delivery_checks.py --run fresh-pre-delivery-bg
python tools/virtue_case/post_delivery_checks.py --run fresh-post-delivery-bg
```

The focused scripts accept `--mod Keyring` too. They use retained earned Stage D
archives; those archives and the legacy Stage B/C fixture are proprietary-data-
bearing local test artifacts, not Git payloads. Normal inventory supply and approved
Avatar-only starts remain explicitly labeled fixtures; quest flags, NPC locations,
recruitment and spell destinations are never assigned by the tests.

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
also confirms accepted artwork, Stage D dialogue/delivery action, native case and
spell implementation are unchanged. The only Stage D source addition is the
read-only `is_delivered()` accessor; no transaction, conversation or timing changes.

## Review limits and remaining human checks

Automated checks verify native dispatch and exact strings; the owner separately
approved the wording. Automated checks do not establish gameplay acceptance
or subjective readability/timing. Human review should assess overhead line wrapping,
duration and character voice at ordinary play resolutions, and whether quiet-moment
placement feels natural. Physical keyboard/IME generation remains a human check;
SDL event-handler input regressions are automated. Cutscene/transition/hostile and
incapacity guards were inspected in source; not every possible protected sequence
has a dedicated live scenario. No claim of exhaustive cutscene certification is made.
Map-existence and numeric destination rejection are source-verified; no artificial
invalid-destination setter was added for testing. The approximation cannot recover
previously marked stones no longer in the party. Stage E wording is human editorial accepted; runtime presentation and final
gameplay remain pending human acceptance. Stage D remains human accepted and unchanged.

Protected-file checks confirm all 399 tracked BG resource/Keyring/Native Wait/Orb
Chart files match the accepted Git blobs. Native-case source, Stage D dialogue,
delivery action and all four Stage E strings are unchanged; the quest module adds
only the read-only delivered accessor. Baseline hash evidence is retained in
`test-output/virtue-stage-e/protected-baseline-sha256.json`; the protected `exult-fork` worktree is
clean at `7bf0bd8ab`.

No merge, tag, publication, packaging or final project acceptance is performed.
