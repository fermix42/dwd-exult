# Runtime gameplay automation

The normal Exult executable includes developer automation, inactive unless explicitly
started with `--test-cli`. Build the ordinary Exult target using the existing build
workflow. There is no alternate engine configuration or CLI executable.

`tools/test_cli/driver.py` supplies the JSON Lines client using Python's standard
library. Its scenarios use this same binary and actual game resources. Detailed
commands, observation fields, errors and completion rules are in
[test_cli_protocol.md](test_cli_protocol.md).

## Local configuration

Configure these environment variables for your own installation:

| Variable | Meaning |
| --- | --- |
| EXULT_TEST_EXE | Normally built Exult executable; defaults to exult.exe at the source root. |
| EXULT_TEST_DATA_PATH | Required directory containing matching Exult .flx resources. |
| EXULT_TEST_BG_PATH | Required for BG: game directory containing static/usecode. |
| EXULT_TEST_SI_PATH | Required for SI: game directory containing static/usecode. |
| EXULT_TEST_MODS_PATH | Conventional mod directory containing Keyring.cfg or sifixes.cfg; defaults to the selected game's mods directory. |
| EXULT_TEST_RUNTIME_DIR | Optional directory of runtime DLLs to prepend to child PATH. |
| EXULT_TEST_OUTPUT | Optional artifact root. Defaults to exult-test-cli under the OS temporary directory. Each fresh session creates runs/<name> here. |

No workspace-specific game, mod, resource or build paths are embedded in the client.
Read-only game/mod resources can be outside the run; configuration, gamedat, saves
and screenshots stay inside a disposable run. Windows link/junction/hard-link
redirections are rejected. Isolation guards do not defend against hostile concurrent
filesystem changes by another process. A marker is a safety guard, not authentication.

Run from the source root after configuring your paths:

```powershell
python tools/test_cli/driver.py
python tools/test_cli/driver.py --script my-scenario.jsonl
python tools/test_cli/demonstrate.py bg --run my-bg-1
python tools/test_cli/demonstrate.py bg --mod Keyring --run my-keyring-1
python tools/test_cli/demonstrate.py si --run my-si-1
python tools/test_cli/demonstrate.py si --mod sifixes --run my-sifixes-1
python tools/test_cli/acceptance.py --run my-semantic-1
python tools/test_cli/navigation_checks.py --run my-navigation-1
python tools/test_cli/lifecycle_checks.py --prefix my-lifecycle-1
python tools/test_cli/review_checks.py --run my-path-checks-1
```

Use fresh names. Path checks require permission to create Windows hard-link/junction
fixtures inside the chosen test output directory. These are filesystem isolation
fixtures, not synthetic game-state fixtures. The current client/evidence helpers
are demonstrated on Windows; other engine build manifests are wired, but other
platform toolchains and the client's Windows desktop-evidence helpers still need
separate validation. Do not interpret untested platforms as passing.

## Short JSON example

```json
{"cmd":"init","game":"bg","run":"example-1"}
{"cmd":"inspect_world","radius":12,"limit":20}
{"cmd":"path_query","x":1081,"y":2214,"z":0}
{"cmd":"move_to","x":1081,"y":2214,"z":0}
{"cmd":"movement_status"}
{"cmd":"assert","state":{"video":"dummy","studio_listener_active":0}}
{"cmd":"save","slot":0}
{"cmd":"exit"}
```

A dispatched/handled action is not gameplay completion. Assert actual positions,
UI ownership and other engine state; inspect movement_status until reached, blocked,
interrupted or timed_out. A stopped actor alone does not prove arrival. Polling
wait/run_until is ordinary engine-time progression, not the native gameplay Wait
feature. No synthetic quest/control changes, arbitrary Usecode invocation, separate
pathfinder/combat rules, network server or desktop input injection are provided.

## BG fast-start caveat

The retained scenarios use the existing skip_intro=yes option for BG fast starts.
The accepted engine marks the first scene completed for this option. This bypasses
the natural first-scene conversations; initial Petre/Finnigan wait schedules may
remain while guards continue pacing. Such a fast start is not evidence of natural
BG opening completion. During review, the same binary with only skip_intro=no ran
the actual dialogue: Petre changed to tend_shop and Finnigan walked to the Avatar.
No flags or NPC actions were forced to obtain that result. SI opening scenarios
explicitly exercise genuine dialogue/scene progression.

To test the natural BG opening, prepare an isolated configuration with skip_intro=no
and launch the same binary's test mode against it; preserve the marker/path guards.
Do not edit an existing player's configuration or use a fast-start checkpoint to
claim that the initial story sequence completed naturally.

## Review and usage discipline

Run only tests relevant to a change; retain previous assertion-backed evidence rather
than repeating every game/mod scenario automatically. Read-only semantic inspection
is the default for timers/Usecode, combat/navigation and UI tests. Screenshots are
optional visual evidence, never the source of object identity, collision or choices.
Human testing is for visual/auditory/subjective behavior. The infrastructure is shared
across projects. Native Wait uses the service described in game_time_events.md. Combat/audio
implementation is unchanged.

Normal idle exit reports full engine cleanup. Unsupported nested exit is explicit
cancellation, nonzero and never PASS. Cancel a supported modal through its native
control, wait for gameplay ownership, then exit normally. Zero assertions still
cannot yield a passing gameplay session. Detailed evidence belongs in the configured
output root, never in the source commit.

For Native Wait regression commands, supported compatibility and known limits,
see game_time_events.md and test_cli_protocol.md.
All commands use the ordinary Exult binary; no separate CLI engine exists.
