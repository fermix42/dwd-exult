# Built-in quiz slider defaults

## Final acceptance

- **Status:** COMPLETED — ACCEPTED
- **Acceptance date:** November 1, 2026
- **Implementation commit:** `0e4980813b3713753781fd17ea7a8e71f1658ce3`
- **Branch:** `codex/window-options`
- **Accepted scope:** Built-in correct-answer defaults for all 29 BG/SI quiz questions.
- **Verification:** All previously reported automated differential tests, native
  loading checks, Windows build checks, and supported mod compatibility checks passed.
- **Accepted limitation:** Actual native quiz encounters were NOT TESTED because
  suitable gameplay states were unavailable. This limitation is acknowledged and
  does not block acceptance.
- **Non-Windows builds:** Not tested.
- **Remaining blockers:** None.
- **Further development or testing required:** None.

The user formally accepted this implementation as complete. The verification
details below remain the historical record; untested encounters and platforms do
not represent outstanding work or reopen the accepted scope.

The ordinary Exult executable supplies corrected Usecode for BG Finnigan
(`0x0886`), BG Batlin (`0x0852`), and SI's shared Fedabiblio/Gwanni-child quiz
(`0x098C`). The only gameplay change is the initial numeric selection. Questions,
answers, text (including quotes and SI keyword markup), random selection, ranges,
steps, flags, validation, retries and consequences retain their original behavior.
Players can still change a selection and submit a wrong answer.

## Sources and loading

- `data/quiz_bg.uc`: the two self-contained BG helpers. Each slider reads its
  initial value from the selected question's existing answer-table entry.
- `data/quiz_si.uc`: the shared SI helper, with its 15 initial values taken from
  the corresponding existing answer comparisons.
- `tools/compile_quiz.py`: ordinary UCC compilation, without a symbol table, into
  separate `quiz_bg` and `quiz_si` byte arrays in a generated `quiz_compat.h`.
- `usecode/quiz.cc`, `quiz.h`, and the call in `usecode/ucinternal.cc`: feed these
  arrays to the existing Usecode reader. No numeric-input intrinsic interception,
  runtime bytecode rewriting, text recognition or configuration is involved.

Initialization and reload use this order:

1. Original game `STATIC/USECODE`.
2. Existing embedded game-time support, unchanged.
3. Embedded quiz Usecode for BG or released SI (not the SI beta).
4. Ordinary selected patch/mod Usecode, with its existing override authority.

There is no mod entry, compatibility package, player switch or separate resource
installation. The correction does not occupy `<PATCH>`, replace original game
files, or write into installed mods. Existing original-function chaining is
unchanged; player mods remain free to override any of these functions.
`LINKDEP1`/`LINKDEP2` are neither needed nor generated.

## Ordinary build integration

The VS2019 Exult project automatically builds a host UCC from the existing compiler
sources and regenerates the quiz header before C++ compilation. Sources/compiler
inputs are dependencies; an unchanged subsequent build does not regenerate it.
The host compiler and header stay under the existing intermediate directory's
`quiz` subdirectory. Python 3 and Win Flex/Bison are build dependencies;
`WIN_FLEX_BISON` can identify their directory, otherwise the existing workspace
tool directory or PATH is used. `QuizPython` can select the Python command.
These are developer toolchain settings, not player settings.

The standalone MSVC UCC build uses `tools/build_quiz_ucc.ps1` and
`tools/compiler/msvc_compat.h`. Two necessary compiler portability corrections
define the missing MSVC printf annotation macro and remove `inline` from four
out-of-line virtual definitions. Compiler diagnostic tests and baseline bytecode
comparisons passed (below).

Automake generates `usecode/quiz_compat.h` with `PYTHON_FOR_BUILD` and
`UCC_FOR_BUILD`; the default UCC is the ordinary compiler target. The quiz resource
translation unit belongs to the executable, avoiding a compiler/resource build
cycle through `libusecode`. Cross builds require a runnable build-host
`UCC_FOR_BUILD`. The iOS project includes the translation unit and a dependency
phase generating the header under `DERIVED_FILE_DIR`; its host UCC setting defaults
to `/usr/local/bin/ucc` and is overridable. Automake/iOS builds were not executed
on this Windows host.

The verified ordinary Release/x64 output is
`../build-exult-fork/bin/Exult.exe`; its existing `data` directory matches the
ordinary build's FLX resources. No player configuration or saves were changed.
The feature is embedded in the executable and follows ordinary executable
deployment; no quiz files need to be deployed beside it.

## Supported resource snapshots

Verified on November 1, 2026. The supplied English game resources identify as
BG with Forge of Virtue and SI with Silver Seed. “Vanilla” below means those game
resources without an optional Exult mod.

