# LOCUS — Roadmap to MVP v0.1

**Status:** Planning draft — review at human gates  
**Updated:** 2026-11-04  
**Language:** English (all repository content, commits, UI, code, and tests)  
**Target:** Responsive, precisely timed Windows rhythm game with a usable local chart editor; size is not capped  
**Design authority:** [GDD.md](./GDD.md)

> This file is an executable **task backlog**, not implemented functionality. Each checked item must mean code/docs were actually delivered and independently verified. **One task ID = one implementation request = one reviewable change.** Do not pre-implement neighboring IDs.

## Product boundary

The MVP is a 4×4 rhythm grid with a moving marker, two relative actions (Left/Right), quarter-note stepping, **Wrap-only boundaries**, gradually revealed correct route, unconditional No Fail, Perfect/Good/Bad/Miss judgements, local audio playback, and a chart editor with timeline plus synchronized grid. Windows comes first; accounts, hosting, variable grid sizes, Bounce, and elaborate note types are excluded.

**Owner approvals:** Resolve the architecture decisions before production code; review the deliverable after **every stage**, not after every task. **If user intervention is required early, label it HUMAN GATE REQUIRED in large uppercase text immediately.** The final release tag is gated by explicit owner approval.

## Architecture review (before the task list)

### A. Keep the core independent of the screen

```text
                  Chart file + local music
                           |
                     Chart validation
                           |
                   Immutable Chart data
                   /        |         \
            Time mapping  Route      Event schedule
                  |         |              |
             SongClock ---> RouteSampler   |
                  |         |              |
     Input keydown timestamps ------> Judgement
                  |         |              |
                  |         v              v
                  +----> Game presentation / results

     Editor ChartDraft -> same validation -> same RouteSampler
                       -> same audio/clock -> test-play snapshot
```

The editor must **not** implement a parallel movement or timing algorithm. Game rendering must **not** assign judgements. User input must **not** steer the logical path. Chart and route logic should be unit-testable without a window or audio device.

### B. One authoritative musical timeline

A frame clock is not a music clock. We need one time model for audio playback, key-down timestamps, fractional grid positions, future-path reveal, judgement windows, editor scrub, pause, and resume. Use double-precision domain time and an explicitly synchronized audio/monotonic-clock adapter; do not assume that a library's exposed playback time is automatically sample-accurate. Validate its observable behavior with a metronome spike before selecting the stack.

**Critical design contracts to settle first:** beat-zero convention, chart audio offset sign, turn-then-step ordering at a node, handling of an event at beat zero, song-end behavior, key-down arbitration, and what a paused/seeked session may judge.

### C. Treat Wrap as topology, not an across-screen line

The 4×4 grid is a torus for movement: for example, (3, 1) stepping right becomes (0, 1), with the same heading. Such a step still consumes one quarter note and does not create an input event. The visual renderer must not interpolate a long line between opposite grid points through the field. It needs a seam-aware sample, producing brief exit/re-entry pieces, both for marker animation and trajectory preview.

### D. Keep chart data small and deterministic

Use versioned local chart data with a fixed 4×4 size, an initial point and heading, BPM/offset, preview lead, local audio reference, and an ordered sequence of (beat, Left/Right). Straight movement is implicit. No boundary-mode property is needed for MVP because Wrap is fixed. Do not store a separate list of every marker position: derive it from the chart.

Separate **Chart** (validated, read-only during play) from **ChartDraft** (editable and possibly invalid in the editor). Save and reopen must preserve route semantics. Resolve audio paths from the chart location; never upload user music.

### E. Prioritize low latency; constrain code and UI complexity

Prefer a coherent C++ implementation with a pure core and dependencies justified by **input/audio timing, frame pacing, and correctness first**. Do **not** reject a stack for having a larger executable: even tens or hundreds of megabytes are acceptable when justified. Keep **project complexity and feature scope** under control, not the file size at all costs. **raylib is a candidate, not a committed choice**; use a short technical probe and move to SDL3 only if evidence warrants the extra low-level work. A grid is simple to draw, but a usable editor timeline, text fields, file picker, focus management, and undo are not: pick the UI technique during the stack decision instead of casually creating an ad-hoc framework later.

A suggested layout, to be adjusted once the stack is approved:

```text
mvp/GDD.md
mvp/ROADMAP.md
mvp/architecture/
src/core/          # chart, time conversion, route, judgement
src/platform/      # audio backend, local filesystem, input
src/app/           # screen transitions, session lifecycle
src/ui/            # game, editor, shared basic widgets
tests/             # unit/integration tests and small fixtures
assets/            # only original or permissively licensed assets
```

These are ownership boundaries, **not** a requirement for a generic engine, entity-component system, dependency-injection framework, plugin architecture, backend, or huge class hierarchy. Maintain fewer, cohesive source files until a real split is justified.

### F. Main technical risks (and how tasks address them)

| Risk | Required mitigation |
| --- | --- |
| Input/audio timing drift and visual frame dependence | A-07, T-06–T-08, T-17, Q-12 |
| Wrong route around edges or jumps through the grid | R-02, R-06–R-08, P-04, P-06 |
| Preview ambiguity where paths overlap | R-11, P-08 and owner playtest |
| Editor route differs from gameplay | E-14, I-17–I-19 |
| Uncontrolled scope/dependency growth (not raw executable size) | A-09, F-09, Q-11, Q-13 |
| Corrupt charts, lost edits, or broken relative audio paths | C-04–C-12, I-10–I-13 |

### G. Definition of a single task

Each checkbox below is intentionally a **bounded change with a concrete check**. A request should ask for **only one ID**. Implement the smallest coherent patch, add focused tests in the same change where relevant, run applicable checks, then report changed files, verification, known limitations, and the next blocked/unblocked task. Do not silently implement future IDs. Do not mark a checkbox complete based on planning or untested code. If a task reveals missing prerequisites, document them instead of expanding scope without consent.

**Conditional task:** A-08 is performed only when A-07 has a material latency, timing, reliability, or maintainability blocker. Package size alone is not a blocker. Mark it *Skipped with reason* rather than falsely completed if unnecessary.


## Human intervention escalation (mandatory)

**HUMAN GATE REQUIRED** must be an explicit, prominent, **uppercase heading in the assistant's user-facing reply** whenever any task unexpectedly requires the owner before its scheduled stage-end gate. Do this **as soon as** the blocking need is discovered, not after the assistant has advanced unrelated tasks. Explain the reason, give the smallest concrete action/checklist, link a runnable test where possible, and say which task is blocked. Do not mark that task complete without its required evidence or approval.

Examples: a Windows machine with an audio device is needed to validate A-07; input latency can only be calibrated with physical hardware; a creative design choice cannot be safely inferred; distribution/license permission is required. Provide logs/results easy to share. The scheduled human gate **after every stage** is still mandatory.

## Stage overview

