# LOCUS — Game Design Document

**Document version:** 0.1-draft  
**Project:** LOCUS  
**Target product milestone:** v0.1 (first playable MVP)  
**Last updated:** 2026-11-04  
**Document language:** English  
**Approval status:** Draft — requires owner review

> This document specifies the intended gameplay and MVP boundaries. It is not an implementation roadmap or a claim that the game has been built.
>
> **Decision legend:** **Confirmed** = explicitly agreed by the project owner; **Proposed** = reasonable working design, awaiting validation; **Open** = not decided. Do not silently promote a proposed rule into a confirmed one.

## 1. High Concept

LOCUS is a lightweight, minimalist rhythm game played on a grid of points. A single bright marker travels automatically along the correct route in musical time. The route is progressively revealed in front of the marker. The player does **not** steer the marker to determine where it goes; instead, they perform the indicated **left** and **right** turns at the correct moments.

Missing a turn or pressing the wrong button does not derail movement. The marker still follows the authored route, but the corresponding event receives a **Miss**. This is a permanent no-fail approach, similar in spirit to no-fail play in A Dance of Fire and Ice.

**One-sentence pitch:** Read the path ahead, then perform its turns in rhythm.

### 1.1 Design pillars

1. **Immediate readability.** One moving marker, one compact grid, two primary actions, and a readable future route.
2. **Rhythm-first precision.** Judgements are based on the music timeline and timestamped inputs, never on rendered animation frames.
3. **Low-latency and reliable first.** Precise audio/visual alignment and minimal input-to-feedback delay take priority over the download size. Profile the actual player experience rather than optimizing a few kilobytes.
4. **Pleasant minimalism.** A dark canvas, soft pastel colors, smooth movement, and carefully limited effects.
5. **Creation is a first-class feature.** The editor shares the game's chart format and route rules; charts are not handcrafted in source code.
6. **Controlled complexity.** Avoid feature sprawl and unnecessary architectural layers. A build of tens or hundreds of megabytes is acceptable when justified by measurable timing, performance, robustness, or maintenance benefits.

### 1.2 Intended audience

- Rhythm-game players seeking a readable, accuracy-focused challenge.
- Newcomers comfortable with simple two-button input.
- Creators interested in placing timed turns against their own local audio.

These are design hypotheses, not validated audience research.

## 2. Product Scope

### 2.1 Confirmed MVP direction

- Initial target platform: **Windows**.
- Grid: **4 columns × 4 rows** (16 points).
- One automatically moving marker and one authored correct route.
- Relative turn input: **Left** and **Right**.
- Only orthogonal travel: up, right, down, left.
- Baseline movement: **one point-to-point step per quarter-note beat**.
- Progressive advance preview of the correct route.
- **Wrap** boundary traversal: leaving one side enters from the opposite side, preserving heading.
- Timing judgements: **Perfect, Good, Bad, Miss**.
- Always-on no-fail behavior; errors affect results, not route execution.
- Chart editor combining an audio timeline and a synchronized grid preview.
- Local audio file import; online distribution is not required for the MVP.
- Dark background with gentle pastel accents and minimal effects.
- Public project/source repository. The exact source-code license is **Open**.

### 2.2 Explicitly outside the first playable MVP

- User accounts, chart hosting, rankings, comments, forums, and moderation.
- Mobile and non-Windows platform support.
- Diagonal movement and variable grid size.
- Changing grid dimensions in the middle of a chart.
- Holds, hazards, additional input types, and complex note gimmicks.
- Feature-rich visual skin systems and elaborate particle effects.
- Mandatory failure/game-over mechanics.

These exclusions do not prohibit later development; they prevent premature complexity.

### 2.3 Open scope details

Final v0.1 feature completeness (for example, results history, tutorial depth, and settings breadth) will be settled when the roadmap is prepared. The roadmap must not reinterpret open decisions as existing requirements.

## 3. Core Game Rules

### 3.1 Grid and coordinates — Confirmed foundation