| Resource | SHA-256 of Usecode |
| --- | --- |
| BG / Forge of Virtue | `49a3fcaf8a763972d0a6006571e8d8eef11eb1602f88898808b520523d0a8576` |
| SI / Silver Seed | `87a86e47767630f6dddb94dd28982a628553b65e4d5cf8014b78ed38607026c4` |
| Keyring 0.11.25 | `e7633dd9c4bc11cc1ab6946dab75138c4d912ed786f22430861ed5f17f22773d` |
| SI Fixes 0.15.15 | `4493b5d434929acaadfd9c66a01fff3279a1e743a4befaa01b4ab5d06c2ade42` |

The compiled Keyring snapshot has 450 functions; SI Fixes has 83. Neither supplies
an affected quiz helper. Their compiled function bodies also match recompilation
of the corresponding checked-in sources, whose readmes supply those version
numbers. No compatibility adjustment or third-party package modification was
necessary. Other releases, languages and mod versions were not validated.

## Question-level results

All 29 defaults passed the compiled-bytecode differential checks. Each was checked
against the original expected answer, range and increment. Correct submissions,
one-step wrong submissions, retry outcomes, question flags and consequences were
compared with the actual original compiled helper, along with complete dialogue
and random-draw traces. These are automated function-level checks, not 29 native
end-to-end encounter playthroughs.

| BG Finnigan question | Default | Range / step | Result |
| --- | ---: | --- | --- |
| Northernmost Spektran latitude | 120 | 0–210 / 5 | PASS |
| Buccaneer's Den longitude | 60 | 0–210 / 5 | PASS |
| Terfin longitude | 120 | 0–210 / 5 | PASS |
| Dagger Isle latitude | 0 | 0–210 / 5 | PASS |
| Skara Brae latitude | 30 | 0–210 / 5 | PASS |
| Deep Forest latitude | 60 | 0–210 / 5 | PASS |
| Buccaneer's Den latitude | 60 | 0–210 / 5 | PASS |
| Skara Brae longitude | 60 | 0–210 / 5 | PASS |

| BG Batlin question | Default | Range / step | Result |
| --- | ---: | --- | --- |
| Ginseng reboilings | 40 | 0–60 / 1 | PASS |
| Archaic runes | 31 | 0–60 / 1 | PASS |
| Natural mandrake locations | 2 | 0–60 / 1 | PASS |
| Bandits in illustration | 6 | 0–60 / 1 | PASS |
| Body parts protected by armour | 6 | 0–60 / 1 | PASS |
| Black pearls per 10,000, fewer than | 1 | 0–60 / 1 | PASS |

| SI shared question | Default | Range / step | Result |
| --- | ---: | --- | --- |
| Ninth Circle spells | 8 | 1–70 / 1 | PASS |
| Armour materials | 4 | 1–70 / 1 | PASS |
| Bestiary beasts | 52 | 1–70 / 1 | PASS |
| Reagent types | 11 | 1–70 / 1 | PASS |
| Mass Death reagents | 6 | 1–70 / 1 | PASS |
| Ginseng boilings | 40 | 1–70 / 1 | PASS |
| Create Automata circle | 6 | 1–70 / 1 | PASS |
| Reasons for Erstam's history | 1 | 1–70 / 1 | PASS |
| Ophidian characters | 36 | 1–70 / 1 | PASS |
| Other Landmarks areas | 4 | 1–70 / 1 | PASS |
| Cities established | 3 | 1–70 / 1 | PASS |
| Forces forming Principles of Balance | 6 | 1–70 / 1 | PASS |
| Monitor clans | 3 | 1–70 / 1 | PASS |
| Weapon types | 4 | 1–70 / 1 | PASS |
| Words of Power | 27 | 1–70 / 1 | PASS |

`tools/quiz/verify.py` executed 9,468 differential cases, covering every ordered
fresh-question selection, correct/wrong/retry submissions, duplicate random draws,
and BG recall using flags from the preceding invocation. Its bounded interpreter
rejects unsupported instructions; it does not alter game state or provide a
replacement game implementation.

## Compiled difference accounting

An additional structural audit recompiles the same UCC sources with the original
initial values restored. Compared with that compiler baseline, the correction
changes exactly one default expression in each BG helper (constant zero becomes
the indexed answer-table lookup) and 15 immediate values in SI. Text/local/external
layouts are identical, and all branch destinations are preserved after relocation.

| Helper | Original body bytes | UCC with original defaults | Corrected UCC | Args / locals / externs |
| --- | ---: | ---: | ---: | --- |
| Finnigan `0x0886` | 1895 | 1880 | 1886 | 0 / 31 / 0 |
| Batlin `0x0852` | 1277 | 1265 | 1271 | 0 / 27 / 0 |
| SI `0x098C` | 1994 | 1986 | 1986 | 0 / 6 / 0 |

Differences from Origin's compiler are text-pool layout/deduplication (two bytes
saved in each BG pool), local slot allocation, loop lowering, boolean/control-flow
lowering and relocated offsets. The dialogue string sets are identical, and the
differential execution compares the ordered dialogue and effects, not raw local
slot numbers. No external function dependencies were added. Original zero-argument
calling conventions and the Fellowship caller itemref are retained.

