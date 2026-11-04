# LOCUS — MVP Screen Flow and Editor Wireframes

**Task:** A-06 — Draw editor and game screen flows  
**Status:** Proposed interaction contract; Stage 1 human-gate approval pending  
**Target:** Windows, MVP v0.1  
**Related documents:** [GDD](../GDD.md), [Architecture Boundaries](./BOUNDARIES.md), [Timing](./TIMING.md), [Chart Format](./CHART_FORMAT.md), [Wrap Visuals](./WRAP.md), [Roadmap](../ROADMAP.md)

> **Scope of A-06:** Decide where a user finds each essential action and how screens connect. These are text wireframes and interaction specifications, not implemented interfaces. Exact colors, UI toolkit, fonts, responsive measurements, and key bindings remain subject to the stack decision and playtests.

## 1. UX goals and constraints

1. **Play quickly:** From Home, select a local chart and start with a single explicit Play action.
2. **Create without touching JSON:** In Editor, a timeline and a synchronized 4×4 preview are visible together. Turn events are created, moved, changed, and deleted on the timeline.
3. **Never confuse authored movement with user input:** The route always comes from the chart; only the judgement changes after a wrong/missing Left or Right press.
4. **Keep the game field readable:** A dark canvas, soft pastel elements, restrained HUD, and a clear route-preview line. On Wrap, show separate exit and entry parts, not a line through the grid.
5. **Never lose local work silently:** New, Open, leaving Editor, and closing the app must respect the draft's dirty state.
6. **No hidden network dependency:** Play and Editor use local charts and local audio. No accounts, upload/download screens, hosted charts, leaderboards, or forums in v0.1.
7. **Use one route and one timing model:** Editor preview reads shared Chart/Route/Clock outputs, while Play adds a separate Judgement session.

## 2. Screen inventory and state boundaries

| Screen / overlay | Purpose | How to enter | Primary way out |
| --- | --- | --- | --- |
| **Home / Local Charts** | Pick a saved local chart; start Play or Editor | App launch, normal Results, leaving Settings/Editor | Play, New Chart, Edit Chart, Settings, Exit |
| **Editor** | Edit ChartDraft using timeline and 4×4 preview | New Chart, Open/Edit Chart, Return from Test Play | Save, Test Play, Home after dirty-state handling |
| **Play: Count-in** | Prepare timing without fake chart beats | Start Play or Retry | Automatically to Play: Active |
| **Play: Active** | Display one marker, future route, grades and combo | Count-in completed, Resume | Pause or Results at song end |
| **Pause overlay** | Freeze the active play session | Pause command, possibly focus loss | Resume, Retry, or exit to originating screen |
| **Results** | Show outcome without changing the route | Audio ends in a Play/test-play session | Retry or Home; for test-play, Return to Editor |
| **Settings** | Lightweight local controls and timing preferences | Home | Back to Home |
| **Modal / validation feedback** | Missing media, invalid chart, unsaved work | Action failure, dirty-state transition | Dismiss, correct problem, save or discard |

**Home / Local Charts is one screen**, not a chart service. **Pause is an overlay**, not a separately ticking game simulation. A test-play session uses the **same Play/Results views and judgement rules** as normal Play, with an origin flag and a preserved editor draft.

### 2.1 Navigation diagram

~~~mermaid
flowchart TD
    Home["Home / Local Charts"]
    Settings["Settings"]
    Editor["Editor (ChartDraft)"]
    Validate{"Valid chart + available audio?"}
    CountIn["Play: Count-in"]
    Active["Play: Active"]
    Pause["Pause overlay"]
    Results["Results"]
    Dirty{"Unsaved Editor changes?"}
    Prompt["Save / Discard / Cancel"]
    Error["Validation / file error"]

    Home -->|"Settings"| Settings
    Settings -->|"Back"| Home

    Home -->|"New / Edit chart"| Editor
    Home -->|"Play selected chart"| Validate
    Editor -->|"Test Play draft snapshot"| Validate
    Validate -->|"Yes"| CountIn
    Validate -->|"No"| Error
    Error -->|"Dismiss; keep source"| Home
    Error -.->|"If from Editor: return to same draft"| Editor

    CountIn -->|"Count-in completes"| Active
    CountIn -->|"Pause"| Pause
    Active -->|"Pause / focus loss"| Pause
    Pause -->|"Resume"| Active
    Pause -->|"Retry"| CountIn
    Active -->|"Track ends"| Results
    Results -->|"Retry"| CountIn

    Editor -->|"Home / close / New / Open"| Dirty
    Dirty -->|"No"| Home
    Dirty -->|"Yes"| Prompt
    Prompt -->|"Cancel"| Editor
    Prompt -->|"Save or Discard, then requested action"| Editor
    Prompt -.->|"When requested destination is Home"| Home

    Pause -->|"Exit normal Play"| Home
    Pause -->|"Exit Test Play"| Editor
    Results -->|"Normal Play: Home"| Home
    Results -->|"Test Play: Return to Editor"| Editor