The game area consists of 16 stationary points in a regular 4×4 matrix. The marker occupies one logical grid point at each beat boundary and interpolates visually between points during a step.

A suggested internal coordinate convention is x = 0..3 from left to right and y = 0..3 from top to bottom; this is an implementation detail, not a player-facing rule.

The 4×4 layout evokes the musical four-count, but **grid positions are spatial locations, not beat indices**. Movement and event timing are driven by the chart's musical timeline. A 4/4 meter is a sensible initial example, not a hidden requirement that every chart must be in 4/4.

### 3.2 Automatic movement — Confirmed

- The chart defines a start point and initial heading.
- The marker moves forward autonomously, one grid edge per quarter note at the baseline rate.
- A chart turn changes the heading when its scheduled moment is reached, whether or not the player pressed anything.
- If there is no chart turn at that beat, the marker continues in its current direction, wrapping to the opposite side when needed.
- Visual movement should be smooth; logical routing remains discrete and deterministic.
- Keyboard input **never** directly selects the next logical position.

**Proposed event ordering at beat boundaries:** apply any scheduled chart turn, then resolve the next step, wrapping if the step leaves the grid. Confirm this ordering in a deterministic route test.

### 3.3 Player input — Confirmed

The primary action set contains two **relative** turns:

- **Left:** rotate the current travel direction 90° counterclockwise.
- **Right:** rotate the current travel direction 90° clockwise.

Buttons are not absolute arrows. The same left/right commands work regardless of whether the marker is currently heading up, down, left, or right.

The initial keyboard bindings are **Open**; use remappable defaults rather than treating a particular pair of keys as the gameplay design itself.

### 3.4 Turn events — Confirmed foundation

A minimal gameplay event contains:

1. A time position on the chart.
2. The required turn direction: Left or Right.

A time position is preferably authored in musical units (beats/ticks) and resolved against the audio timeline. The exact serialized representation is **Proposed**, not finalized.

For the baseline rule, turn events occur at eligible beat boundaries. There is no mandatory input on straight, uneventful steps.

**Illustrative event list, not a committed file schema:**

    Beat 4  → Right
    Beat 8  → Left
    Beat 12 → Left
    Beat 16 → Right

Start position, heading, BPM information, audio offset, and preview lead belong to chart-level configuration rather than being repeated on each turn. Boundary handling is fixed to Wrap in the MVP and does not need a chart setting.

### 3.5 No-fail route execution — Confirmed

The authored route is authoritative. At a scheduled turn, the marker takes the correct turn even if the input is missing, early, late, or wrong.

Input performance changes judgement, combo, accuracy, and other eventual results only. It cannot move the marker to an unintended cell or desynchronize subsequent chart events.

There are two deliberately independent responsibilities:

- **Route simulation:** Where is the marker supposed to be at musical time t?
- **Input judgement:** What grade did the player's actions earn for the event at time t?

The renderer displays the results of these systems and does not decide either of them.

### 3.6 Boundary handling — Confirmed: Wrap

**Wrap is the only boundary rule in the MVP.** When a movement step would leave the 4×4 grid, the marker re-enters at the opposite edge on the same row or column, with its heading unchanged. For example, moving right from (3, 1) arrives at (0, 1); moving up from (2, 0) arrives at (2, 3).

- A wrapped step consumes the same one-quarter-note duration as any other step.
- Wrapping is automatic: it never requires an extra Left/Right input or produces a judgement.
- If a charted turn occurs at an edge, resolve the turn before determining whether the following step wraps (**proposed ordering**, to be confirmed by a test).
- Do not implement Bounce or a chart-selectable boundary-mode setting in the MVP.
- For visual readability, a wrapped step must **not** be drawn as a line traversing the grid interior between opposite points.

**Proposed visual treatment:** Split the moving segment and future-path preview at the outer grid boundary: show a brief exit on one side and re-entry on the opposite side. The exact animation is a playtest decision, but the logical destination and timing are fixed.