| Stage | Focus | Atomic tasks | Gate |
| --- | --- | ---: | --- |
| 1 | Architecture contracts and stack decision | 9 | Approval required before production implementation |
| 2 | Windows foundation and build hygiene | 9 | Approval required before next stage |
| 3 | Chart domain and local persistence | 12 | Approval required before next stage |
| 4 | Deterministic route and Wrap geometry | 11 | Approval required before next stage |
| 5 | Audio clock, input timestamps, and judgement | 18 | Approval required before next stage |
| 6 | Playable game screen and readable motion | 15 | Approval required before next stage |
| 7 | Editor shell and chart parameters | 14 | Approval required before next stage |
| 8 | Editor event authoring and round-trip workflow | 19 | Approval required before next stage |
| 9 | Windows MVP hardening and release | 17 | Approval required before next stage |

**Total planned atomic tasks: 124**, including the conditional A-08 and the post-approval release-tag task Q-17. Gate reviews are additional owner decisions, not hidden implementation work.

---

## Stage 1: Architecture contracts and stack decision

**Objective:** Approve the minimum architecture before introducing production dependencies.

- [x] **A-01 — Define module ownership**  
  **Depends on:** None.  
  **Change:** Create `mvp/architecture/BOUNDARIES.md` with Chart, MusicClock/Audio, Route, Judgement, App UI, and Editor responsibilities plus allowed dependency directions.  
  **Done when:** The document includes one data-flow diagram and explicitly forbids renderer-driven timing and editor-specific route logic.

- [x] **A-02 — Freeze beat/event semantics**  
  **Depends on:** A-01.  
  **Change:** Write `mvp/architecture/TIMING.md`: define beat zero, offset sign, quarter-note stepping, the exact turn-at-node ordering, and song end behavior.  
  **Done when:** Worked examples at BPM 120 cover beats 0, 1, and 4, and a turn on the outermost grid point.

- [x] **A-03 — Freeze input arbitration policy**  
  **Depends on:** A-02.  
  **Change:** Document what qualifies as a key-down, simultaneous Left/Right, first-hit-wins, out-of-window presses, and pause/seek handling in `TIMING.md`.  
  **Done when:** For every ambiguous case, the document gives one deterministic expected judgement; numerical window sizes remain tunable.

- [x] **A-04 — Specify visual Wrap seams**  
  **Depends on:** A-02.  
  **Change:** Add annotated route and wrap examples to `mvp/architecture/WRAP.md`, covering horizontal/vertical exits and turn-on-edge cases.  
  **Done when:** None of the examples draws a segment across the grid interior; marker progress over a seam is unambiguous.

- [x] **A-05 — Define the first chart schema**  
  **Depends on:** A-02.  
  **Change:** Write `mvp/architecture/CHART_FORMAT.md` with a small versioned JSON example and fields for audio path, BPM, offset, start point/heading, preview lead, and timed Left/Right events.  
  **Done when:** One minimal chart and one invalid chart are shown; no Bounce field, variable grid, or speculative event plugin model is included.

- [x] **A-06 — Draw editor and game screen flows**  
  **Depends on:** A-01.  
  **Change:** Write `mvp/architecture/UI_FLOW.md` with a compact editor wireframe (timeline + 4×4 preview) and screen transitions for play, pause, results, and editor test-play.  
  **Done when:** Every essential action has an identified UI location; no online/account screen appears.

- [x] **A-07 — Spike raylib audio time and packaging**  
  **Depends on:** A-02.  
  **Change:** Create an isolated throwaway raylib probe that plays a local metronome, shows reported song time and key-down time, supports pause/seek, and records executable/dependency size.  
  **Done when:** Build and run on Windows; save observations, limitations, and size numbers in `mvp/architecture/STACK_EVALUATION.md`. Do not use the probe as the game architecture.
  **Status (COMPLETE — 2026-11-04):** Windows x64 build and headless WAV self-test passed; Release EXE measured 979,968 bytes with zero adjacent DLLs. The owner ran the BAT-driven interactive probe on their Windows PC and confirmed everything worked. **Owner explicitly declined to upload a ZIP; this is accepted for A-07's qualitative functional gate.** No input/audio latency in milliseconds was measured. Keep the working **`build_and_run.bat`** in the project. See `mvp/architecture/STACK_EVALUATION.md` for evidence and limitations.

- [ ] **A-08 — Probe SDL3 only if raylib fails the timing gate (conditional)**  
  **Depends on:** A-07.  
  **Change:** If A-07 exposes a material timing, audio-output, event-capture, or stability blocker, create an isolated SDL3 audio/input probe comparing the same timing behavior; report package size as secondary context.  
  **Done when:** Comparable measurements are documented; skip explicitly if raylib meets the agreed criteria.
  **Status (SKIPPED — 2026-11-04):** Conditional task not triggered. The owner reported successful raylib local operation; no material functional blocker has been observed. No SDL3 measurements or implementation were performed. **Not counted as a completed task.** Reopen only after a specific, measured latency/audio/input/stability blocker emerges.

- [x] **A-09 — Choose and record the production stack**  
  **Depends on:** A-07.  
  **Change:** Create `mvp/architecture/STACK_DECISION.md` selecting rendering/audio/input approaches, UI approach, pinned dependencies, and reasons prioritizing verified timing/latency and correctness over binary size, based on the spikes (and A-08 if triggered).  
  **Done when:** Owner can approve one implementation path; temporary experiments are clearly separate from production code.
  **Status (DOCUMENT COMPLETE — 2026-11-04):** See `mvp/architecture/STACK_DECISION.md`: C++17, raylib 6.0, raygui 5.0, native Win32 dialogs, nlohmann/json v3.12.0, CMake/CTest. **HUMAN GATE 1 STILL REQUIRES OWNER APPROVAL** before F-01. These selections do not certify measured end-to-end latency.

### Human gate 1 — APPROVED (2026-11-04)

The owner explicitly approved the A-01–A-06 architecture contracts and A-09 technology stack, authorizing Stage 2 production Foundation. The earlier A-07 audio test was accepted qualitatively without a ZIP; numerical end-to-end latency remains unmeasured and is addressed in the appropriate later timing tasks. **No further Stage 1 approval is required.**

---

## Stage 2: Windows foundation and build hygiene

**Objective:** Create an executable and testable shell without introducing gameplay logic.

- [x] **F-01 — Create the root CMake project**  
  **Depends on:** A-09.  
  **Change:** Add `CMakeLists.txt` with a C++ application target, one core library target, and an isolated tests option.  
  **Done when:** A clean configure produces Windows build files and a minimal executable without manual IDE setup.
  **Verified (2026-11-04):** Root `CMakeLists.txt` builds `locus_core` and the `locus` console stub as separate targets; the optional tests switch is wired but no test runner is created until F-04. Clean Windows VS2022 x64 Release configure/build/run passed in [CI run 41894135183](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41894135183). The A-07 experiment and `build_and_run.bat` remain intact. No game window is claimed at F-01.

