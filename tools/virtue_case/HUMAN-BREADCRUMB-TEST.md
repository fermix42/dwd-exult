# Stage E human gameplay review

Review folder: `C:\Codex\Ultima7 Decomp\test-output\virtue-stage-e-final\human-review`.
Close Exult, then double-click **Play BG.cmd** or **Play Keyring.cmd** and choose:

1. **a-iolo**: A pending, Iolo active, no hints spoken. Stand still and watch Iolo.
2. **a-shamino**: A pending, Iolo dismissed, Shamino recruited through original
   conversation. Stand still and watch Shamino.
3. **b-shamino**: B pending after two real MARK casts, Shamino active. A is ineligible.
4. **b-dupre**: B pending after two real MARK casts, Dupre active, Shamino absent.
5. **deferred-recruit**: A pending, no eligible companion in the party. Avatar starts
   near Shamino at his original location. Wait quietly: no hint should occur. Talk
   to Shamino, use his original **join** topic, then **bye**. If asleep, wait normally
   and feed the Avatar when hungry. The hint should be preserved for after joining.
6. **both-pending**: Shamino active, both pending. Watch A, then B at a later quiet
   opportunity, at least five game minutes later. Open inventory or talk to a
   companion to check deferral, then close it and wait. Walking also defers speech.
7. **delivered-silent**: Both hints were pending when Xanthia completed the actual
   tabletop handoff; both are now canceled. The real earned reward is in the
   backpack and Shamino is active. Stand still, save/load, drop or move the case:
   no breadcrumb should return. The commission remains permanently delivered.

The profile restores current gamedat from the selected checkpoint, so a hint may
appear shortly after launch. Restart that checkpoint to see it again. **S** opens
normal save/load; prepared slot **8** is the same state. **I** opens inventory.
A checkpoint reset changes only this disposable profile's current game and slot 8;
choose **C** to continue your current review game. Close Exult before resets.

Check the readability, wrapping and duration of overhead speech, whether it occurs
at natural moments, and the spacing in checkpoint 6. Try opening a gump or starting
normal conversation before the hint is delivered; it should wait. Report any hint
that interrupts an inappropriate sequence. Natural combat/cutscene situations
still need broader human observation; no artificial hostile or cutscene fixture is
provided. Gameplay acceptance is yours; automated assertions do not establish it.

All four lines are HUMAN EDITORIAL ACCEPTED, November 3, 2026. Their wording is
final, including Shamino's original B line. Overhead readability, natural timing and
final integrated gameplay remain PENDING HUMAN GAMEPLAY ACCEPTANCE. Stage D remains
accepted, with no dialogue changes. No merge, release integration or publication.

These are separate replay branches, not consecutive milestones in one game. Item
provisioning and Avatar-only companion/Minoc starts are labeled disposable fixtures
approved by the owner. Threshold pickup, MARK with normal mana/reagents, original
companion recruitment/dismissal, quest handoff, reward pickup and save/load were
performed by the real engine. No NPC positions or quest flags were assigned.
Checkpoint 7's commission and 48-hour deadline came from an earned Stage D archive.
The other branches have not commissioned the case; their empty case/stone and spell
supplies are explicit test inventory. Pending B can remain after its marked stones
leave the party, as approved. See `fixture-provenance.json` for exact source saves.

Cheats/free casting are disabled. Only isolated review configs use the existing
640x400 world/UI and Point scaling. Previous review folders, player configs/saves,
original installed game assets and Keyring resources are preserved. No desktop GUI
verification has been performed by Codex. Launcher configurations and prepared
states are checked with the ordinary engine's explicit semantic CLI.