## 4. Audio Time and Judgement

### 4.1 Musical clock — Confirmed design principle

Authoritative timing derives from the audio/music timeline, not from rendered frames or a count of update-loop iterations.

The implementation must account for playback position, audio offset, paused/resumed state, input timestamps, and eventual user calibration. Rendering uses the same timebase to interpolate position and reveal the future route, but render latency must not decide hit accuracy.

The baseline traversal is one movement step per quarter note. BPM and audio offset are necessary chart parameters. Tempo changes and variable step duration are deferred until justified by playtesting.

### 4.2 Timing windows — Confirmed categories, Open thresholds

Every input event has exactly one final judgement:

| Grade | Meaning |
| --- | --- |
| Perfect | Closest to the intended musical instant |
| Good | Noticeably offset, still accurate |
| Bad | Inside the broad accepted window |
| Miss | Wrong turn or no valid input inside the window |

The boundaries of Perfect/Good/Bad/Miss are **Open**. They must be numerical, consistent, and measurable in the engine once selected; thresholds should be tested for playability and platform/audio latency.

Windows are centered on the event's musical timestamp. Early and late hits are both possible inside the selected window; visual path progress does not change their evaluation.

### 4.3 Input adjudication — Confirmed intent with proposed edge rules

**Confirmed:**

- Only presses within a timing window can score.
- No early-input buffer or pre-hold scoring.
- A wrong turn input within the active event window immediately makes that event **Miss**.
- The route still follows its correct authored turn.

**Proposed for MVP, pending validation:**

- The first eligible key-down within an event's window resolves that event; subsequent presses cannot change its grade.
- A correct first key-down receives Perfect, Good, or Bad by absolute timing error.
- An event without a valid key-down by its window's end becomes Miss.
- Presses outside all event windows are ignored rather than punished.
- Held keys do not generate repeated judgements; evaluate fresh key-down transitions.
- Simultaneous/oppositely directed or overlapping event windows must be handled deterministically; preferably the chart validator disallows ambiguous overlapping events at the MVP stage.

Only one judgement is counted per chart event.

### 4.4 Scoring and feedback — Partly open

**Confirmed:** The game should display the accuracy categories Perfect/Good/Bad/Miss and offer accuracy-sensitive play in the style of precision rhythm games.

**Proposed:** Show a lightweight current judgement, combo, and summary after the chart. Combo breaks on Miss. Exact score formula, grade weights, rank thresholds, and leaderboards are **Open** and should not complicate the MVP core.

Provide visible feedback without obscuring the trajectory. Audio hit sounds, if implemented, must be optional and must not interfere with local music playback.

## 5. Trajectory Preview and Visual Behavior

### 5.1 Progressive route reveal — Confirmed

The correct future trajectory is drawn on the grid before the marker reaches it. The visible route grows forward over musical time at the marker's movement rate, maintaining a lead ahead of the marker.

The preview indicates **what the chart demands**, not what the player's inputs have caused. It must remain correct on misses.

**Proposed time-based model:** At music time t, render the authored path between the marker at t and the route position at t + L, where L is a preview lead in musical beats. As t advances, both the marker and the leading edge progress. This expresses the intended continuous path drawing without tying it to frame count.

### 5.2 Preview lead — Confirmed author control, Open default

The chart author can configure how far ahead the route is revealed. A lead of roughly one bar is a promising starting point but **not** a settled default. Density, tempo, crossings, and player legibility will determine a suitable range and any minimum.

The preview should not jump or become misleading when the path crosses itself or revisits points. At a Wrap seam, render disconnected edge exit/re-entry segments, not a long line across the grid interior. This is a primary playtest risk on a 4×4 grid.

### 5.3 Past trajectory — Confirmed

Already traversed trajectory disappears immediately behind the marker. Avoid persistent route history or long trailing particles that could mask upcoming information.

A minimal glow around the marker is acceptable as presentation, but it must not imply a remaining past path.

### 5.4 Art direction — Confirmed