- [x] **F-02 — Pin the chosen external dependencies**  
  **Depends on:** F-01.  
  **Change:** Add the smallest reproducible dependency acquisition/configuration mechanism defined by A-09.  
  **Done when:** Clean builds use stated versions; no machine-specific installation paths are required.
  **Verified (2026-11-04):** `cmake/dependencies.cmake` pins raylib 6.0, raygui 5.0, and nlohmann/json v3.12.0 by exact upstream commit SHA. Root CMake uses FetchContent and the required CMake 3.25 minimum. A headless `locus_dependencies_smoke` successfully built and reported all three versions on Windows MSVC Release in [CI run 41895581746](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41895581746); no installed per-machine SDK paths are necessary beyond the standard Windows build toolchain. The existing `build_and_run.bat` remains unchanged. A real CTest suite belongs to F-04.

- [x] **F-03 — Add repository hygiene files**  
  **Depends on:** F-01.  
  **Change:** Add `.gitignore` for generated builds, local audio, editor temp files, and system artifacts; add a minimal `.clang-format`.  
  **Done when:** Tracked sources remain untouched by a clean build, and formatting is deterministic.
  **Verified (2026-11-04):** Expanded `.gitignore` for generated CMake/IDE files, local audio/charts, editor autosaves and diagnostics without blanket-ignoring authored fixtures; added root `.clang-format` (LLVM-based, C++17, four spaces). [Windows CI run 41897345354](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41897345354) passed `git check-ignore` checks, a two-pass clang-format idempotence check, a clean post-build `git status --porcelain`, production build/smoke checks, and the A-07 BAT probe. `build_and_run.bat` was retained unchanged; the owner authorizes future updates when useful.

- [x] **F-04 — Add a CTest-compatible test executable**  
  **Depends on:** F-01.  
  **Change:** Create a tiny core test runner and register it with CTest.  
  **Done when:** One passing placeholder test is reported by `ctest` on Windows.
  **Verified (2026-11-04):** Added a core-only `locus_core_tests` executable and registered `locus_core_foundation` with CTest behind `LOCUS_BUILD_TESTS=ON`; test code has no raylib, Win32, or audio dependency. [Windows CI run 41899304598](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41899304598) reported **1/1 test passed**, including a fully green production build, dependency smoke, A-07 probe/BAT checks, and a clean Git worktree. No gameplay tests are claimed at F-04; these arrive with their respective domain tasks.

- [x] **F-05 — Open and close a blank game window**  
  **Depends on:** F-02.  
  **Change:** Implement application initialization and a clean shutdown path using the selected library.  
  **Done when:** Window opens, remains responsive, and exits with no resource errors.
  **Status (2026-11-04 — HUMAN GATE REQUIRED EARLY):** The production `locus.exe` implements a 960×540 dark raylib window, Esc/X exit, and a 60-frame CI smoke mode. Windows CI [run 41902352666](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41902352666) **passed** the production compilation, core CTest, and both BAT (`test`/`build`) modes. Its hosted runner reported `WINDOW_INIT_FAILED` for graphical initialization and saved `F05_GUI_UNAVAILABLE_ON_HOSTED_RUNNER`; this is **not** a passing real-window test. With owner authorization, `build_and_run.bat` now defaults to building/launching the production window, preserving the A-07 experiment under `audio` mode. **Owner confirmed on 2026-11-04:** The blank window opened and closed normally on their Windows PC; the early F-05 human gate is **satisfied**, with no ZIP needed. The hosted-runner GUI limitation remains documented as a CI environment constraint, not a local failure.

- [x] **F-06 — Separate update and render entry points**  
  **Depends on:** F-05.  
  **Change:** Introduce a single application loop with explicit input polling, state update, and render calls.  
  **Done when:** The program runs the three phases in a documented order without embedding gameplay logic in drawing.
  **Verified (2026-11-04):** `src/app/main.cpp` now runs `PollInput()` → `UpdateState()` → (stop if requested) → `RenderFrame()` in one raylib loop. Input is observed once; update advances only minimal app state; rendering draws without applying gameplay rules. `locus.exe --loop-check` verifies the 60-frame update/stop limit and immediate close without a window. [Windows CI run 41904030990](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41904030990) passed this headless check (`LOOP_STATE_PASS frames=60`), CTest, production build, BAT test/build, and clean-worktree check. The owner already confirmed F-05's real window operation; the hosted runner has no interactive GUI.

- [x] **F-07 — Introduce a minimal screen state switch**  
  **Depends on:** F-06.  
  **Change:** Add an enum/state controller supporting placeholder Home, Play, Editor, and Results states.  
  **Done when:** A debug-only action can switch placeholder screens without recreating the window.
  **Verified (2026-11-04):** `src/app/main.cpp` defines one `Screen` enum for Home/Play/Editor/Results and stores the selected screen in `AppState`; `PollInput()` maps F1–F4 only with the explicit `--debug-screens` switch, `UpdateState()` applies transitions, and `RenderFrame()` draws the current placeholder without recreating the raylib window or modifying state. The root `build_and_run.bat` opts into debug screens for local review, preserving its A-07 `audio` mode. The headless `--screen-check` covers four transitions, no-input persistence, and close precedence. [Windows CI run 41907242084](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41907242084) passed, reporting `SCREEN_STATE_PASS transitions=4`, core CTest, existing build/BAT checks, and clean worktree. The CI runner cannot visually inspect GUI rendering; the human Stage 2 gate remains after F-08.

- [x] **F-08 — Create Windows CI build and test workflow**  
  **Depends on:** F-04.  
  **Change:** Add a GitHub Actions workflow that configures CMake, builds the application and test target, and runs CTest.  
  **Done when:** CI passes on a clean Windows runner without private credentials.
  **Verified (2026-11-04):** Added `.github/workflows/windows_ci.yml`, a separate production workflow running on a clean `windows-2022` worker with public GitHub source dependencies and `contents: read` only. It configures CMake, builds Windows x64 Release application and test targets, runs all CTest cases, and checks headless foundation, loop, four-screen transitions, and dependency integration. [Windows CI run 41909550297](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41909550297) **passed** (`1/1` CTest, `LOOP_STATE_PASS`, `SCREEN_STATE_PASS`, `DEPENDENCY_SMOKE_PASS`); build remained Git-clean. CI uploaded `locus-windows-foundation-x64`. Hosted CI does not validate visible text or physical keyboard response; the owner has already reported F-07 label switching works. The owner also requested improved font legibility, so `src/app/main.cpp` now loads installed Windows Segoe UI/Arial with a raylib fallback, larger text and explicit cleanup; review the appearance at the scheduled Stage 2 gate after F-09.

- [ ] **F-09 — Record blank-app baseline metrics**  
  **Depends on:** F-05.  
  **Change:** Document cold startup method, release executable size, and idle memory/CPU observation in `mvp/BASELINES.md`.  
  **Done when:** The numbers, Windows machine conditions, and measurement commands are reproducible; no arbitrary size target is invented yet.
  **Status (IN PROGRESS — 2026-11-04):** Added `tools/measure-baseline.ps1`, `mvp/BASELINES.md`, and an owner-friendly `build_and_run.bat baseline` mode. [Dedicated Windows CI run 41911357263](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41911357263) passed and documented the production Release EXE (650,752 bytes), first *headless process launch-to-exit* (27.50 ms), median of five warm headless runs (7.88 ms), OS build/CPU/RAM and source/executable hashes. These are **not** true GUI startup or measured I/O latency. The hosted runner has no interactive desktop; genuine idle **GUI working set and CPU observations remain unmeasured**. **HUMAN GATE REQUIRED (F-09):** owner runs `build_and_run.bat baseline` on their Windows PC, leaves the window idle 12 seconds, closes with Esc/X, and reports key numbers plus font readability. No ZIP is mandatory. **Leave F-09 unchecked until the actual GUI metrics are recorded.**

