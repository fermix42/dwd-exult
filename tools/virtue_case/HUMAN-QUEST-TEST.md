# Virtue Stone Case commissioning review

Private review folder: `C:\Codex\Ultima7 Decomp\test-output\virtue-stage-d\human-review`.
Double-click **Play BG.cmd** or **Play Keyring.cmd**, then choose a checkpoint:

1. **not-started** — Avatar-only Minoc starting fixture; one real Virtue Stone,
   500 gold and ordinary cloth. Xanthia and William retain their original positions
   and schedules. The commission has not been accepted and no case exists.
2. **requirements** — Cloth and the 100-gold Guild fee are already handed over.
   Arrange William's timber; he quotes 20 gold before the separate purchase choice.
3. **building** — All three requirements were completed through actual dialogue.
   Construction has an earned, saved 48-hour deadline. Normal hunger still applies.
4. **ready** — The deadline has elapsed. Talk to Xanthia and select **collect case**.
   Follow her as she walks to the Guild workbench. Ordinary offscreen NPC walking
   stops if the Avatar remains behind. If blocked, clear the route and ask again.
5. **delivered** — The real empty case has been picked up into the worn backpack.
   The quest is permanently complete; its topics must remain absent.

Selecting a numbered checkpoint resets only that disposable review profile's
current game and slot 8. Choose **C** to continue your current review game instead.
Close Exult before changing checkpoints. Your usual game installation, configs and
saves are not used for writes. The previous Stage B/C review folder is unchanged.

In game, **I** opens inventory. Double-click the worn backpack, then double-click
the wooden case normally. **S** opens normal save/load; prepared slot 8 matches the
chosen checkpoint. Do not use F3: it is a cheat binding, not the save dialog.
Cheats and free casting are disabled. No quest state is changed by the launcher.

Start with Xanthia nearby. Showing the company's real stone introduces the case;
**commission case** accepts it. William starts southeast of Xanthia. A successful
transaction disappears immediately and permanently. An unsuccessful attempt
disappears only until the next conversation. Materials and progress topics appear
once per conversation. Cloth and the Guild fee may be supplied in either order.

At collection, watch Xanthia face and reach toward the actual workbench, set down
one case, straighten, step aside and speak. Pick up the case yourself. It must not
appear directly in the backpack. Save/reload, drop the case or move it to another
inventory: none of those actions should reopen the completed commission.

The accepted case art, real stone objects, label editor and spell rules are
unchanged. Insert a real stone before casting through its compartment. MARK and
RECALL still need the ordinary spellbook, reagents and mana. Clicking a parchment
label starts native text entry; Enter commits and Escape cancels.

Automated evidence uses the same ordinary Exult.exe in explicit test CLI mode,
with real menus, movement, object interactions and SDL events. Both launcher
profiles were smoke-tested in that engine against their canonical prepared paths.
No desktop automation was used. Human desktop launch and visual acceptance remain
for the owner. Private Keyring configuration selects these isolated save paths;
the original installed Keyring resources are unchanged.

Please check natural conversation flow, readable choices, physical delivery and
animation, and permanent topic removal. Also check sleeping during construction,
leaving and returning to Minoc, and a completely obstructed workbench; these
specific quest scenarios are not yet certified
by the automated suite. Native Wait, exact 47/48-hour boundaries, pre-placement
interruption/reload, actual placement/pickup, ordinary time, waiting past48 hours,
and dropping the delivered case followed by terminal reload were exercised.

Actual engine screenshots and JSON test evidence are under the sibling `runs`
folder, especially `d3-delivery-bg-final` and `d3-delivery-keyring-01`.
This is a development review package, not a release or human acceptance.