- Dark background.
- Soft, muted/pastel palette.
- Bright, clearly distinguishable player marker.
- High-contrast but non-aggressive future path.
- Smooth, responsive motion and small, deliberate judgement feedback.
- A clean interface without unnecessary borders, dashboards, or particle clutter.

Colors, typefaces, sizes, and animation curves are to be finalized through screenshots and playtest gates. Readability takes precedence over decorative fidelity.

## 6. Chart Data Model

### 6.1 Required conceptual fields

| Field | Purpose | Status |
| --- | --- | --- |
| Format version | Enable future migrations and validation | Proposed |
| Chart/title metadata | Identify a local chart | Proposed |
| Audio reference | Point to a local audio asset without embedding it | Confirmed approach |
| BPM | Convert beats into music time | Required by baseline |
| Audio offset | Align chart beats to the track | Required by baseline |
| Grid width/height | Initially fixed to 4×4 | Confirmed initial value |
| Start point | Initial marker cell | Required |
| Initial heading | Initial travel direction | Required |
| Boundary rule | Fixed Wrap for MVP; no per-chart field needed | Confirmed |
| Preview lead | Author-configured look-ahead | Confirmed capability |
| Turn events | Ordered pairs of beat/time + Left/Right | Confirmed foundation |

The proposed concrete MVP v1 JSON schema, field constraints, file naming, and portable local audio-path rules are documented in [CHART_FORMAT.md](./architecture/CHART_FORMAT.md) (A-05). This format is pending Stage 1 owner approval; no online URL or server dependency is required to play a local chart.

### 6.2 Essential invariants

- Event times are finite, ordered, and valid for the chart timing model.
- One turn event per permitted step boundary for the MVP.
- Every turn is Left or Right; straight movement is implicit.
- Start point lies within the grid.
- Every calculated movement resolves to a valid point under Wrap (modulo-4 coordinates), without changing heading.
- Route generation produces the same outcome in the editor, test harness, and game.
- A chart with no turn events is valid and plays as automatic movement.
- Parsing errors produce clear messages instead of crashes or silent correction.

### 6.3 Forward compatibility without overengineering

Version the chart schema from the start. Keep parsing and validation separate from route simulation. Do **not** implement a general scripting system, plugin API, or speculative event hierarchy in the MVP.

Future possibilities include grid size fixed per chart, grid resizing during a chart, diagonals, BPM changes, pauses, and additional note events. None of these are guaranteed v0.1 features.

## 7. Chart Editor

### 7.1 Confirmed interaction model

A chart editor must show two synchronized views:

1. **Music timeline:** Turn events positioned against the imported audio.
2. **Grid preview:** The marker, current heading, and corresponding route at the selected musical time.

The author can move through the song and inspect exactly what players will see and when turns are judged.

### 7.2 Minimum editing workflow — Proposed MVP scope

1. Create or open a local chart.
2. Select a local audio file and set timing basics (BPM and offset).
3. Choose the starting point and initial heading.
4. Place, select, move, change direction, and delete Left/Right turn events on the timeline.
5. Scrub and play/pause the audio while watching the grid preview.
6. Configure future-path lead; Wrap is fixed and does not require an editor control.
7. Validate chart events and inspect any detected errors.
8. Save and reopen the chart without changing its route or event timing.
9. Test-play the same chart with the actual game judgement rules.

Undo/redo, waveform rendering, shortcuts, snap resolution, and chart metadata editing should be prioritized during roadmap planning rather than assumed complete. A fast, predictable editor is more important than a wide feature set.

### 7.3 Local audio and sharing

MVP playback uses local audio files. The application must not automatically upload music, require an account, or silently bundle user-supplied copyrighted tracks.

Public chart sharing and server-hosted music are out of scope until licensing, moderation, storage, and security have been deliberately addressed.

## 8. Menus, Settings, and Accessibility

**Proposed minimal navigation:** Start / chart selection, Play, Editor, Settings, and Results.