### Human gate 2 — OWNER APPROVAL PENDING

**HUMAN GATE REQUIRED:** After F-09 local GUI idle measurements are recorded, owner reviews the now more readable placeholder text on Windows, confirms clean shutdown and the already green Windows CI, and **explicitly approves Stage 2** before C-01. The owner already verified F-05 window lifecycle and F-07 placeholder-key switching; do not ask them to repeat those earlier checks solely to validate the old stages. No ZIP is required if the F-09 text metrics can be shared. Do not begin Stage 3 before approval.

---

## Stage 3: Chart domain and local persistence

**Objective:** Represent a chart as validated, versioned data without requiring graphics or audio playback.

- [ ] **C-01 — Create core chart value types**  
  **Depends on:** F-04, A-05.  
  **Change:** Define `Chart`, `TurnEvent`, `Direction`, `Heading`, and fixed 4×4 start coordinates in the pure core.  
  **Done when:** Data types compile without the renderer or audio library.

- [ ] **C-02 — Implement beat-to-seconds conversion**  
  **Depends on:** C-01, A-02.  
  **Change:** Implement a pure double-precision conversion between integer beat boundaries and song seconds using BPM and signed offset.  
  **Done when:** Unit tests cover BPM 60/120/180, zero offset, and positive/negative offsets.

- [ ] **C-03 — Parse chart JSON into core values**  
  **Depends on:** C-01.  
  **Change:** Implement a JSON decoder for the agreed v0.1 chart fields, with no file I/O inside the parser.  
  **Done when:** A valid in-memory JSON string decodes into the expected chart values.

- [ ] **C-04 — Validate chart metadata**  
  **Depends on:** C-03.  
  **Change:** Reject unsupported versions, invalid BPM/offset, invalid start cell or heading, and unsupported grid dimensions.  
  **Done when:** Separate tests for each invalid metadata field return field-specific messages.

- [ ] **C-05 — Validate turn events**  
  **Depends on:** C-04, A-03.  
  **Change:** Reject unknown actions, duplicate beats, unsorted events, invalid timing positions, and ambiguity under the agreed timing contract.  
  **Done when:** One valid straight chart and one valid turning chart pass; each bad event type fails with a precise error.

- [ ] **C-06 — Serialize core chart values to JSON**  
  **Depends on:** C-01.  
  **Change:** Implement stable, readable JSON output with schema version and defined field ordering.  
  **Done when:** Serialization output is valid JSON and preserves numeric precision needed by the chart contract.

- [ ] **C-07 — Load charts from the local filesystem**  
  **Depends on:** C-03.  
  **Change:** Add a small filesystem adapter to read `.json` chart files and forward content to the parser.  
  **Done when:** An existing chart opens; nonexistent file and invalid UTF-8/corrupt file give non-crashing errors.

- [ ] **C-08 — Save charts atomically to disk**  
  **Depends on:** C-06.  
  **Change:** Implement write-to-temporary-file then rename/replace for chart saving, using a safe failure path.  
  **Done when:** A successful save produces valid JSON; simulated write failure does not truncate the last valid chart.

- [ ] **C-09 — Resolve chart-relative audio paths**  
  **Depends on:** C-07.  
  **Change:** Implement local audio path resolution relative to the chart directory and reject accidental unsafe absolute-path rewriting.  
  **Done when:** Moving a chart folder with its referenced audio preserves loading; missing audio shows a clear path error.

- [ ] **C-10 — Add two reference chart fixtures**  
  **Depends on:** C-05.  
  **Change:** Commit a zero-turn Wrap loop fixture and a four-turn edge-crossing fixture under `tests/fixtures/`.  
  **Done when:** Each fixture parses and validates and documents expected starting heading and turn beats.

- [ ] **C-11 — Add parser/serializer round-trip tests**  
  **Depends on:** C-06, C-10.  
  **Change:** Test parse → serialize → parse for fixtures, including version and signed offset.  
  **Done when:** The second parse exactly matches relevant first-pass chart fields; no loss of turn positions.

- [ ] **C-12 — Fail clearly on unknown future schema versions**  
  **Depends on:** C-04.  
  **Change:** Add explicit error handling for file versions newer than the supported v0.1 schema; never guess compatibility.  
  **Done when:** A fixture with an unsupported version cannot start gameplay and returns an actionable diagnostic.

### Human gate 3

**Do not start the next stage until approved.** Owner inspects an English chart fixture, saves and reopens it, and reviews validator error messages.

---

## Stage 4: Deterministic route and Wrap geometry

**Objective:** Make every future and past marker position calculable without rendering or key presses.

- [ ] **R-01 — Implement relative heading rotation**  
  **Depends on:** C-01.  
  **Change:** Add pure Left/Right quarter-turn operations on the four cardinal headings.  
  **Done when:** All eight heading/action combinations pass unit tests.

- [ ] **R-02 — Implement modulo-4 Wrap stepping**  
  **Depends on:** R-01.  
  **Change:** Add a pure next-cell step that wraps on every edge while preserving the heading.  
  **Done when:** Tests cover entering from all four sides, including repeated crossings.

- [ ] **R-03 — Apply authored turns before stepping**  
  **Depends on:** R-01, C-05.  
  **Change:** Implement the beat-boundary state transition using the ordering approved in A-02.  
  **Done when:** A turn on an edge chooses the expected outgoing axis before Wrap is applied.

- [ ] **R-04 — Calculate discrete route states**  
  **Depends on:** R-02, R-03.  
  **Change:** Given a chart and beat index, compute the marker cell/heading deterministically, without reading input.  
  **Done when:** A fixture produces the same ordered states on every run.

- [ ] **R-05 — Sample fractional-beat marker progress**  
  **Depends on:** R-04.  
  **Change:** Provide a route-sampling operation for arbitrary nonnegative beat times, returning source, destination, and 0–1 step progress.  
  **Done when:** At beats N, N+0.5, N+1 the sample is stable and independent of FPS.

- [ ] **R-06 — Label Wrap versus ordinary segments**  
  **Depends on:** R-05.  
  **Change:** Extend route samples to indicate whether the current step crosses a grid seam and which edges are involved.  
  **Done when:** A wrap from x=3 to x=0 is flagged; moving x=1 to x=2 is not.

- [ ] **R-07 — Build preview segment samples**  
  **Depends on:** R-05.  
  **Change:** Generate a bounded sequence of route segments from beat t to beat t+lead, clipping first/last segments by fractional progress.  
  **Done when:** Tests at leads 0.5, 2, and 4 beats return correct segment counts and endpoints.