~~~

The diagram represents destinations, not individual implementation classes. **New/Open while editing** returns to Editor with a new/opened draft after the save/discard confirmation. Closing the window after dirty-state resolution exits the application, not necessarily Home. On validation error, the return target is the **originating** screen; the diagram's two Error exits represent that conditional return.

**Play-origin model (proposed):** When starting a session record either *normal* (local chart from Home) or *test* (validated snapshot from Editor). This origin affects **navigation only**, never movement, timing, or scoring.

## 3. Home / local chart selection

### 3.1 Compact wireframe

~~~text
+-----------------------------------------------------------+
| LOCUS                                            [Settings]|
|                                                           |
| LOCAL CHARTS                                              |
|  [Open Chart File...]                                     |
|  +-----------------------------------------------------+  |
|  | Example                   BPM 120      [Play] [Edit]|  |
|  | Another Local Chart       BPM 160      [Play] [Edit]|  |
|  +-----------------------------------------------------+  |
|                                                           |
|  [New Chart]                                    [Exit]     |
|                                                           |
|  Status / load error, if any                              |
+-----------------------------------------------------------+
~~~

### 3.2 Actions and state

| Action / location | Behavior |
| --- | --- |
| **Open Chart File...** (above list) | Choose an existing local .locus.json file; parse/validate it and show its title or a specific error. No remote lookup. |
| **Play** (selected chart row) | Validate chart and audio, begin a new Count-in and judgement session. |
| **Edit** (selected chart row) | Load into Editor as a ChartDraft with its local path/audio reference. |
| **New Chart** (bottom left) | Open Editor with a fresh draft and no unsaved prior content. |
| **Settings** (top right) | Open lightweight preferences. |
| **Exit** (bottom right / window close) | Close app cleanly, subject to dirty editor handling only if Editor is currently active. |

**MVP list proposal:** It is acceptable for Home to show just the selected/recently opened local chart and an Open File action; **do not** build a chart database, file indexer, or persistent playlist solely for this screen. An empty state must still offer **Open Chart File** and **New Chart**.

## 4. Game: Play and Count-in

### 4.1 Playing screen wireframe

~~~text
+-----------------------------------------------------------+
| [Pause]    Example                            Combo: 012 |
|                                                           |
|                    FUTURE PATH                            |
|              o -------- o      o       o                 |
|              o          o ---- o       o                 |
|              o          @      o       o                 |
|              o          o      o       o                 |
|                                                           |
|                     PERFECT                               |
|                                                           |
|  Left action: [key]       Right action: [key]              |
+-----------------------------------------------------------+
~~~

The marks above are a **layout schematic**, not a precise example of legal connected movement. The actual marker, four rows of points, seam geometry, and path must come from the shared Route sampler.

### 4.2 Actions

| Location | Action | Result |
| --- | --- | --- |
| **Pause** (top left or designated pause key) | Pause | Freeze audio/music time and judgement expiration; show Pause overlay. |
| **Game field** (center) | No direct manipulation | Visualizes 4×4 points, the one marker, and the author-defined future route. |
| **Mapped Left / Right keys** | Key-down turn attempt | Send timestamped action to Judgement; **never steer** the logical marker. |
| **Judgement label** (below grid) | Read-only status | Show latest Perfect/Good/Bad/Miss without obscuring the route. |
| **Combo** (top right) | Read-only status | Show current combo if included in v0.1; no interaction. |
| **Count-in** (same screen, before Play) | Read-only countdown | Show a short count-in from music time. The first chart beat and eligible beat-0 turn retain their authored timestamps. |

**Count-in contract:** No animation loop may invent chart time. It is a transport/preroll phase; when an actual turn window becomes active during preroll it is judged according to TIMING.md. Countdown design and duration are handled in P-10, not by A-06.

**Wrap contract:** Only an edge exit and opposite edge entry can be drawn. The game field must reserve half-point spacing beyond the outermost dots for seam pieces as specified in WRAP.md.

### 4.3 Pause overlay

~~~text
+-----------------------------------------------------------+
|                [ PAUSED ]                                 |
|                                                           |
|                       [Resume]                            |
|                       [Retry]                             |
|                       [Exit to Home]                      |
|          (or [Return to Editor] in Test Play)             |
+-----------------------------------------------------------+
~~~