**Important settings to evaluate for v0.1:**
- Input key remapping.
- Master/music/hit-sound volume.
- Audio/input/visual timing calibration or offset where technically needed.
- Display and window mode.
- Reduced visual effects and readable judgement indicators.

Avoid building a large settings framework until particular settings are proven necessary. Feedback should not rely on color alone. Keyboard focus and controls must remain predictable in menus and editor.

## 9. Technical Direction and Quality

### 9.1 Platform and stack

**Confirmed:** Windows first. **Highest priorities** are precise musical timing, minimal input-to-feedback and audio-output delay, stable performance, and correctness under frame stalls. **Secondary priorities** are maintainable code and a tightly controlled dependency/feature surface. Binary/download size is a diagnostic metric, **not a size cap**; a substantially larger build is acceptable for demonstrated benefits.

**Proposed (A-09; Stage 1 owner approval pending):** The initial Windows MVP stack is **C++17 + raylib 6.0** for window/rendering and basic audio transport, **raygui 5.0** for simple editor controls, **native Windows Common Item Dialogs** for file picking, and **nlohmann/json v3.12.0** for chart serialization. See [STACK_DECISION.md](./architecture/STACK_DECISION.md). A-07's real-device functional test was successful per the owner; **no numerical end-to-end latency was measured**.

**Conditional alternative:** SDL3 only after an observed timing, input, audio, or reliability blocker. Do not build two complete backends speculatively.

Godot and Rust are not categorically forbidden. Choose revisions and future replacements by **measured audio/input timing, stability, and performance**, not by the fewest executable bytes. The selected tools require owner approval at Human Gate 1 before production work.

### 9.2 Architectural boundaries — Proposed

- **Chart:** Parse, validate, and represent authored data.
- **Clock/Audio:** Track authoritative music time and playback lifecycle.
- **Route:** Derive deterministic positions and headings from chart time, with no player-input dependency.
- **Judgement:** Consume timestamped input and resolve one grade per turn event.
- **Presentation:** Draw the grid, marker, future route, feedback, and UI.
- **Editor:** Modify chart data and reuse production route/clock behavior.

These are responsibility boundaries, **not** a demand for six frameworks, dynamic plugins, or one class per noun. Favor clear interfaces and simple ownership.

### 9.3 Quality constraints

- Correct judgement must not depend on rendering frame rate.
- Audio seek/pause/resume must not duplicate or skip turn judgements.
- The game must remain playable on ordinary Windows hardware without a heavyweight runtime installation.
- Benchmark input-event capture delay, audio/visual synchronization, audio-output latency where physical measurement is feasible, frame pacing, startup, memory, and release size. **No hard release-size limit is imposed**; prioritize timing and performance when metrics conflict.
- Chart simulation and editor preview must match exactly.
- Automated tests should cover turn rotation, boundary behavior, chart validation, timing-window edges, and miss/no-fail invariants.
- Dependency additions must be justified by real timing, performance, stability, and maintainability needs. Avoid uncontrolled scope/dependency growth; package size is informational rather than a primary constraint.
- All repository content (code, documentation, comments, commit messages, UI text, and user-facing game strings) is written in **English**.

## 10. Human Gates and Change Control

**Confirmed:** The owner reviews each completed development stage before dependent work proceeds. Detailed one-task-per-request planning will live in a separate roadmap, not in this GDD.

Each review should include:
- A runnable build or editor demonstration.
- A short summary of behavior delivered.
- Evidence of relevant tests or benchmarks.
- Known issues and deviations from this document.
- Specific manual checks the owner can perform.
- Explicit approval or requested changes.

Changes to core gameplay rules, timing, chart format, or scope require the owner's approval and a GDD update. Prefer small, reviewable code changes even when a gate occurs at the end of a larger stage.

### Early human intervention and communication — Confirmed