- [ ] **R-08 — Split visual geometry at Wrap seams**  
  **Depends on:** R-06, R-07, A-04.  
  **Change:** Convert a Wrap segment into exit/re-entry drawable pieces without a center-crossing line.  
  **Done when:** Geometry tests demonstrate both horizontal and vertical edge crossings remain outside the grid interior between opposite endpoints.

- [ ] **R-09 — Test route equality despite different inputs**  
  **Depends on:** R-04.  
  **Change:** Add a regression scenario in which correct, wrong, and missing key sequences all reference the same authored chart.  
  **Done when:** The resulting route-state sequence is byte-for-byte identical, independent of player actions.

- [ ] **R-10 — Test seek and replay determinism**  
  **Depends on:** R-05.  
  **Change:** Sample beats in non-monotonic order and restart the same fixture multiple times.  
  **Done when:** Each beat always yields the same position, heading, and Wrap status.

- [ ] **R-11 — Test repeated and crossing preview paths**  
  **Depends on:** R-07.  
  **Change:** Add fixtures where the route revisits a grid cell and crosses another segment while the look-ahead is visible.  
  **Done when:** Segment order and geometry remain deterministic; no omitted or accidentally connected discontinuities.

### Human gate 4

**Do not start the next stage until approved.** Owner checks sample routes including all edges, turns at edges, and seek/replay determinism.

---

## Stage 5: Audio clock, input timestamps, and judgement

**Objective:** Deliver a measurable timing core that remains correct when frames are skipped or the song is paused.

- [ ] **T-01 — Generate a rights-clean timing test WAV**  
  **Depends on:** F-01.  
  **Change:** Add a small generated metronome fixture (or deterministic generator) to `tests/fixtures/`, with known tick timestamps.  
  **Done when:** The asset is original, distributable, and its beat timing is documented.

- [ ] **T-02 — Load local audio into the selected backend**  
  **Depends on:** F-02, T-01.  
  **Change:** Implement audio file loading for the MVP's explicitly supported formats (start with WAV; add MP3 when verified by the chosen backend).  
  **Done when:** A valid test WAV opens; an unsupported or corrupt file fails gracefully.

- [ ] **T-03 — Implement play and stop commands**  
  **Depends on:** T-02.  
  **Change:** Add playback start/stop lifecycle to the audio adapter and release resources correctly.  
  **Done when:** Repeated play/stop cycles do not leak handles or trigger double playback.

- [ ] **T-04 — Implement pause and resume**  
  **Depends on:** T-03.  
  **Change:** Add pausing without losing logical song position and resuming without a new chart start.  
  **Done when:** Paused time does not advance; resume continues from the previous position.

- [ ] **T-05 — Implement audio seek**  
  **Depends on:** T-04.  
  **Change:** Support seek to a valid music position and explicitly clamp/reject invalid positions.  
  **Done when:** Seeking to beginning, middle, and near end reaches consistent playback state.

- [ ] **T-06 — Measure backend time drift and stability**  
  **Depends on:** T-03.  
  **Change:** Instrument reported playback position against a monotonic reference while playing the test fixture, including long sessions and pauses.  
  **Done when:** Logs include drift/jitter observations; failures are documented before relying on a nominal audio-time getter.

- [ ] **T-07 — Implement a single SongClock adapter**  
  **Depends on:** T-05, T-06, C-02.  
  **Change:** Expose song time through one interface driven by the selected audio backend and a monotonic time anchor, avoiding frame-time accumulation.  
  **Done when:** Update and render can read the same effective song timestamp; diagnostics demonstrate continuous progress and no reset on pause.

- [ ] **T-08 — Apply chart BPM and offset to SongClock**  
  **Depends on:** T-07, C-02.  
  **Change:** Translate the current song time into chart beat position using the approved offset sign and BPM.  
  **Done when:** At BPM 120, beat N aligns with the documented track instant, including offset changes.

- [ ] **T-09 — Timestamp Left/Right key-down edges**  
  **Depends on:** T-07.  
  **Change:** Create an input adapter producing monotonic timestamps for fresh Left/Right presses, not held-key repeats.  
  **Done when:** Diagnostic logs distinguish key-down from hold; each press can be mapped to song time.

- [ ] **T-10 — Bind two default turn keys**  
  **Depends on:** T-09.  
  **Change:** Choose two non-conflicting provisional keyboard bindings and route them to Left and Right actions.  
  **Done when:** Pressing either key emits exactly the designated action; other keys do not emit a turn.

- [ ] **T-11 — Introduce judgement window configuration**  
  **Depends on:** C-05, A-03.  
  **Change:** Represent four judgement outcomes via ordered Perfect/Good/Bad outer thresholds, preserving one numerical policy for a play session.  
  **Done when:** Threshold validation rejects negative, reversed, or overlapping-inconsistent values; defaults are labeled provisional.

- [ ] **T-12 — Judge one correct key-down**  
  **Depends on:** T-09, T-11.  
  **Change:** Implement evaluation of a timestamped correct action against one event's scheduled time.  
  **Done when:** Tests at exact center and both sides of all three positive windows produce the expected grade.

- [ ] **T-13 — Judge wrong action as immediate Miss**  
  **Depends on:** T-12.  
  **Change:** Resolve an active turn event to Miss when its first eligible key-down is the wrong relative action.  
  **Done when:** A later correct press cannot improve that event; route movement remains unaffected.

- [ ] **T-14 — Expire untouched windows to Miss**  
  **Depends on:** T-12.  
  **Change:** At the end of each judgement window, resolve unresolved events to Miss exactly once.  
  **Done when:** No-input playthrough yields one Miss per turn event and no duplicate results.

- [ ] **T-15 — Handle off-window and repeated presses**  
  **Depends on:** T-13, T-14, A-03.  
  **Change:** Implement the approved policy for presses outside windows, held keys, and repeated presses after an event resolves.  
  **Done when:** Automated cases match the approved arbitration table without issuing duplicate judgements.

- [ ] **T-16 — Aggregate judgements into a result model**  
  **Depends on:** T-15.  
  **Change:** Record counts by grade, hit timing deviations, and current/max combo in a pure results object.  
  **Done when:** Counts sum to attempted events; combo behavior on Miss matches the approved rule.

- [ ] **T-17 — Prove frame-rate-independent judgement**  
  **Depends on:** T-16.  
  **Change:** Feed identical timestamped inputs while simulating 30, 60, 144 FPS and dropped render frames.  
  **Done when:** All input grades and aggregate results are identical across simulated frame schedules.

- [ ] **T-18 — Reset scoring correctly on pause/seek/retry**  
  **Depends on:** T-05, T-16.  
  **Change:** Implement explicit session reset and editor-preview seek semantics so stale judgements cannot survive a restart or scrub.  
  **Done when:** Pause never grades future notes; restart clears scores; seeking in editor does not create gameplay Misses.

### Human gate 5

**Do not start the next stage until approved.** Owner taps to a test track, reviews timing grades, and checks pause/seek/retry behavior.