**Resume** restores the same audio position and unresolved judgements. **Retry** resets the song, route, grade state, and count-in. **Exit** ends the current session, not the local chart. In test-play, Exit returns to the preserved editor draft rather than dropping work. Losing keyboard focus may trigger pause (proposed safe behavior from TIMING.md). A user cannot arbitrarily seek while scoring in Play.

## 5. Results

### 5.1 Layout

~~~text
+-----------------------------------------------------------+
| RESULTS                     Example                       |
|                                                           |
|       Perfect    14       Good     3                     |
|       Bad         2       Miss     1                     |
|       Max combo  12                                      |
|                                                           |
|         [Retry]      [Home]                              |
|   (Test Play: [Retry]   [Return to Editor])              |
+-----------------------------------------------------------+
~~~

The numbers are mock placeholders, not a scoring formula. Results show the final grade counts (and combo if implemented) from **Judgement**, never from rendered path positions.

**Normal Play:** Retry restarts the selected chart; Home returns to local chart selection. **Test Play:** Retry uses the same immutable snapshot; Return to Editor restores the draft, timeline position, zoom, selection, and unsaved status. The user must not be accidentally sent Home after a test.

Do not introduce online submission, ranks, leaderboards, or accounts. An exact accuracy/score formula is open until the judgement/UI roadmap tasks.

## 6. Editor: timeline and 4×4 preview together

### 6.1 Proposed desktop wireframe

~~~text
+----------------------------------------------------------------------------+
| LOCUS / EDITOR       New Chart*       [Save] [Save As] [Test Play] [Home]   |
|----------------------------------------------------------------------------|
| Title: [My Chart          ]  Audio: [audio/song.wav] [Choose...]          |
| BPM: [120]  Offset (s): [+0.250]    Preview lead (beats): [4.0]           |
|----------------------------------------------------------------------------|
| EDIT CONTROLS                | GRID PREVIEW 4x4                           |
| Start cell: [Pick on grid]   |                                           |
| Heading: [Up v]             |       o      o      o      o              |
| Selected turn: [Left/Right]  |       o      o      o      o              |
| [Add Left] [Add Right]       |       o      @---o      o                 |
| [Delete] [Undo] [Redo]      |       o      o      o      o              |
|                              |  Current beat: [8.00]                     |
|                              |  No gameplay judgement in preview         |
|----------------------------------------------------------------------------|
| TIMELINE                          [Play / Pause] [Stop]                    |
| Beat ruler:  0     1     2     3     4     5     6    ...                |
|             |-----|-----|-----|-----|-----|-----|----...                |
| TURN LANE:  [L]                     [R]           [L]                    |
|                      ^ Playhead (draggable)                               |
| [Zoom -] [Zoom +]   [Scroll <] [Scroll >]      Snap: quarter beats        |
|----------------------------------------------------------------------------|
| Validation: Ready / "turns[...].beat: duplicate event"                    |
+----------------------------------------------------------------------------+
~~~

