# Stage D human review corrections (unpublished candidate)

Starting HEAD: a58f8235829dc639992e2d0b66eb409036acb71d.
Branch: codex/virtue-stone-case; isolated exult-virtue-native worktree.

Commits: b86a9b634 (dialogue/merchant knowledge);88328959b (native sprite
observations and private review scale correction);65e070b0a (visible studio-table
handoff), followed by the integration/review evidence commit.

## Corrections

William's original function0x45d describes selling sawmill planks to Owen and the
Artist's Guild (docs/usecode.dis near65299). The new exchange now concerns only
wood,20 gold and Guild delivery. Topics are `timber delivery` and `buy timber`.
His purchase no longer calls the construction receipt helper. That helper also
checks speaker86 before appending construction information. The state transition
and20-gold transaction are unchanged, including when timber is the final material.

Xanthia's opening starts with direct quoted Avatar speech, attributed after the
line. Her reply and the Avatar's practical request remain intact. Only the actual
showing of the real stone is narrated as an action. Acceptance explains William's
delivery and the two days. Materials are listed in ordinary prose with `and`,
rather than semicolon status fragments. Quest-only strings format quotation and
paragraph markers locally; no general conversation behavior was changed.

Both contexts already used shape1601/frame0,28x20 native pixels, origin14,19.
The same frame is installed from production resource frame0; world paint and the
backpack both use it directly. Original and corrected native dimensions therefore
remain28x20. The apparent difference came from the old private review's320x200
world and640x400 inventory layers. At1280x800 this made the world sprite112x80
screen pixels versus56x40 in inventory. The revised private review uses640x400
for both:56x40 screen pixels at2x,28x20 at1x. No PNG/SHP, origin, footprint or
production object renderer was changed or resampled. No player/global UI setting
was forced. Players choosing different world/UI scales still get those independent
scales; this correction deliberately does not override their settings.

The handoff now uses existing Guild studio table shape633/frame1 at1592,370,0,
with a clear front-corner reward patch1592,370,2. It is inside the Guild's working
studio, away from the old wall and tall weapon display. Table footprint1590..1592,
368..370, height2. South approach1591,371 is reachable; Avatar pickup approach is
1592,371. East/west/north approach bounds now use the actual table footprint.
Facing targets the real reward point. The reach,350ms placement, straightening,
step-aside, speech, native pathfinding/retries and exactly-once terminal transition
remain the accepted action. Original map geometry, tools and furniture are intact.

Investigation rejected Xanthia's scheduled workbench1018 at1572,386 because its
hammer has a2x2 footprint covering the proposed clear spot. Native collision
correctly refused that attempted placement without creating a case. Other original
surfaces were observed; the selected studio table passed actual walking, placement,
centre hit/visibility, pickup and save/load checks. Actual screenshots were visually
inspected after the ending speech faded, without changing camera behavior.

## Preservation and verification

Quest state,48-hour absolute deadline, stone prerequisite, cloth/fee costs and
persistence format are unchanged. Conversation-local suppression and persistent
state guards still prevent repeated transactions and terminal restart. Tested a
save while Xanthia was reaching: this is an earned READY checkpoint with the NPC
at the table, not a forced flag or actor teleport. A real party stone was then
dragged onto the new patch. Collection remained READY, suppressed the failed choice
for that conversation, and created nothing. Normal pickup cleared the patch; the
next conversation delivered exactly one case, still terminal after save/reload.

All primary runs below passed with normal engine cleanup. Existing accepted
label checks include physical stone insertion/removal, eight notes, SDL text,
Backspace/Enter/Escape, multiple cases, transfers, world drops, save/load, and
original spell interoperability. Spell checks separately cover MARK/RECALL,
correct mana/reagents, missing spellbook/spells/resources and cancellation.

| Real-engine run | Assertions |
|---|---:|
| r4-dialogue-bg | 21 |
| r1-transactions-bg | 35 |
| r2-sprite-bg-2x-04 | 9 |
| r4-sprite-bg-1x | 9 |
| r3-delivery-bg-04 | 35 |
| r4-labels-bg | 79 |
| r4-spells-bg | 42 |
| r4-obstruction-bg | 9 |
| r4-dialogue-keyring | 21 |
| r1-transactions-keyring | 35 |
| r2-sprite-keyring-2x-04 | 9 |
| r4-sprite-keyring-1x | 9 |
| r3-delivery-keyring-04 | 37 |
| r4-labels-keyring | 79 |
| r4-spells-keyring | 42 |
| r4-obstruction-keyring | 9 |

Total: **480 gameplay assertions**,0 failures in these final runs.
Ten additional checkpoint preparation assertions and two copied-review executable
smoke assertions passed. Runtime evidence/JSON and screenshots are under
C:/Codex/Ultima7 Decomp/test-output/virtue-stage-d-revision.