---

## Stage 6: Playable game screen and readable motion

**Objective:** Turn the proven headless route/timing model into the first complete playable experience.

- [ ] **P-01 — Draw an evenly spaced 4×4 grid**  
  **Depends on:** F-05.  
  **Change:** Render exactly 16 point markers centered responsively in the game view.  
  **Done when:** All points remain visible and evenly spaced at two tested Windows sizes.

- [ ] **P-02 — Draw the player marker**  
  **Depends on:** P-01.  
  **Change:** Add one high-contrast marker at a sampled grid coordinate, with distinct pastel styling.  
  **Done when:** Marker can be visually distinguished from all 16 grid points.

- [ ] **P-03 — Interpolate ordinary marker steps**  
  **Depends on:** P-02, R-05, T-08.  
  **Change:** Drive marker position by fractional beat from Route sampling, not delta-time integration.  
  **Done when:** Movement from point A to adjacent B is smooth; tests with dropped render frames show correct position.

- [ ] **P-04 — Render marker Wrap exit/re-entry**  
  **Depends on:** P-03, R-08.  
  **Change:** Display a seam-crossing step using the approved two-edge treatment rather than an across-grid glide.  
  **Done when:** A right-edge Wrap never animates through the center to the left-edge destination.

- [ ] **P-05 — Render the advance route preview**  
  **Depends on:** P-01, R-07.  
  **Change:** Draw future line segments for the authored preview lead, clipping its advancing front to fractional-beat progress.  
  **Done when:** Ahead line advances with music; no event input changes its geometry.

- [ ] **P-06 — Render the preview correctly at Wrap seams**  
  **Depends on:** P-05, R-08.  
  **Change:** Draw disconnected edge pieces for wrapped preview segments.  
  **Done when:** A wrap is readable as continuation across the boundary and never as a line across the grid.

- [ ] **P-07 — Remove traversed preview immediately**  
  **Depends on:** P-05.  
  **Change:** Clip the path's trailing end to the marker's current route progress.  
  **Done when:** At each frame, no completed route segment lingers behind the marker.

- [ ] **P-08 — Choose a legible route-overlap treatment**  
  **Depends on:** P-06, R-11.  
  **Change:** Implement one lightweight visual differentiation for overlapping/revisited segments (ordering, subtle offset, or opacity).  
  **Done when:** A dedicated looping chart remains interpretable without glow clutter.

- [ ] **P-09 — Load and start a chosen chart in Play**  
  **Depends on:** C-07, T-02, F-07.  
  **Change:** Connect a chart file and its audio reference to a Play session using the existing adapters.  
  **Done when:** A reference chart begins from its defined start cell and heading.

- [ ] **P-10 — Add a short pre-song count-in**  
  **Depends on:** P-09, T-07.  
  **Change:** Implement a clearly visible/audible count-in before scoring begins, with no movement-time drift.  
  **Done when:** The chart starts on a deterministic beat-zero instant; no early Miss occurs during count-in.

- [ ] **P-11 — Connect keys to judgement and show grades**  
  **Depends on:** P-09, T-16.  
  **Change:** Feed timestamped key presses to the judgement session and display the most recent Perfect/Good/Bad/Miss near the game field.  
  **Done when:** Correct, incorrect, and missing actions show expected grades; the route never diverges.

- [ ] **P-12 — Show combo and accuracy counts**  
  **Depends on:** P-11.  
  **Change:** Add a minimal non-obstructive HUD for current combo and accumulated judgement counts.  
  **Done when:** HUD values agree with the pure results object at every test event.

- [ ] **P-13 — Pause and resume a game session**  
  **Depends on:** P-11, T-04.  
  **Change:** Add a pause overlay that suspends audio, route time, and judgement expiry together.  
  **Done when:** No movement or Miss accrues while paused; resume remains synchronized.

- [ ] **P-14 — Finish at song end and display results**  
  **Depends on:** P-12, T-16.  
  **Change:** Transition Play to Results at audio completion, resolving any eligible outstanding events according to the clock.  
  **Done when:** Results screen shows all grade totals and lets the player return to Home.

- [ ] **P-15 — Retry the same chart from the start**  
  **Depends on:** P-14, T-18.  
  **Change:** Add Retry to reset song position, route sample, score, and feedback in one action.  
  **Done when:** Two attempts of the same fixture start identically, without carryover judgements.

### Human gate 6

**Do not start the next stage until approved.** Owner plays several fixture charts, judges readability, and approves the feel of input and Wrap movement.

---

## Stage 7: Editor shell and chart parameters

**Objective:** Make the authoring workspace visible and synchronize the grid preview with a timeline cursor.

- [ ] **E-01 — Create the Editor screen layout**  
  **Depends on:** F-07, A-06.  
  **Change:** Build a resizable workspace with an audio timeline area and a simultaneous 4×4 preview panel.  
  **Done when:** Neither view hides the other at the minimum supported window size.

- [ ] **E-02 — Introduce editable ChartDraft state**  
  **Depends on:** E-01, C-01.  
  **Change:** Create a mutable editor-side draft object with explicit dirty state and conversion into validated Chart.  
  **Done when:** Editing never mutates an active gameplay chart snapshot.

- [ ] **E-03 — Create a New Chart action**  
  **Depends on:** E-02.  
  **Change:** Initialize a new draft with schema version, empty turns, 4×4 grid, and documented placeholder timing values.  
  **Done when:** New Chart clears prior draft after confirmation when necessary and shows empty timeline.

- [ ] **E-04 — Add a local audio file picker**  
  **Depends on:** E-03, T-02.  
  **Change:** Implement an editor action to choose a local supported audio file and store a chart-relative reference.  
  **Done when:** Choosing a valid track updates the draft; cancel leaves it unchanged.

- [ ] **E-05 — Edit BPM with input validation**  
  **Depends on:** E-02, C-04.  
  **Change:** Add a numeric BPM control bound to ChartDraft.  
  **Done when:** Invalid BPM values cannot be committed; changing valid BPM updates the time ruler.

- [ ] **E-06 — Edit chart audio offset**  
  **Depends on:** E-05.  
  **Change:** Add a signed offset control with the same units/sign as the SongClock contract.  
  **Done when:** An offset adjustment shifts event placement against the track without changing stored beat numbers.

- [ ] **E-07 — Set marker starting point on the grid**  
  **Depends on:** E-02.  
  **Change:** Let the author pick one of 16 start cells from the editor grid.  
  **Done when:** Selected cell writes to ChartDraft and appears in subsequent route preview.

- [ ] **E-08 — Set initial cardinal heading**  
  **Depends on:** E-07.  
  **Change:** Add four initial-heading controls (or a compact equivalent).  
  **Done when:** Each heading generates the correct unturned first step in preview.

- [ ] **E-09 — Edit preview lead in beats**  
  **Depends on:** E-02, R-07.  
  **Change:** Add an author-configurable preview-lead control with validated positive bounds.  
  **Done when:** Preview length changes without shifting event timestamps; invalid values show feedback.

