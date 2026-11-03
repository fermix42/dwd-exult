# Stage D dialogue editorial acceptance (unpublished implementation)

**Stage D — Human dialogue editorial review: ACCEPTED.**

Recorded2026-11-03 from the owner's explicit acceptance, conditional on three exact
wording corrections. The owner reviewed and approved the complete dialogue; those
three corrections were explicitly authorized and are now implemented and verified.
No further editorial revision is required. Acceptance is limited to the dialogue
and existing approved Stage D scope. Separately outstanding gameplay features,
checks and release milestones retain their previous status. Automated regression
results support implementation verification; they are not human GUI acceptance.

Finalization baseline:6044249b17e052ccd11eb793feb81f242e9abee3 on
codex/virtue-stone-case. The earlier comparison and385-assertion evidence below
are retained as history; the acceptance verification at the end is current.

Starting HEAD:98cb4f4a38f0bed68c8a79e7ce261a810d4a9e7a.
Branch:codex/virtue-stone-case, isolated exult-virtue-native worktree.

The complete new dialogue, grouped by topic and including all seven outstanding
materials lists, is [virtue_case_dialogue.txt](virtue_case_dialogue.txt).
This report supersedes the previous writing examples, not the accepted gameplay.

## Original-writing comparison

Primary reference: the retained original BG/FoV disassembly docs/usecode.dis.
Inspected original Xanthia0x456 (61850â€“61995), William0x45d (65212â€“65325),
Gladstone0x455 (61502â€“61640), Seara0x458 (62655â€“62735), Erethian0x9a/FoV
(44â€“125 and213â€“236), Beverlea's merchant exchange (99300â€“99335), and physical
narration around Burnside/Owen's plans (64432). Original topics were checked
alongside their associated responses, not inferred from NPC names.

Xanthia is specifically a candelabra maker, experienced in commissions, not a
generic shopkeeper. She is introduced as cheerful, takes the Guild seriously,
reacts strongly to the murders and has opinions about Elynor and Owen. Her new
responses retain practical confidence and appreciation for attractive objects,
with restrained enthusiasm about protecting the stones and accepting the lining.
She does not gain additional lore or knowledge about their magic.

William is worried and sometimes abrupt; his original trade is cutting logs into
planks for Owen and the Guild. New speech stays practical and subdued. It neither
makes light of the murders nor inserts a fresh murder discussion into the sale.
He knows only the wood, price and Guild delivery. Original investigation,
Fellowship and Owen branches remain loaded and untouched.

Gladstone's easy welcome and business concerns and Seara's craftsman's pride
provide Guild context without transferring their individual voices to Xanthia.
Erethian provides a FoV example of modern narrator prose surrounding strongly
individualized speech; his theatrical character is not copied into this quest.
Original narration describes what you see/show/tell in ordinary English. Spoken
Britannian forms belong inside speech. Merchant examples quote a price, ask
whether to buy, then give short character-specific acknowledgements or refusals.

No Conversation Stylebook was located in the available repository files. No
Stylebook quotation or unavailable-source claim is made. Original scripts were
the authority; no full original conversations are reproduced in the deliverables.
Keyring source names these NPCs but supplies no NPC overrides for the effective
0x456/0x45d conversation functions. Runtime checks inspect both loaded functions;
Keyring's separate shape0x456 spell-item code is not confused with NPC0x456.

## Earlier candidate editorial decisions (historical)

The accepted introductory speech is preserved. Brief `you say` / `you reply`
attribution remains only where required to distinguish Avatar speech in the
existing single-NPC text presentation. It follows direct speech; no introductory
`You ask Xanthia about...` narration or new portrait/UI is introduced. Showing
the actual party-owned stone is the only narrated physical action in the exchange.

| Before | Previous candidate wording |
|---|---|
| `thou sayest` / `thou repliest` | `you say` / `you reply` |
| `Thou showest her a stone from thy company's belongings.` | `You show Xanthia one of the stones.` |
| `I shall need good timber and a length of cloth for the lining.` | `Good timber, and soft cloth for the lining. We must give those stones a proper home!` |
| `Aye, I shall undertake thy commission...` | `I shall be pleased to make it...` |
| `We still need ... Once all is in hand, allow us two days...` | `Before we can begin thy case, I must have ... Bring what thou canst when thou art ready.` |
| `One hundred gold. I thank thee; our fee is settled.` | `Splendid! That takes care of the Guild's fee.` |
| `This cloth will do very well. I shall keep it safe...` | `Yes, this will make a fine lining! I shall put it aside for thy case.` |
| `All is in hand...allow two days...` | `There, that is everything! We can get to work on thy case now.` |
| `Good work takes time. Our two days are not yet passed. Return anon...` | `Patience, Avatar! I would not have thee carrying a poorly made case because we hurried the work. Give us a little more time.` |
| William: `I can provide suitable wood...Wilt thou buy it?` | `I have suitable planks. Twenty gold for the wood and delivery to the Guild. Dost thou wish to buy them?` |
| William: `Twenty gold, then...` | `Very well. I shall have the planks sent to the Artist's Guild.` |
| `Pray clear a little space upon my workbench.` | `There is no room for the case here. Clear a space on the table, please.` |