Retained unsuccessful runs are not counted PASS: initial R1 dialogue assertions
expected translated quotes before quest strings were locally formatted; initial
R2 tests compared a requested drop tile rather than the actual native drag result,
then attempted party-only inventory inspection on a world object, then left an
inventory gump obscuring pickup. Corrected tests observe actual placements and
legal visible controls. First R3 workbench candidate was blocked by the original
hammer; this was a real placement refusal, not a valid delivery run. Final studio
runs passed. Initial launcher verification mishandled multiline XML whitespace and
a Keyring map subdirectory in its byte comparison; final recursive verification
passed. No failed/cancelled run is represented as acceptance evidence.

Not certified: sleeping through construction, leaving/returning to Minoc, every
schedule boundary, or every possible arrangement of furniture over the table.
The new delivery patch WAS physically obstructed and cleared in both profiles.
No computer control, desktop input automation, quest-state forcing, or NPC delivery
teleport was used. Avatar-only Minoc initial fixture remains explicitly owner-approved.

## Review package

C:/Codex/Ultima7 Decomp/test-output/virtue-stage-d-revision/human-review.
Old Stage D and Stage B/C review folders and personal saves remain intact.
Play BG.cmd / Play Keyring.cmd show five earned checkpoints: not-started,
requirements, building, ready and delivered. Choose4 for the revised handoff;
follow Xanthia normally. Choose1 for new introduction; ask William about `timber
delivery`, then `buy timber`. Choose5 for permanent completion and drop/reload checks.
S is native save/load; prepared archive slot8 matches the chosen checkpoint.
Both profiles were restored to not-started after verification. The actual copied
review executable/resources loaded READY in fresh disposable CLI sessions; normal
desktop launch and final visual acceptance remain the owner's checks.

Actual engine captures: runs/r4-dialogue-bg/xanthia-introduction.png;
runs/r3-delivery-{bg,keyring}-04/{xanthia-reaches-workbench,
case-placed-on-workbench,unobstructed-reward-after-step-aside}.png;
runs/r4-sprite-{bg,keyring}-1x and r2-sprite-{bg,keyring}-2x-04 contain paired
backpack/world captures. These are engine screenshots, not resized artwork previews.

Changed files since starting HEAD: native_case_commission.cc,
native_case_delivery.inc, cli_world.cc; tools/virtue_case/{commission_checks,
transaction_checks,delivery_checks,sprite_checks,obstruction_checks,
configure_quest_review,prepare_quest_review}.py; tools/virtue_case/HUMAN-QUEST-TEST.md;
docs/virtue_case_commission.md and this report. Accepted art, case/spell/editor
production code, Wait, Orb Chart, combat, audio and ultima7-modern are unchanged.
No merge, tag, release publication or companion breadcrumbs. Final human acceptance
is pending.

## Quest dialogue for writing review

The following are the actual new response strings, with local quote/paragraph
markers resolved. Original NPC dialogue is not reproduced or rewritten.

### Virtue Stone case

"I should like to commission a carrying case for the Virtue Stones," thou sayest.

"Virtue Stones? I am not familiar with them. Might I see one?"

Thou showest her a stone from thy company's belongings.

"Oh, how lovely! And quite unusual. Tell me, what sort of case hast thou in mind?"

"Something sturdy, yet small enough to carry on my travels. I should hate to have such precious things rattling about in my pack," thou repliest.

"Hmm. A fine wooden case, lined with soft cloth to protect the stones. Aye, I believe I could fashion something suitable."

"I shall need good timber and a length of cloth for the lining. The Guild's labor and fittings will cost one hundred gold."


### commission case

"Aye, I shall undertake thy commission. Arrange the timber with William; he can send it straight to the Guild. Bring me the cloth and our fee when thou art ready. Once all is in hand, allow us two days for the work."


### case materials

"We still need [only outstanding: timber from William, a length of cloth and/or one hundred gold for labor and fittings]. Once all is in hand, allow us two days for the work."

### timber delivery

"I require some fine timber. Couldst thou arrange its delivery to the Artist's Guild?" thou askest.

"Aye, I can provide suitable wood for twenty gold and have it sent to the Guild. Wilt thou buy it?"


### buy timber

"Twenty gold for sound, seasoned timber, delivered to the Artist's Guild. Thou hast not enough gold at present."

"Twenty gold, then. I shall have the timber sent straight to the Artist's Guild."


### give cloth

"I need a length of cloth for the lining. Bring one when thou art ready."

"This cloth will do very well. I shall keep it safe for thy case."


### pay Guild fee

"The Guild's labor and fittings cost one hundred gold. Thou hast not enough. We can settle it when thou art able."

"One hundred gold. I thank thee; our fee is settled."


### collect case

"Thy case is finished, but I need a little clear space upon my workbench. Make room, then ask me again."

"Thy case is finished, Avatar! Come, I shall set it upon my workbench for thee."


### case progress

"Good work takes time. Our two days are not yet passed. Return anon, Avatar."


Successful final cloth/fee receipt, spoken only by Xanthia:

"All is in hand. The Guild may begin thy case; allow two days for the work."

Give me a little room to reach my workbench, Avatar.

Pray clear a little space upon my workbench.

There! A fine piece of work, if I may say so myself. I trust it shall serve thee well, Avatar.

If the route cannot start: "Give me room to reach my workbench, then ask me again."