- [ ] **E-10 — Draw beat ticks on the timeline**  
  **Depends on:** E-05.  
  **Change:** Render a quarter-note ruler labeled by beat index using the Chart BPM mapping.  
  **Done when:** At BPM 120 adjacent quarter-note ticks represent 0.5 seconds of audio.

- [ ] **E-11 — Implement horizontal timeline scroll**  
  **Depends on:** E-10.  
  **Change:** Add horizontal panning through longer songs without changing chart event coordinates.  
  **Done when:** Visible beat tick positions update smoothly; chart data is unchanged.

- [ ] **E-12 — Implement timeline zoom**  
  **Depends on:** E-11.  
  **Change:** Add bounded horizontal zoom around cursor/playhead.  
  **Done when:** Zoom changes screen pixels per beat but not stored beat positions.

- [ ] **E-13 — Add a draggable timeline playhead**  
  **Depends on:** E-12, T-05.  
  **Change:** Provide a time cursor that snaps to a supported time resolution and can seek through the selected song.  
  **Done when:** Scrubbing changes editor preview time, not gameplay scores.

- [ ] **E-14 — Drive the editor grid from Route sampling**  
  **Depends on:** E-13, R-05, R-07.  
  **Change:** Render marker and future path from the current ChartDraft and playhead time using the same Route engine as Play.  
  **Done when:** At the same beat, editor and Play show the same point, heading, and seam geometry.

### Human gate 7

**Do not start the next stage until approved.** Owner can create a draft, select audio, edit chart parameters, and scrub the preview without gameplay.

---

## Stage 8: Editor event authoring and round-trip workflow

**Objective:** Make it possible to create, validate, save, reopen, and test-play a complete local chart.

- [ ] **I-01 — Insert a Left/Right event at the playhead**  
  **Depends on:** E-13, E-02.  
  **Change:** Add two explicit insertion actions that place a turn on a permitted beat in ChartDraft.  
  **Done when:** New event appears at the correct beat and updates the route immediately.

- [ ] **I-02 — Select a turn event on the timeline**  
  **Depends on:** I-01.  
  **Change:** Support click selection and a clear selected-state highlight.  
  **Done when:** Clicking one event selects that event only; empty-space click deselects.

- [ ] **I-03 — Move selected turn to another beat**  
  **Depends on:** I-02.  
  **Change:** Add dragging or a discrete move command to change an event's beat while preserving its direction.  
  **Done when:** Move honors beat snapping and updates preview without duplicating the event.

- [ ] **I-04 — Change selected event Left ↔ Right**  
  **Depends on:** I-02.  
  **Change:** Add a compact direction switch for the selected event.  
  **Done when:** Changed direction is reflected by the marker path in the synchronized grid.

- [ ] **I-05 — Delete a selected turn event**  
  **Depends on:** I-02.  
  **Change:** Add Delete key/button handling for the selected event.  
  **Done when:** Event disappears from timeline and route; deleting with no selection is harmless.

- [ ] **I-06 — Reject duplicate events at the same beat**  
  **Depends on:** I-01, C-05.  
  **Change:** Block or visibly resolve insertion/move attempts that would place two turns at one beat.  
  **Done when:** The editor never silently overwrites another event; user sees why the action failed.

- [ ] **I-07 — Make timeline snap resolution explicit**  
  **Depends on:** E-10, I-03.  
  **Change:** Show one supported snap step for MVP quarter-beat events and apply it to insertion/movement.  
  **Done when:** A dragged event always lands on an allowed beat, even after zooming.

- [ ] **I-08 — Add editor audio play/pause**  
  **Depends on:** E-13, T-04.  
  **Change:** Wire an editor playback control to the shared audio backend and update playhead from SongClock.  
  **Done when:** Audio and cursor run together; pausing freezes both.

- [ ] **I-09 — Keep playhead visible during playback**  
  **Depends on:** I-08, E-11.  
  **Change:** Add follow-playhead scrolling with a manual-scroll override.  
  **Done when:** Long-song playback keeps the cursor visible unless the author explicitly pans away.

- [ ] **I-10 — Show chart validation errors inline**  
  **Depends on:** C-04, C-05, E-02.  
  **Change:** Add an editor panel/status message listing validation errors with their affected field or beat.  
  **Done when:** An invalid chart cannot silently appear playable; error location is actionable.

- [ ] **I-11 — Save the draft as a local chart**  
  **Depends on:** I-10, C-08.  
  **Change:** Add Save/Save As flow using approved JSON serializer and an explicit destination.  
  **Done when:** Saved file is parseable and refers to the correct local audio path.

- [ ] **I-12 — Open an existing chart in the editor**  
  **Depends on:** I-11, C-07.  
  **Change:** Implement Open Chart to load validated chart values into a new ChartDraft and populate controls.  
  **Done when:** Opening a saved chart reproduces audio path, BPM/offset, start pose, preview lead, and events.

- [ ] **I-13 — Guard against unsaved work**  
  **Depends on:** I-11, E-02.  
  **Change:** Prompt before New, Open, exit, or leaving Editor when there are unsaved modifications.  
  **Done when:** Cancel preserves the draft; save/discard actions follow the selected outcome.

- [ ] **I-14 — Add Undo for one editor action**  
  **Depends on:** I-03, I-04, I-05.  
  **Change:** Implement a bounded edit history for add/move/change/delete actions (snapshot or command approach).  
  **Done when:** A single Undo restores the exact preceding draft state and route preview.

- [ ] **I-15 — Add Redo for undone edits**  
  **Depends on:** I-14.  
  **Change:** Implement Redo and clear its branch after a new edit.  
  **Done when:** Undo/Redo for event operations is deterministic and does not corrupt draft dirty state.

- [ ] **I-16 — Add basic authoring shortcuts**  
  **Depends on:** I-15.  
  **Change:** Bind documented shortcuts for save, undo, redo, delete, and play/pause without conflicting with text controls.  
  **Done when:** Shortcuts work only in relevant editor contexts; typing BPM does not accidentally trigger them.

- [ ] **I-17 — Test-play the current draft**  
  **Depends on:** I-10, P-09.  
  **Change:** Add a Test Play action that validates and launches the game from a snapshot of the current draft without requiring a prior save.  
  **Done when:** The chart performed matches the editor route and all events; invalid drafts remain in Editor with errors.

- [ ] **I-18 — Return from test-play without losing draft**  
  **Depends on:** I-17.  
  **Change:** When test-play ends or is aborted, restore the same editor draft, selection, playhead, and unsaved indicator.  
  **Done when:** No edits are lost or duplicated after returning to Editor.

- [ ] **I-19 — Prove editor-save-game round trip**  
  **Depends on:** I-12, I-17.  
  **Change:** Add an end-to-end test using New → insert turns → save → reopen → test-play with a known fixture.  
  **Done when:** Saved chart and played chart have identical beat events and route positions at all sampled beats.

### Human gate 8

**Do not start the next stage until approved.** Owner produces a new chart without manually editing JSON, reopens it, and verifies test-play matches preview.

---

## Stage 9: Windows MVP hardening and release