The complete file contains the full acceptance, insufficient-money, missing-cloth,
collection and obstruction lines. Acceptance explains two days once; materials,
progress and final-requirement acknowledgements no longer repeat the duration.
A final-requirement acknowledgement can still occur only after every requirement
is complete. The approved final handoff sentence is unchanged verbatim.

The collection announcement changes only `my workbench` to `the table`, matching
the accepted studio location. Path interruption mentions inability to reach the
table; occupied tabletop messages ask for space. The generic action-start failure
is deliberately non-specific because the existing guard covers more than a blocked
route; no new causal claim, branch or recovery mechanism was added.

No topic label changed. Original Xanthia/William menu labels do not collide with
the nine existing quest-owned labels. Short alternatives such as `case`, `payment`
and `materials` lose the clear context already supplied by the current labels;
there is no sufficient editorial benefit to reopening accepted menu selection.

## Scope and verification

A string/character-literal-aware source comparison confirms that the ONLY
production edits are string literals in native_case_commission.cc and
native_case_delivery.inc. Quest guards, transitions, costs, timing, save format,
world/UI scaling, case art, labels, spell code, table coordinates and350ms placement
timing are identical to the accepted baseline. CLI tests were extended rather
than weakened. No new test infrastructure or conversation engine was built.

Existing transaction checks additionally earn a timber-only checkpoint by normal
purchase. Existing dialogue checks load that real checkpoint and use the already
owner-approved Avatar-only Minoc starting fixture. No NPC is moved as setup and no
quest flag or requirement bit is assigned. All seven materials combinations are
then reached through ordinary handoffs/payments/save-load and checked against the
actual paged response text, not a direct setter or synthetic quest-state model.

Final ordinary-build real-engine runs all passed with normal cleanup.
Normal dialogue greetings/job/bye, quotation/paragraph formatting, exchange order,
stone presentation, speaker attribution, failed-attempt suppression, permanent
transaction removal and terminal completion are checked in both game profiles.
Original accepted spell/keyboard behavior is exercised by the existing label
interoperability suite; separate prior42-assertion-per-profile spell-requirement
coverage remains applicable because those implementations did not change.

Not newly certified: sleeping, leaving/returning to Minoc, every NPC schedule
boundary or human judgement of writing quality. These were not reopened by this
editorial revision. No GUI/computer-control/desktop-input testing was used. Human
writing acceptance was pending at that earlier checkpoint and is now recorded above. No companion breadcrumbs, merge, tag or release.

## Human review

New isolated package:
C:/Codex/Ultima7 Decomp/test-output/virtue-stage-d-final-dialogue/human-review.
Previous review environments and player saves are preserved. Both BG and Keyring
have their five existing earned checkpoints: not-started, requirements, building,
ready and delivered. The revised executable supplies the new text in all of them;
the save files do not embed dialogue strings. Launch Play BG.cmd / Play Keyring.cmd.
Choose1 for the initial exchange,2 for outstanding requirements,3 for progress,
4 for collection,5 for completed-topic absence. Visit William for `timber delivery`
and `buy timber`. S is native save/load; I opens inventory. C continues the current
review game instead of resetting it. DIALOGUE.txt is the complete editorial script.

Source files changed: native_case_commission.cc, native_case_delivery.inc,
tools/virtue_case/commission_checks.py, tools/virtue_case/transaction_checks.py,
docs/virtue_case_dialogue.txt, docs/virtue_case_editorial_review.md and the index
notice in docs/virtue_case_commission.md. Build products/fixtures remain outside Git.

## Previous candidate runtime evidence (historical)