**Legend:** \`*\` indicates unsaved changes. \`@\` indicates the marker in this conceptual mock-up. The apparent route line is illustrative, not a promise of an exact valid step sequence. Labels may be shortened and the two upper work panels may be rearranged after the UI toolkit and window dimensions are tested.

**Must remain simultaneously visible:** **timeline** (with event markers + movable playhead) and **grid preview** (4×4 points, marker, future route). Do not put them on exclusive tabs. Controls may use a collapsible sidebar if the window is constrained, but both core views must remain visible.

### 6.2 Authoring actions and precise UI location

| Action | Location | Result |
| --- | --- | --- |
| New chart | Home **New Chart**; Editor top bar **New** (when implemented) | Start a blank ChartDraft, asking Save/Discard/Cancel first if dirty. |
| Open chart | Home **Open Chart File** / row **Edit**; Editor **Open** (planned) | Load a chart as ChartDraft and replace previous draft only after dirty confirmation. |
| Edit title | Editor metadata row **Title** | Set required local title; invalid empty title flagged. |
| Choose local audio | Editor metadata row **Choose...** | Select readable local file; save must use a chart-relative audioPath under the chart folder. Explicit copy/relocation confirmation if needed; never silently copy/upload. |
| Set BPM | Editor metadata row **BPM** | Change tempo conversion and timeline scale; no event beat numbers silently change. |
| Set audio offset | Editor metadata row **Offset (s)** | Change signed chart-to-audio alignment; zero beat meaning stays fixed. |
| Set preview lead | Editor metadata row **Preview lead (beats)** | Change how far ahead the route appears; not a gameplay event. |
| Pick start cell | **Grid preview**, in start-point selection mode | Set start x/y in range 0..3; update route sample. |
| Pick initial heading | **Heading** control in left side panel | Choose Up/Right/Down/Left; routes use relative turns. |
| Add Left / Right event | Left panel **Add Left** / **Add Right** at current playhead | Insert one event on an allowed integer beat. |
| Select event | Click an **L/R marker** in timeline turn lane | Highlight event, populate selected turn controls. |
| Move event | Drag selected timeline marker to beat tick | Change event beat, snapping to integer quarter-note boundaries; block duplicate beats. |
| Change Left ↔ Right | **Selected turn** control in left panel | Change direction without changing beat; route preview recomputes. |
| Delete event | Left panel **Delete** or contextual Delete key | Remove currently selected event; no effect if nothing selected. |
| Undo / Redo | Left panel **Undo / Redo** (shortcuts may be added) | Restore/reapply editing operations without affecting route implementation. |
| Scrub to a time | Drag **timeline playhead** | Seek editor audio/time and recalculate grid preview; **no gameplay grades**. |
| Preview audio | Timeline **Play/Pause** and **Stop** | Listen and watch the timeline cursor, marker, and future route together. |
| Pan / zoom timeline | Bottom **Scroll** and **Zoom** controls | Change visible range/scale without changing stored beat indices. |
| Inspect validation | Persistent bottom **Validation** strip | Show field/beat errors. Invalid drafts remain editable and cannot silently become playable. |
| Save / Save As | Editor top bar | Validate and write .locus.json locally; preserve draft if save fails. |
| Test Play | Editor top bar | Validate draft + audio, start a normal scoring Play session from a **read-only snapshot**. Do not require prior Save. |
| Return Home | Editor top bar **Home** | Save/Discard/Cancel if dirty; no accidental loss of draft. |

**Single source of truth:** The editor may alter **ChartDraft**, but every route shown in preview and test-play is calculated by the production **Route** code. Editor timeline playback uses the same SongClock timing model as Play; it never awards grades while scrubbing.

**No editor boundary-mode control exists:** Wrap is fixed for MVP. No adjustable grid size or tempo-change lanes exist.

### 6.3 Audio path and save flow

A v1 chart requires a **local, chart-relative audioPath**. Selecting an audio file outside the eventual chart folder is a normal UX case but cannot be silently written into an invalid chart.

**Proposed simple workflow:**

1. **Choose...** selects a local audio source; Editor shows the selected source and whether it is currently portable relative to the draft's save location.
2. On **Save / Save As**, if audio already resides inside the chosen chart folder, store its normalized relative path.
3. If it lives elsewhere, show a choice: **Copy audio into chart folder** (explicit local copy, check for name conflicts) or **Cancel** to pick another folder/file. Never upload or silently overwrite music.
4. If copying or chart write fails, show an actionable error, preserve existing source files and the dirty draft, and do not claim success.
5. **Test Play** requires a valid draft and an available audio source; it may use the selected source before a final Save, provided no invalid path is written into a chart file.

Precise file-dialog, file-copy, and atomic-write implementation is deferred to the editor/storage roadmap tasks. The UI should not impose an audio-library backend decision in A-06.

### 6.4 Editor validation and unsaved changes

- When a control contains invalid content (for example an incomplete BPM), keep the entered value in the draft/form and show a nearby diagnostic. Do not silently coerce it to 120.
- An event on an occupied beat is rejected or visibly flagged; no silent overwrite or automatic event reordering.
- **Save** and **Test Play** are disabled or fail with an actionable explanation if the chart is invalid; a temporarily invalid editor state is allowed.
- A draft remains dirty after an unsuccessful save or file-copy attempt.
- On New, Open, Home, or window close with unsaved changes, show **Save / Discard / Cancel**. Cancel returns to the exact draft state; Save proceeds only after a successful write.
- When returning from Test Play, preserve the source draft **and** its selection/playhead/zoom/dirty state. Test Play never edits the draft or changes its audio reference.

## 7. Settings (small, local, and optional in scope)

~~~text
+-----------------------------------------------------------+
| SETTINGS                                            [Back] |
| Music volume:      [-----------o]                          |
| Turn Left key:     [binding]                               |
| Turn Right key:    [binding]                               |
| Timing adjustment: [value] [Reset]                         |
| Reduced effects:   [on/off]                                |
+-----------------------------------------------------------+
~~~

These are **planned interaction locations**, not proof that every item is implemented in the initial prototype. The roadmap owns their implementation (particularly Q-01 through Q-04 and Q-09). Timing adjustment belongs to **player preferences**, not chart offsetSeconds. No account settings or cloud sync.

## 8. Navigation and input policy

| Context | Input focus / exit behavior |
| --- | --- |
| Home | Buttons are keyboard-focusable; chart row actions are accessible without requiring a network connection. |
| Editor text input | Typing in Title/BPM/Offset does **not** trigger timeline shortcuts or Left/Right gameplay actions. |
| Editor timeline | Click/drag modifies selected event/playhead; Enter/Test Play is explicitly requested, not automatic on an event selection. |
| Editor grid | Click selects the initial point **only when start-point selection mode is active**; ordinary preview cannot accidentally edit data. |
| Active Play | Only current turn bindings are gameplay input; Pause is distinct from Left/Right. |
| Pause | Turn key presses during pause are ignored; resume requires a fresh key-down after release per TIMING.md. |
| Results | Input no longer changes scores. Retry and the appropriate return button are explicit. |
| Focus lost | **Proposed:** auto-pause Play. Editor draft is not discarded. |

**Proposed Escape behavior:** Escape in Play opens Pause; in Pause it resumes; in Editor it cancels the current selection/drag or asks before leaving a dirty draft (rather than silently exiting); in Settings it returns Home. Exact bindings are frozen in later UI/input tasks.

## 9. Critical user journeys

### J-01 — Play an existing local chart

Home → **Open Chart File...** → select valid .locus.json → chart appears in local list → **Play** → Count-in → Play: Active → audio end → Results → Home.

**Failure variant:** Missing local audio or invalid schema → clear message; remain on Home with the chosen file path available for correction. Do not enter silent game session.

### J-02 — Make, save, and replay a chart

Home → **New Chart** → Editor → **Choose audio** → set **BPM / Offset** → select **Start cell and Heading** → add **Left / Right** events on timeline → scrub and preview grid → **Save As** locally → **Test Play** → Results → **Return to Editor** → Home after dirty-state handling.

**Expected:** At the same chart beat, the Editor and Play render the same marker position/heading and future route. No need to hand-edit JSON.

### J-03 — Test-play an unsaved draft without losing it

Editor with unsaved edits → **Test Play** (valid snapshot and available audio) → Count-in → Play → **Pause** → **Return to Editor**.

**Expected:** Draft title, audio selection, turn events, playhead, timeline zoom, selection, and dirty flag are all intact. The test-play score is not imported into the editor draft.

### J-04 — Cancel leaving a dirty draft

Editor with unsaved changes → **Home** → Save/Discard/Cancel dialog → **Cancel** → unchanged Editor state.

**Expected:** No file write or draft reset occurred.

### J-05 — Retry after a Miss

Home → Play valid chart → press wrong turn → visible **Miss**, marker still follows authored route → Results → **Retry**.

**Expected:** Fresh Count-in, new judgement session, same chart route, no retained grade/combo values.

## 10. Screen-level MVP acceptance checks

- [x] Home shows where to **open**, **play**, **edit**, and **create** local charts; no online entry point.
- [x] Play, Pause, Retry, Results, and both normal/test-play return destinations are documented.
- [x] Editor wireframe displays **timeline and 4×4 preview simultaneously**.
- [x] Every core editor action has a named screen control, timeline interaction, or grid interaction.
- [x] The local audio-path, validation-error, and dirty-draft flows are explicit.
- [x] UI behavior does not introduce its own musical clock, routing algorithm, or judgement rules.
- [x] No Bounce toggle, alternate grid-size control, account, upload, marketplace, or leaderboard is proposed for MVP.

These checks indicate **documentation completeness only**, not a built UI or a completed usability test.

## 11. Deferred decisions

| Topic | Roadmap stage / reason |
| --- | --- |
| Window size, exact layout spacing and typography | A-09 stack decision and E-01/P-01 implementation |
| Real widget toolkit and focus manager | A-09 technical evaluation |
| Default key bindings and shortcut conflict handling | T-10, I-16, Q-02 |
| Count-in duration and visuals | P-10 |
| Numerical timing thresholds and result score formula | T-11, T-16, P-14 |
| Chart list persistence / recent-file conveniences | Foundation and Q stages; no mandatory database |
| Detailed waveform or advanced editor conveniences | Not required by the initial schema; approve only if useful |
| Additional colors, animations, and seam cues | P stage and gameplay human gate |

**A-06 deliverable:** This single English Markdown specification. Do not generate UI code, art assets, game screens, or unrelated roadmap tasks as part of A-06.