**Objective:** Make the documented game/editor experience stable, distributable, and objectively testable.

- [ ] **Q-01 — Persist local user preferences**  
  **Depends on:** F-07.  
  **Change:** Add a tiny versioned local preferences file for only required settings, with sane defaults on first run.  
  **Done when:** App starts with no config; saved preferences survive restart; corrupt config falls back safely.

- [ ] **Q-02 — Allow remapping Left/Right keys**  
  **Depends on:** Q-01, T-10.  
  **Change:** Add two settings controls for remapping turn actions, rejecting identical/conflicting bindings.  
  **Done when:** Remapped keys work in the game after restart; default bindings remain available.

- [ ] **Q-03 — Add music volume control**  
  **Depends on:** Q-01, T-02.  
  **Change:** Add a persistent music volume control wired to the audio backend.  
  **Done when:** Changing volume affects playback immediately and survives restart.

- [ ] **Q-04 — Add timing offset setting**  
  **Depends on:** Q-01, T-08.  
  **Change:** Expose a persistent user timing adjustment separate from chart audio offset, with a reset button.  
  **Done when:** A known offset shifts judgement alignment in the documented direction without altering the stored chart.

- [ ] **Q-05 — Make navigation keyboard-usable**  
  **Depends on:** P-14, E-01.  
  **Change:** Ensure Home, Settings, Editor, Play, and Results can be reached or exited with clear keyboard focus and Escape behavior.  
  **Done when:** Owner can navigate core screens without a mouse except where editor interaction intentionally requires one.

- [ ] **Q-06 — Handle missing or corrupt audio in UI**  
  **Depends on:** P-09, C-09.  
  **Change:** Show a clear error state for missing, unsupported, or unreadable chart audio and provide a path back to selection/editor.  
  **Done when:** Invalid audio never crashes the app or leaves a silent running session.

- [ ] **Q-07 — Handle invalid charts in Play UI**  
  **Depends on:** P-09, C-12.  
  **Change:** Show validation diagnostics rather than starting gameplay when a chart has invalid schema or event data.  
  **Done when:** An invalid fixture never enters active Play; user can return safely.

- [ ] **Q-08 — Keep grid and editor usable when resized**  
  **Depends on:** P-01, E-01.  
  **Change:** Constrain scaling and minimum window layout so point spacing and UI controls remain usable.  
  **Done when:** Two supported resolutions show a complete grid and visible critical editor controls.

- [ ] **Q-09 — Add reduced-effects visual option**  
  **Depends on:** Q-01, P-11.  
  **Change:** Make nonessential glow/transitions disableable without removing trajectory or judgement readability.  
  **Done when:** With effects disabled, every note and Wrap seam remains legible.

- [ ] **Q-10 — Run deterministic core regression suite**  
  **Depends on:** R-11, T-17, I-19.  
  **Change:** Create one repeatable command that runs chart, route, clock/judgement, and save/load regression tests.  
  **Done when:** Clean Windows CI passes the full suite and clearly identifies failing subsystems.

- [ ] **Q-11 — Profile startup and idle footprint**  
  **Depends on:** F-09, Q-10.  
  **Change:** Measure cold start, idle RAM/CPU, and release footprint against the blank-app baseline in `mvp/BASELINES.md`.  
  **Done when:** Measurements include environment, technique, and before/after numbers; regressions are justified or fixed.

- [ ] **Q-12 — Profile rhythmic playback under load**  
  **Depends on:** P-15, Q-10.  
  **Change:** Run fixture playtests with increased rendering load and capture clock/judgement drift and frame pacing.  
  **Done when:** A saved report shows whether the same key timestamps yield identical grades despite load; critical drift is fixed before release.

- [ ] **Q-13 — Build a clean portable Windows package**  
  **Depends on:** Q-10.  
  **Change:** Create a release build bundle with the executable, required dynamic libraries/assets, and no local absolute-path dependencies.  
  **Done when:** A clean Windows machine/VM can launch it without developer tools and open a bundled rights-clean test fixture.

- [ ] **Q-14 — Write end-user README and controls**  
  **Depends on:** Q-13.  
  **Change:** Add English `README.md` with launch, keys, chart loading, editor basics, local audio rules, and known limitations.  
  **Done when:** An unfamiliar tester can complete a start-to-finish play and editor flow using only the README.

- [ ] **Q-15 — Choose and add a public-source license**  
  **Depends on:** Q-13.  
  **Change:** After owner approval, add the chosen `LICENSE` file and verify notices for vendored dependencies and assets.  
  **Done when:** Repository distribution terms are explicit; no license is assumed from GitHub visibility alone.

- [ ] **Q-16 — Run a fresh-user MVP acceptance checklist**  
  **Depends on:** Q-14, Q-15.  
  **Change:** Execute and record an English checklist: start → import audio → edit chart → save → reopen → test-play → Miss/No Fail → results → retry.  
  **Done when:** Every GDD §11 acceptance criterion is marked pass/fail with build identifier; blocking failures create narrowly scoped follow-up tasks.

- [ ] **Q-17 — Tag the approved v0.1 release**  
  **Depends on:** Q-16.  
  **Change:** Only after the final human gate approves Q-16, create the `v0.1` tag and publish the packaged build with concise English release notes.  
  **Done when:** Tag points to the tested commit; downloadable Windows archive matches the acceptance build.

### Human gate 9

**Do not start the next stage until approved.** Owner runs a clean packaged build, completes the end-to-end checklist, and explicitly approves a v0.1 tag.

---

## Sequence rules

1. Finish and verify a task before starting its dependent tasks; the **Depends on** field identifies the immediate prerequisite IDs, while prior stage approvals are global prerequisites.
2. The task ID and its **Done when** condition define the request scope. If the design changes, update GDD and this roadmap first.
3. Keep PRs narrowly scoped. A task may touch a couple of production files and a relevant test, but it should not absorb another numbered task.
4. After each stage, provide a runnable artifact or reproducible test output, **timing/latency and performance evidence first**, package-size evidence as context, an English changelog, and explicit manual checks for the owner.
5. Use only original, generated, or appropriately licensed fixtures. Never commit imported user tracks without permission.
6. Check a task's box only after verifying the corresponding commit on the target branch. Preserve the original ID even when task wording changes.
7. If the owner rejects a human gate, add explicit corrective tasks before proceeding; do not bury repairs in unrelated tasks.

## Ready-to-use implementation request template

> Implement **LOCUS task `<ID>`** from `mvp/ROADMAP.md`. Read `mvp/GDD.md` and all applicable architecture decisions. Implement **only this task** and its required focused tests. Keep all project content in English. Run the relevant build/tests and summarize changes, test results, and any risks. Do not start any adjacent task and do not mark a human gate as approved.

## MVP release completion

v0.1 is complete **only when** GDD section 11 passes in the final Windows build, the editor can round-trip a local audio-backed chart, Wrong/Miss never alters the authored route, Wrap rendering is legible, timing remains consistent under variable frame scheduling, and the owner approves the final human gate. Q-17 must not run before that approval.
