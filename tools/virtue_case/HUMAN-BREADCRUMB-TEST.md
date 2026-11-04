# Stage E natural companion review

**PENDING HUMAN EDITORIAL AND GAMEPLAY ACCEPTANCE.** All four short candidates are
unchanged. Stage D and the accepted overhead renderer/duration remain unchanged.

Close Exult and launch **Play BG.cmd** from
`C:\Codex\Ultima7 Decomp\test-output\virtue-stage-e-final\human-review`.
Press **Enter** to select **8: already-organized**. Keyring has the same launcher
menu and is automatically verified. **C** continues your current disposable game.
A numbered selection restores only this review profile; close Exult before resets.
**S** opens normal save/load; prepared slot **8** is the selected checkpoint.

## Main review: 8, already-organized

The party already has several loose stones and real saved MARK destinations.
Shamino is active. The commission is not completed, neither remark has been spoken,
and no case is owned. Close any saved-open inventory windows, then walk normally
or pause near Shamino. Do not move a stone or cast a spell. A natural remark may
follow after the startup/safety gates; another may follow later on the same walk.

There is a delay of at least ten engine seconds after load or newly qualifying
circumstances. Delivery checks occur at ordinary game-minute boundaries, so do
not expect a precise second. Walking is allowed. Combat, conversation, gumps,
spellcasting, sleep, Native Wait, existing overhead speech and other protected
states defer delivery. After a line, the next requires at least five game minutes
and ten engine seconds. Merely loading never produces an instant remark. There
is no random lottery, inventory trigger requirement or 15-second expiration.

## Other review branches

1. **a-iolo**: Existing loose stones; close gumps and explore/pause near Iolo.
   “These stones are filling our packs!”
2. **a-shamino**: Iolo absent; explore/pause near active Shamino.
   “Our packs are filling with stones.”
3. **b-shamino**: One previously marked loose stone, no owned case. Explore/pause.
   “Knowest thou where each stone leads?”
4. **b-dupre**: Same one-marked-stone condition, Dupre active, Shamino absent.
   “Which stone leads where? I hope one leads to an inn!”
5. **deferred-recruit**: A eligible, no participating companion. Wait: silence.
   Recruit Shamino through original **join**/**bye** dialogue, then explore normally.
   Sleep/hunger may require ordinary waiting/feeding. Touch no stone to renew a hint.
6. **both-pending**: Both state-based conditions, no owned case. Explore normally:
   A first, B later at a safe opportunity after the minimum separation. No repeat
   spell or inventory drag is needed. A single update cannot deliver both.
7. **delivered-silent**: Xanthia already performed the real physical handoff. The
   earned case is in the backpack. Explore, save/reload, transfer or drop the case:
   neither canceled hint may return. Permanent quest completion is authoritative.

For an immediate ownership check in state 8, an empty test case was left nearby
when its real stones were extracted. Drag that closed-case sprite into a party
backpack before the next line. This is a labeled disposable fixture, not a new
quest reward. Acquiring any real case into party inventory before a pending line: both hints should stop without falsely being marked spoken.
An alternate nonquest case uses current ownership: dropping it may restore an
undelivered hint when the party still has the qualifying stones. No new permanent
acquisition history exists. Completed commissioning remains permanently silent.

A requires three distinct original colors carried loose through ordinary nested
party inventory. Seated stones do not count. B requires one valid marked real stone;
a handwritten label alone is insufficient. Completed observations never repeat,
including through fallback companions or after save/load. Removing the qualifying
stones prevents stale observations. Merely commissioning/building/ready does not
suppress; ownership or completed delivery does.

## Preservation and limits

All seven original checkpoints, previous contextual overlays, current human saves,
gamedat and configs are preserved. Separate `prepared-natural` overlays adapt B and
both-pending by native extraction/drop of the existing case and real stones; they
never assign MARK destinations or quest flags. The new organized checkpoint uses
that earned branch. Prior Avatar-only starting fixtures remain clearly disposable;
NPC positions/membership were not assigned. Recruitment/dismissal and any test
spell use original gameplay. Existing skip_intro=yes is retained; these saves do
not certify the natural opening and introduce no Petre/Finnigan corrections.

Review reading comfort, character voice, naturalness, timing and unobtrusiveness.
Static screenshots and automation are separate from your acceptance. SDL event
handler label tests are automated; physical keyboard/IME and broader ordinary
adventuring remain human checks. No computer control or desktop automation.
No merge, tag, release, publication or automatic human acceptance.
