# Native BG Virtue Stone Case commission (unpublished candidate)

**Stage D - Human dialogue editorial review: ACCEPTED.** Owner approval and the
three final wording corrections are recorded in the editorial report linked below.
Other gameplay checks and release milestones retain their previous status.

The current writing and full script are in [Final editorial review](virtue_case_editorial_review.md)
and [Dialogue script](virtue_case_dialogue.txt). Earlier writing examples below are historical.

Human functional review accepted the original quest/delivery. Current corrections
and review evidence are in [Stage D human review corrections](virtue_case_review_corrections.md).
That report supersedes dialogue topic, delivery coordinate and private review
scale references below; the following records the original Stage D implementation.

The accepted physical case, artwork, stones, spells, notes, SDL keyboard routing,
Wait, Orb Chart, combat and audio implementations remain frozen. Quest code is
separate in native_case_commission.cc. It appends topics at the existing native
Usecode conversation boundary while the original effective NPC function runs.
Xanthia is NPC86/function0x456, William NPC93/function0x45d. Original Keyring's
source has no replacements for these NPCs; loaded functions are still inspected
in each profile. Original name/job/murder/Fellowship/Guild/Owen dialogue remains
in the original Usecode, rather than being copied into a replacement function.

## State and topics

Stages are0 NOT_STARTED,1 COMMISSION_ACCEPTED,2 REQUIREMENTS_IN_PROGRESS,
3 BUILDING,4 READY_FOR_DELIVERY,5 DELIVERED. Requirement bits:1 timber,2 cloth,
4 fee. Any completed requirement moves acceptance to requirements-in-progress.
Only all three start construction. The deadline is the original game clock's
absolute minutes plus2880, never wall time or just the clock hour. Speaking does
not reset it. Time expiry changes readiness only; it creates no case.

Quest state is an optional20-byte VSCQ/version1 trailer after the existing
usecode.var local-statics terminator: magic4, version uint32, stage uint32,
requirement mask uint32, deadline uint32. It does not claim existing global quest
flags, timers, function IDs or mod IDs. Old saves without this record start blank.
Only an active BG commission writes it. Normal save archives already contain
usecode.var. Loading/reset clears transient conversation state and validates the
record before applying it. Ordinary SI statics are unchanged.

A conversation-local set suppresses each used quest topic immediately, including
unchanged failures and progress questions. It resets at the outer NPC Usecode
conversation boundary and is not saved. Every native menu refresh first removes
only the quest-owned topic names, then adds currently legal, unsuppressed topics.
Persistent stage/requirement bits permanently remove completed transactions.
Original NPC answers are not suppressed. A successful commission removes the
introductory offer immediately, without closing the conversation.

## Materials and payment

A real original shape330 Virtue Stone anywhere in active party inventories,
including nested containers, enables the offer. Progress thereafter does not
require continuing ownership of a stone. The stone is shown, not consumed.

The lining consumes one ordinary cloth item, shape851, matching original BG /
Keyring sewing content. No bandage, clothing or thread substitutes are removed.
The Guild fee is exactly100 gold (shape644). William quotes20 gold before a
separate purchase topic; on purchase he sends timber directly to the Guild.
No lumber object is placed in the Avatar's inventory.

William's original function describes supplying planks to Owen and the Guild but
has no existing retail timber price.20 gold is a new, explicit design choice for
selected seasoned wood and local delivery, kept well below the approved100-gold
labor/fittings charge. Original BG merchant conversations quote10 gold for a
cradle and20 for a lute/sextant (docs/usecode.dis near99307/99394/99418), then ask
whether to buy. The new timber exchange follows that price-before-purchase
convention. It does not change any original service or price.

Transactions check both persistent stage and completion bits again before taking
anything. Affordability/cloth count is checked across the actual active party's
nested inventory before ordinary remove_quantity runs on the engine thread.
Insufficient inventory removes nothing, advances nothing, and suppresses that
attempt only for the current conversation. No game state can interleave between
count, removal and marking that requirement complete. The last requirement sets
the48-hour deadline once. There is no route back from construction to payment.

## CLI tests and fixture provenance

Use tools/virtue_case/commission_checks.py and transaction_checks.py. They select
actual dialogue options from the normal loaded Usecode; they do not set quest
flags or invoke quest transitions as test setup. Read-only case_commission_state
reports state, inventory counts and NPC/schedule observations.

The owner explicitly approved a disposable Minoc starting fixture after the
AGENTS.md restriction on teleporting actors was raised. test_case_commission_start
places only the Avatar at Minoc as that labeled start, never moving Xanthia or
William or asserting natural travel. test_case_commission_items provides explicit
stone/gold/cloth inventory conditions, not quest progress. All subsequent quest
acceptance and requirements come from real conversations, native inventory and
normal saves. NPC delivery walking must remain real and is not a fixture.

The first D1 compile failed on a header name, and a later fixture compile used
an incorrect move overload; both corrected before successful engine runs. D1
passed19 assertions in BG and19 in Keyring. Initial D2 navigation runs failed or
were interrupted because the test harness searched beyond observed terrain or
replaced unfinished long walking routes. They are retained and not counted PASS.

Stage D2 d2-transactions-bg-04 and d2-transactions-keyring-04 each passed33
real-engine assertions, including both cloth/payment orders and deadline save/load.


## Construction and physical delivery

The final requirement records an absolute game-minute deadline, current time +2880.
Normal time, native Wait and save/load all use the same game clock; progress dialogue
does not modify the deadline. Expiry changes only BUILDING to READY_FOR_DELIVERY.
The case is not granted automatically.