## Native engine and build checks

The existing semantic CLI in the ordinary executable was used in fresh disposable
profiles, with dummy video and no desktop automation. `inspect_usecode` is a
read-only observation of effective function bodies; numeric slider state exposes
value/minimum/maximum/increment. Existing native key handlers adjust and confirm
sliders. No arbitrary function invocation or quest-flag mutation was added.

| Configuration | Effective quiz bodies | Other checks | Result |
| --- | --- | --- | --- |
| Vanilla BG | Both exact embedded bodies | Rest/bribe helpers unchanged; save/reload | PASS, 8 assertions |
| BG + Keyring | Both exact embedded bodies | All 450 mod bodies intact; rest/bribe; save/reload | PASS, 9 assertions |
| Vanilla SI | Exact embedded shared body | Rest/riddle helpers unchanged; natural opening; Iolo conversation; save/reload | PASS, 8 assertions |
| SI + SI Fixes | Exact embedded shared body | All 83 mod bodies intact; rest/riddle; natural opening; Iolo conversation; save/reload | PASS, 9 assertions |

All four sessions shut down normally with cleanup complete. A separate native
quantity-slider check passed four assertions: original default/range, left/right
adjustment, and cancellation preserving the inventory stack. Resource regeneration
after a source update and timestamp preservation on the following no-op MSBuild
both passed. The ordinary Release/x64 executable built successfully.

The rebuilt UCC passed all 130 existing diagnostic tests. Its bytecode for
`tools/compiler/test1.uc`, Keyring and SI Fixes was identical to output from the
previous available UCC, confirming no compiler code-generation regression.

Full native Finnigan, Batlin, Fedabiblio and Gwanni-child quest encounters were not
played end to end. Wrong-answer/retry/consequence coverage is the exhaustive
function-level comparison described above. Native checks establish resource
resolution, retained mod code, representative gameplay and ordinary slider input.

### Minimal native encounter follow-up

The final bounded gameplay check used the existing semantic CLI and disposable
copies of compatible saves. No testing infrastructure, quest-state manipulation,
teleportation or implementation changes were introduced.

| Corrected helper | Native default / submission | Reason |
| --- | --- | --- |
| Finnigan `0x0886` | NOT TESTED | Loaded the existing vanilla BG “The Beginning” save and spoke to Finnigan normally. He asked whether the stables had been searched; investigation/report prerequisites remain unmet. Completing the investigation chain exceeds this minimal smoke check. |
| Batlin `0x0852` | NOT TESTED | No available compatible save has Fellowship induction/retry progress. The numeric quiz requires the membership quests and induction, rather than Batlin's initial personality examination. |
| SI `0x098C` | NOT TESTED | Completed the natural opening and attempted ordinary starting-area navigation toward the Thoxa encounter. Walking reached `(425,2478,0)`, but further sampled destinations were blocked and no quiz prompt appeared. Available saves do not provide a later ready encounter. |

The save inventory covered 225 existing archives under the local player,
experiment and prior test-output directories. Read-only inspection found no BG
quiz/report progress or SI quiz progress suitable for an immediate encounter.
The Finnigan exploratory log includes one rejected lowercase dialogue choice,
subsequently corrected to the reported `No`; this was an input error, not a quiz
failure. Both exploration sessions returned player control and shut down normally.
The SI session's initial checkpoint load was refused while the opening owned
control; the natural opening was completed instead. These exploratory CLI runs
therefore contain command errors and are not reported as passing quiz tests.
No visible quiz default or successful native quiz submission is claimed.

Final diff review found only quiz implementation, build integration, focused CLI
observations/tests and documentation. Generated headers/bytecode, executables,
logs, screenshots and disposable profiles remain excluded. Original game and
installed mod Usecode hashes still match the resource snapshots above.
`ultima7-modern` was not edited; its pre-existing unrelated changes remain alone.

## Reproducing the checks

Configure the existing CLI environment as in `test_cli.md`, using matching built
FLX resources, and keep `EXULT_TEST_OUTPUT` inside a disposable output directory.
The local evidence from this run is under `test-output/quiz` (ignored by Git).

```text
python tools/quiz/verify.py --ucc <ucc> --bg <BG/STATIC/USECODE> --si <SI/STATIC/USECODE> --mods <mods-directory> --header <quiz_compat.h> --report <existing-output-directory/report.json>
python tools/quiz/integration.py --game bg --header <quiz_compat.h> --run <fresh-name>
python tools/quiz/integration.py --game bg --mod Keyring --header <quiz_compat.h> --run <fresh-name>
python tools/quiz/integration.py --game si --header <quiz_compat.h> --run <fresh-name>
python tools/quiz/integration.py --game si --mod sifixes --header <quiz_compat.h> --run <fresh-name>
python tools/quiz/slider_smoke.py --run <fresh-name>
```

No development or tests modify `ultima7-modern`, original game resources, normal
player profiles, or installed third-party packages.