| Run | Assertions |
|---|---:|
| editorial-dialogue-bg | 32 |
| editorial-transactions-bg | 37 |
| editorial-delivery-bg | 35 |
| editorial-labels-bg | 79 |
| editorial-obstruction-bg | 9 |
| editorial-dialogue-keyring | 32 |
| editorial-transactions-keyring | 37 |
| editorial-delivery-keyring | 36 |
| editorial-labels-keyring | 79 |
| editorial-obstruction-keyring | 9 |

Total: **385 gameplay assertions**, zero failed final runtime runs.
Both copied-review executable smoke runs also passed (one readiness assertion each).
All ten launcher checkpoint restores were verified recursively against their
prepared fixtures, and both profiles were restored to not-started. Existing
private configs, UI/world scales and launcher behavior were retained. The only
pre-runtime issues were a PowerShell here-string parse error before edits ran and
an over-simple source-comparison regex; neither was an engine test failure. The
corrected literal-aware source comparison passed for both production files.

No automatic result constitutes human writing acceptance. The owner has now
approved the full dialogue, subject only to the three final corrections recorded
below. No merge/publication/tag occurred.


## Final owner-authorized corrections and acceptance verification

Only the following three substitutions were made after6044249b1. Full-file
comparison confirms the two production files and authoritative dialogue script
match that baseline plus exactly these substitutions; every other spoken line,
state guard, transition, timing value, save format and animation operation is
unchanged. The approved `Hmm. A fine wooden case...` paragraph remains intact.

A. Introduction, immediately after that paragraph:

> "We must give those stones a proper home! The Guild's work and fittings will cost one hundred gold."

The redundant `Good timber, and soft cloth for the lining.` prefix was removed.

B. `commission case`, exact accepted response:

> "I shall be pleased to make it. Ask William to send the timber here, and bring me the cloth and the Guild's fee. Once we have everything, we shall need two days for the work."

Only `our fee` changed to `the Guild's fee`.

C. Interrupted walk, exact accepted speech:

> I cannot reach the table, Avatar. Give me a little room.

Only `get to` changed to `reach`.

One ordinary executable was rebuilt through the accepted workflow and tested in
ordinary BG and original Keyring profiles; no separate engine/mod build or altered
rules were introduced. The executable contains all three exact corrected strings
and none of the three superseded complete strings. Existing dialogue tests also
verify A and B in the actual paged response and verify C in the loaded executable.
The interrupted-walk bark was not newly forced; existing delivery and obstruction
suites retain their ordinary gameplay paths. Seven incomplete-material combinations
remain checked in real menus. Original effective NPC functions and ordinary menu
choices remain intact. No test infrastructure was added and no assertions weakened.

| Final real-engine run | Assertions |
|---|---:|
| accepted-commission-bg | 35 |
| accepted-transaction-bg | 37 |
| accepted-delivery-bg | 34 |
| accepted-label-bg | 79 |
| accepted-obstruction-bg | 9 |
| accepted-commission-keyring | 35 |
| accepted-transaction-keyring | 37 |
| accepted-delivery-keyring | 35 |
| accepted-label-keyring | 79 |
| accepted-obstruction-keyring | 9 |

Total: **389 passing gameplay assertions, zero failures**, all normal cleanup.
Relative to385: six additional exact-wording assertions (A/B/C in each profile),
minus two repetitions of optional food-use checks in timed delivery (one Iolo food
check in BG and one Avatar food check in Keyring). Every previous named gameplay
check/branch remains covered; nothing was removed. The original32 dialogue checks
per profile remain, plus three final-wording checks each. Transaction counts remain
37 each; labels79 each; obstruction9 each. Delivery counts34 BG/35 Keyring reflect
those two fewer feed repetitions, not changed construction timing or rules.

Evidence: C:/Codex/Ultima7 Decomp/test-output/virtue-stage-d-accepted/runs.
Build log: test-output/virtue-stage-d-final-dialogue/acceptance-build.log.
Existing human-review environments, launchers, earned checkpoints and player saves
were preserved without modification. Only docs/virtue_case_dialogue.txt,
docs/virtue_case_editorial_review.md, docs/virtue_case_commission.md,
native_case_commission.cc, native_case_delivery.inc and the existing
commission_checks.py changed in this finalization. No art, spell/label code,
prices, prerequisite,48-hour deadline, save/load format or studio-table handoff
mechanics changed. No other project/worktree was modified.

Outstanding independent work retains its prior status: sleeping/leave-and-return
checks not newly certified, companion breadcrumbs not started, release/merge/
publication not authorized. No GUI acceptance is inferred from automation.
The human dialogue editorial milestone is complete; no further editorial revision
is required. No Git tag, merge, publication or release was performed.