Collection ends the existing NPC conversation through its ordinary bye branch.
A private Actor_action walks Xanthia to the original table at1578,387, using native
pathfinding, door handling and bounded retries. There is no NPC teleportation or
schedule-table replacement. The actor faces the table, reaches, places one native
empty case at supported tabletop lift2, straightens and walks aside. A blocked route
or full tabletop leaves collection retryable. Follow Xanthia normally: the ordinary
engine stops offscreen NPC walking. The action is intentionally not serialized;
load before placement leaves READY with no reward, so the player can ask again.
Placement and DELIVERED happen in the same engine-thread operation. Save after
placement preserves both terminal state and the ordinary world object. Loss or
removal of that object cannot reopen the quest or create a replacement.

D3 BG calendar run01 passed the47/48-hour boundary and hunger/food interactions,
but failed delivery observation. Runs02–05 retained failures involving offscreen
walking, doorway interruption and test navigation. The bounded local replan and
following test corrected those issues. Run06 reached the bench and created the
case, but the test incorrectly required one-handed reach frame5; this sprite uses
native two-handed reach frame8. Run07 passed18 assertions including actual reach,
placement at lift2, pickup, specialized gump opening, empty labels, save/reload and
permanently absent completed topics. Screenshots are actual engine captures.

Full final D3 runs d3-delivery-bg-final and d3-delivery-keyring-01 each passed32
assertions, including the47/48-hour boundary and full delivery/pickup lifecycle.
Existing label/spell interoperability checks d4-labels-bg and d4-labels-keyring
passed79 each through real SDL text/key events and original spell Usecode.


## Integration review evidence

Final primary suites, per profile (BG and original Keyring): dialogue19,
transactions33, calendar/delivery32, accepted label interoperability79,
accepted spell rules42, quest edges7. Total424 assertions across both profiles.
All primary suites passed with normal engine cleanup. Edge tests initially assumed
2 real seconds would advance a game minute; those runs failed and are retained.
The corrected test polls ordinary gameplay time for a bounded35 seconds and passed.
No clock, quest progress, destination or NPC delivery position was fabricated.
The sleep probe rejected radius64 (CLI maximum32); the follow-up found no existing
bedroll/nearby beds in the starting fixture. It does not certify sleeping.

Private review package is at
C:/Codex/Ultima7 Decomp/test-output/virtue-stage-d/human-review.
Each profile has not-started, requirements, building, ready and delivered saves.
The first setup attempt assumed Keyring gamedat was below the BG gamedat directory;
it failed before copying Keyring. The final preparer observes the real selected
engine gamedat path; all ten checkpoints were verified before copying.
Launchers show a checkpoint menu and restore only the disposable review profile.
The private Keyring config explicitly selects canonical portable save/gamedat paths.
A first CLI smoke exposed a formatting error in that private config; corrected to
the engine's native multiline format. Both final launcher smoke tests loaded
READY through the actual ordinary engine executable and canonical prepared paths.
Both profiles were then restored to NOT_STARTED. No desktop automation was used;
human desktop launch and acceptance remain pending.

Actual screenshots:
- runs/d3-delivery-bg-final/xanthia-reaches-workbench.png
- runs/d3-delivery-bg-final/case-placed-on-workbench.png
- runs/d3-delivery-keyring-01/xanthia-reaches-workbench.png
- runs/d3-delivery-keyring-01/case-placed-on-workbench.png
under the Stage D output root. The tabletop scene was visually inspected. These
are real engine captures, not artwork previews. Accepted art/source files were
unchanged against starting HEAD fb0ceafffb2971f9175ee6a10e27edaa1b0d5474.

Not certified automatically: sleeping during construction, a completely blocked
tabletop, leaving/returning to Minoc, every NPC schedule boundary, or natural quest
pacing/visual acceptance. There is no new sleep automation framework. The owner
can exercise these with the included building/ready checkpoints. This limitation
is explicit; the passing automated suites do not constitute human acceptance.
No companion breadcrumbs, merge, tag or release publication was performed.


## Branch and source inventory

Starting HEAD: fb0ceafffb2971f9175ee6a10e27edaa1b0d5474.
Branch: codex/virtue-stone-case, isolated exult-virtue-native worktree.
Logical commits:118b3d3c2 (dialogue/state),48bf315c4 (transactions),6ff4fac30
(construction/delivery), followed by the D4 review/checkpoint documentation commit.
The feature worktree and protected exult-fork checkout were clean at final checks.

Changed source/build files: Makefile.am; msvcstuff/vs2019/Exult.vcxproj;
native_case_commission.h/.cc; native_case_delivery.inc; usecode/ucinternal.h/.cc;
cli_driver.cc; cli_world.cc; tools/test_cli/driver.py.
Tests/review tools: tools/virtue_case/{commission_checks,transaction_checks,
delivery_checks,commission_edge_checks,prepare_quest_review,start_quest_review}.py.
Documentation: docs/test_cli_protocol.md, docs/virtue_case_commission.md,
tools/virtue_case/HUMAN-QUEST-TEST.md. No accepted art, case/spell/editor production
implementation, Native Wait, Orb Chart, combat, audio or ultima7-modern files changed.

The review Exult.exe was built from the D3 code before the D3 commit was recorded;
its embedded DWD revision therefore reads48bf315c4. Its tested code includes the
committed D3 delivery implementation; D4 changes only tests and documentation.
The review executable is the same ordinary build tested by all final D3/D4 runs.