If any task needs the owner **before** the planned end-of-stage review (real-device audio/keyboard testing, visible/feel judgement, installation on the owner's Windows machine, unresolved design choice, permission, or license approval), **stop that blocked part immediately and ask for the exact required action**. The assistant must prominently label the user-facing response with the all-caps heading **HUMAN GATE REQUIRED**. Never bury an early human dependency in small print or report it only after moving to an unrelated stage.

When possible, provide a reproducible launcher, brief manual checklist, and a log/diagnostic bundle the owner can send back. The relevant roadmap item stays **open** until the required evidence arrives or the owner explicitly changes the acceptance criteria. The phrase **HUMAN GATE REQUIRED** applies equally to unscheduled early intervention and to planned owner approval at stage boundaries.

## 11. MVP Acceptance Principles

The first playable MVP should demonstrate all of the following:

- A Windows build displays a 4×4 grid and smoothly moving marker.
- With no turn events, the marker moves automatically in musical quarter-note steps and correctly wraps across all four grid edges.
- The correct route preview becomes visible ahead at the authored lead and disappears behind the marker.
- Timed Left/Right events are graded Perfect, Good, Bad, or Miss using the audio clock, not visual frame position.
- Wrong or absent input still produces the same correct marker trajectory.
- A local audio track and saved local chart can be loaded and played.
- The editor can add/edit/remove timed turns, preview them on the grid, and save/reopen the chart.
- A chart created in the editor plays identically in the game.
- Basic invalid charts are rejected with clear feedback.
- The owner confirms that response, readability, startup, and overall feel are acceptable on the target Windows setup.

These are product-level acceptance criteria, not a task breakdown. Precise performance budgets and gameplay timing thresholds will be added after measurements and playtests.

## 12. Open Decisions and Playtest Questions

| ID | Question | Current direction |
| --- | --- | --- |
| O-01 | Exact beat-boundary ordering for turns and Wrap resolution | Proposed: turn, then resolve step |
| O-02 | Numerical Perfect/Good/Bad/Miss thresholds | Playtest and calibrate |
| O-03 | Initial key bindings and remapping | Two relative actions; bindings TBD |
| O-04 | Handling stray keys outside windows | Proposed: ignore |
| O-05 | Exact preview lead units/default/range | Author-defined; roughly a bar is a test hypothesis |
| O-06 | Preview of overlapping, revisited, and wrapped route segments | Needs visual tests; no line across the grid on Wrap |
| O-07 | Final chart serialization and audio packaging | Keep simple, versioned, local |
| O-08 | Final score, accuracy, combo, and grade formula | Define after timing tests |
| O-09 | Library choice: raylib vs SDL3 | Compare real timing, size, maintenance |
| O-10 | License for publicly available source | Owner to choose before a formal license claim |
| O-11 | Exact v0.1 non-core settings and editor conveniences | Decide in separate roadmap |

## 13. Future Vision (Not MVP Commitments)

Potential later extensions:

- A chart-selected fixed grid size (such as 3×3 or 5×5).
- Dynamic grid resizing within a chart.
- Tempo changes, different step subdivisions, pauses, and novel turn mechanics.
- Linux/macOS/mobile support.
- Online chart publication, discovery, profiles, leaderboards, and community discussion.
- Visual themes and richer editing tools.

Future community features must address music rights, moderation, security, and operational cost before implementation.

---

## 14. Reference Vocabulary

- **Marker:** The single visible player-character dot.
- **Grid point:** A discrete spatial position on the 4×4 matrix.
- **Heading:** The marker's current cardinal travel direction.
- **Step:** One movement between adjacent grid points in the baseline quarter-note rhythm.
- **Turn event:** A chart-defined time plus Left/Right input requirement.
- **Route:** The correct, deterministic sequence of positions and headings.
- **Preview lead:** How far ahead of the marker the correct route is visible.
- **Judgement:** Perfect, Good, Bad, or Miss for one turn event.
- **No-fail:** Misses never change the correct route or end the chart.

**Next document:** A separate v0.1 roadmap with tasks small enough for one implementation request each. This GDD intentionally does not contain that roadmap.
