# LOCUS

## Four-beat forward-only route preview and uncluttered gameplay HUD

In normal **PLAY** and editor **TEST**, the red trajectory shows a full
**four-beat measure ahead of the player** (not just a single beat), so turns
remain visible with enough time to react. Directly in front of the fixed
centre marker it is bright and wide; farther along the next four beats it
becomes thinner and more transparent, reaching zero at the end of the
measure. **Nothing remains behind the player**. Small sub-beat line slices
are clipped at the current fractional music beat, so entire route segments
do not pop in/out. Upcoming red turn points use the same full-measure fade
window. The ROUTE editor's existing authoring preview is unaffected.

PLAY uses a **540×540 square** centered on the original 960×540 virtual screen,
from x=210 to x=750. The stationary player is at the exact center. Only the
trajectory, turn targets, player and effects **inside the square** are rendered;
the route continues infinitely beyond it but stays invisible there. A thin
**white (by default) square border** can be recolored or made translucent under
SKIN → COLORS → FIELD BORDER. The Perfect/Good/Bad/Miss HUD is in the left
sidebar and the latest hit/song information is in the right sidebar; it never
covers the playing square. Camera zoom remains configurable in the skin,
and with default zoom a size-8 chart shows eight world cells across 540px.
There is no gameplay grid or grid-node clutter.

## Infinite world and stationary player (current gameplay)

**No Wrap, teleportation, or gameplay grid in PLAY/TEST.** All eight
headings advance the authored route on an unbounded 2D plane; a diagonal
moves one world unit along each active axis per beat. A camera translates the
red path so the character remains **fixed at the centre of the playable area**
while the world moves underneath it. The camera never rotates and its position
comes directly from the *fractional music beat*, not from a delayed
screen-following animation. Turns on 45°, 90°, or 135° therefore change the
world's scroll direction, but cannot cause a camera position jump.

In normal **PLAY** and **TEST** from the editor, **there are no grey grid
lines or grid-node dots**. Only the red trajectory, large red turn junction
points, centred player and the gameplay hit statistics appear. The grid
remains visible in the **ROUTE editor**, scrolling below the preview so
even long songs are still editable. Its persisted `grid.width/height` values
control the **square field's cell count** (size 8 by default), not travel
distance, collisions or physical world boundaries.
Existing version-1 charts and timing/turn event definitions are unchanged,
although paths previously wrapping now extend naturally beyond the former
grid instead.


## Gameplay HUD and settings layout

Normal **PLAY** now focuses on the enlarged route field, moving marker and
live **Perfect / Good / Bad / Miss** counters (plus the latest grade). It
does not show chart editing hints, BPM/beat/time readouts, input tutorials,
technical playback status, or debug navigation labels. Pause still shows
resume/retry/home controls, and Results retains the separate result summary.
The **editor's** detailed workspace remains unchanged.

The SETTINGS view has aligned two-column panels for audio, hit flashes,
diagonal chord timing, fullscreen and gameplay key bindings. Messages appear
beneath the panels, so they do not overlap any switches. The ordinary
`build_and_run.bat` launcher no longer enables `--debug-screens`. Debug
F1–F4 screen shortcuts remain available **only** when launching the
executable explicitly with `--debug-screens`. Pressing **F11** still changes
fullscreen without an intrusive floating banner.


## Home, song library and settings (current navigation)

The starting screen now has four large clickable buttons: **PLAY**, **EDIT**,
**SKIN**, and **SETTINGS** (keyboard: **P/Enter**, **E**, **K**, and **S**).
**PLAY** opens the osu!-style song browser: the left column lists installed
songs, and the right column lists all the separate charts/difficulties attached
to the selected audio. Select songs with **Up/Down** or the mouse wheel, select
difficulties with **Left/Right**, then use **Enter / PLAY**. The built-in click
demo is permanently available as the first item. **H/Escape** goes back.

**Import and permanent storage:** On PLAY choose **IMPORT (C)** to select an
existing `.locus.json` with its corresponding WAV/MP3. LOCUS copies the
audio and chart into **`%LOCALAPPDATA%\\LOCUS\\Songs`** (separate
per-song folders, with multiple difficulties sharing a song) and keeps the
parsed chart catalog in memory. The original files are not moved or deleted.
The same original chart can be reimported without duplicating its difficulty.
Installed songs remain available after closing or rebuilding the game, and
playing a different difficulty on the same song reuses the decoded audio.
Audio files are loaded into memory *only when needed* rather than loading
every WAV/MP3 in the library at launch. **RESCAN (F5)** picks up files placed
directly in the Songs tree. **Ctrl+S in the existing editor** still saves where
you choose, and now also installs/updates a library copy. To change an
installed chart, choose it in PLAY and press **E / EDIT CHART**; the unchanged
editor opens that difficulty.

**SKIN** is intentionally only a placeholder for later work. **SETTINGS**
already works and saves to **`%LOCALAPPDATA%\\LOCUS\\settings.json`**:
master music volume (built-in demo, songs and editor preview), the on/off hit
flash, fullscreen, and the two-key diagonal chord window (40–160 ms). The
**KEY BINDINGS** section changes four gameplay directions and the
**Pause/Retry/Home** actions. Click an action, then press the new key; Escape
cancels and conflicting or reserved keys are rejected. Defaults are
J/K/F/D for Up/Right/Down/Left, Space, R, H. All four **directional bindings are
shared with ROUTE editor note placement**, including diagonal chords.
Saved settings apply after relaunch.

LOCUS is a Windows rhythm game. **The first playable 16-second demo is complete and owner-approved (2026-11-05).** It includes nine F/J turns, music-driven movement, smooth route reveal, fullscreen, pause/retry and Results. Development now focuses on **local songs and a practical custom-chart editor**, not additional infrastructure or benchmarks. Two Windows applications remain available: **LOCUS** (the square-field rhythm game, local WAV/MP3 playback, JSON charts and an editor with JSON save/open/test-play, launched by double-clicking `build_and_run.bat`) and the optional **A-07 audio/input diagnostic** (BAT `audio` mode).

[Game Design Document](mvp/GDD.md) · **[Short playable-prototype roadmap](mvp/ROADMAP.md)** · [Audio Evaluation](mvp/architecture/STACK_EVALUATION.md) · [Stack Decision](mvp/architecture/STACK_DECISION.md)

## Successful-turn player flash

During scored Play (demo and custom charts), a correct **Perfect / Good / Bad**
arrow judgement triggers a tiny 180 ms halo and expanding gold ring **on the
moving marker**. Perfect is brightest, Good is softer, and Bad is subtle.
Wrong-direction hits and Misses do not flash. The effect uses wall-clock time,
so practicing at 10% playback speed does not make it linger; pause, restart
and returning Home clear it. This is visual feedback only and does not alter
route movement, chart JSON, or judgement windows.

## Current architecture (November 2026)

LOCUS retains two build layers: **`locus_core`** (pure route, timing, grading,
chart format and editor model, no raylib/Windows calls) and **`locus`** (audio,
file adapters, application and rendering). Responsibilities now have smaller
boundaries: `src/app/game_session.*` handles scored playback, arrow inputs,
pausing and retry; `src/app/route_renderer.*` shares the red one-bar route
and junction effects between Play and Editor; `chart_editor_actions.cpp` owns
keyboard/mouse commands while `chart_editor_ui.cpp` draws the editor, using
`chart_editor_layout.h` for common hitboxes. The route sampler skips entire
straight runs instead of recomputing every beat from song start on each sample.
Manual `SetLength/ChangeLength` are removed; music duration/BPM/offset
determine the timeline. The v1 JSON reader still accepts legacy `lengthBeats`.
See [current dependency boundaries](mvp/architecture/BOUNDARIES.md).

## Next: play and make custom charts

The owner accepted the first playable prototype after a complete Windows run with **7 Perfect, 2 Good, 0 Bad and 0 Miss** (9/9 turns). The current [short roadmap](mvp/ROADMAP.md) has five outcome-based milestones: local WAV/MP3 playback, external JSON chart loading, basic chart editor, save/reopen/test-play, and Windows usability/distribution. **M-01–M-04 are implemented: WAV/MP3 listening, JSON gameplay, chart editing, saving/reopening and immediate test-play. M-05 (Windows usability polish and distribution) is next.** There is no mandatory benchmarking, no ZIP requirement, and no repeated prototype approval gate.

## Save, reopen and test-play your chart (M-04)

Use the **E** editor on Home. For a new chart, press **O** *inside the editor* to choose your own local WAV/MP3. Build the route with the grid and timeline. Then:

| Shortcut | Action |
| --- | --- |
| **Ctrl+S** | Save the chart; the first save opens Windows Save As |
| **Ctrl+Shift+S** | Save As another `.locus.json` file |
| **Ctrl+O** | Open a saved chart directly into the editor |
| **Ctrl+N** | Start a fresh 32-beat chart |
| **T** | Test-play the edited route against the attached song, even before saving |
| **H** | Home; return with E to keep editing |

**Default JSON filename:** On first save or Save As, LOCUS suggests the edited **chart title** plus `.locus.json` (e.g. title `rip me apart` → `rip me apart.locus.json`). Characters not allowed in Windows filenames are replaced. The Save As dialog still lets you change the name. After a file has been saved, ordinary **Ctrl+S updates that same file** even if you later rename the chart title; use **Ctrl+Shift+S** to choose a new filename. Saving keeps your selected beat and Undo/Redo history; it does **not** jump the timeline back to the beginning.

**Where should the JSON go?** Save in the **same folder as your WAV/MP3** (or one of its parent folders). The game writes a safe *relative audioPath*, so a chart and its song can move together. Saved v2 JSON uses **integer `offsetMs`** and **absolute integer `timeMs` for every note**, so changing BPM or snap density does not rewrite their audio timestamps. Old v1 `beat`-based charts still open and migrate on save. The timeline covers the full audio duration automatically; older files containing `lengthBeats` are still readable but the field is ignored and no longer saved.

**Unsaved changes:** The editor visibly marks changed drafts. Before discarding them with Home, Ctrl+N or Ctrl+O, LOCUS asks you to confirm: **Y** discards, **N** cancels. You can use Ctrl+S first. Save uses a temporary file plus replacement so a failed write should not destroy the previous chart. A missing song, out-of-range turn, invalid path or too-long timeline shows an error rather than silently writing broken game data. Force quitting can still lose unsaved edits.

**Try the full round trip:** E → O to choose a song → add notes with DFJK (or your rebound gameplay direction keys) → Ctrl+S to save `MyChart.locus.json` beside the song → Ctrl+O to reopen it → T to test → song finishes → Results. Go Home and E to continue editing the chart.

Windows CI verifies file serialization, actual disk save/reopen, unchanged gameplay tests and compiler linkage without a graphical desktop. The appearance of Save/Open dialogs and subjective rhythm synchronization on your monitor remain for manual playtesting.

## Edit and preview a chart inside LOCUS (M-05 usability revision)

After owner playtesting revealed the previous editor was not usable enough, its controls were reworked. From **Home**, press **E**.

**Load music visibly:** Click **LOAD SONG** at the top (or press **O**) to attach WAV/MP3. The selected filename appears directly under the buttons. To edit an existing chart instead, press **Ctrl+O** in Editor; its song opens with it.

**Editor workspace tabs:** **ROUTE** is the default editor workspace: a centered preview of the moving route and the nine compass direction buttons. **AUDIO** holds chart title, BPM and millisecond offset. Practice-speed buttons **100/75/50/25/10%** are on **ROUTE** next to the large grid; keys 1–5 still work across the editor. **FIELD** holds one square visibility size (2–16, default 8) and starting heading; the start position is automatic. Its song timeline is read-only and automatically follows song duration, BPM and offset. The selected song, transport buttons and timeline are visible on **all three tabs**; switching tabs does not reset audio or change the chart.

**Type values rather than clicking +/-:** In **AUDIO**, click **CHART TITLE**, **BPM** or **OFFSET IN MILLISECONDS**; in **FIELD**, click **VISIBLE SQUARE SIDE** (the start coordinates and Total Beats are no longer editable). First typed character replaces the value; Enter confirms, Esc cancels, Ctrl+V pastes. BPM accepts decimal point or comma and any finite positive BPM, including 400+. Offset is a signed **whole number of milliseconds** (e.g. `-175`); fractions are rejected. Enter only **one whole number**, for example `8` means an **8×8 square**; independent width and height inputs like `8/8` are no longer accepted. Valid sizes are **2 through 16**. Changing the size automatically recenters the internal start origin and does not alter note timestamps. The game always clips its route into a central **540×540 pixel square**, and the chosen size controls the world-cell density inside it. Click the starting-direction button or press **Q/E** to rotate heading; clicking the ROUTE grid no longer changes the start point.

**Continuous osu-style timeline:** The bottom row is now a scrolling beat ruler, **not pages of 16 squares**. A thick gold playhead stays fixed **one third of the way from the left edge**, so you can see some previous beats and more upcoming beats; the ruler and authored Left/Right turn markers glide underneath at real fractional-beat speed, with a longer tick every four beats. Click a tick to select it or seek there on the next Space. Use **←/→** (or PageUp/PageDown) to select one visible grid line (a whole beat at snap 1/1, a fraction at finer snap), **Ctrl+←/→** or **Ctrl+PageUp/PageDown** to skip four musical beats (one 4/4 bar); **hold Ctrl+←/→** or **Ctrl+PageUp/PageDown** to scroll repeatedly with acceleration, and **Shift+←/→** or **Shift+PageUp/PageDown** to move a turn without overwriting another event. **- / = (plus)** or **[ / ]** zoom the ruler out/in; you can also use **Ctrl+mouse wheel** or the visible **-/+** buttons. Zoom changes only visual spacing. The separate **LESS / MORE** buttons by the timeline (or **↓/↑**, or **comma / period**) change the note-placement grid through **1/1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12, 1/16, 1/24** without changing BPM or zoom. Existing notes remain at their exact whole-millisecond audio times. Lowering snap density hides intermediate lines but **never removes finer notes**. Navigation clamps to the start/end and does not change chart data.

**Group editing without leaving ROUTE:** On the bottom timeline, **drag with the left mouse button** across beats to select an inclusive range of turn notes (highlighted purple); keeping the mouse held at or beyond either edge **autoscrolls the timeline and extends selection** across as much of the chart as needed; **Shift+click** selects a range from the current beat. Single-click the timeline to clear the range and select a new destination. Use **Ctrl+C** to copy turns in the range, **Ctrl+X** to cut, and **Ctrl+V** to paste them starting at the currently selected beat. The copied pattern is **automatically cropped from its first real note to its last real note**: empty ticks accidentally included before/after the notes are ignored, while blank beats **between** the notes are preserved as spacing. The selection highlight snaps to those note bounds when copied. Pasting starts at the selected beat, overwrites only turns at destination note beats, and keeps unrelated notes. **After a successful Ctrl+V or PASTE click, the entire pasted range stays highlighted**, ready to move, invert or copy again. A failed paste leaves the previous selection unchanged. **I** (or **FLIP L/R**) swaps all selected Left and Right turns. **Ctrl+Shift+←/→** or **Ctrl+Shift+PageUp/PageDown** moves a selected group by one snap subdivision; it refuses an off-chart shift or a collision with a note outside the selection, with no partial edits. **Delete** removes selected turns. The pattern buttons, **UNDO** and **REDO** are on the ROUTE tab, alongside the grid — **there is no separate PATTERNS tab**. **Ctrl+Z / Ctrl+Y** (or Ctrl+Shift+Z) undo/redo up to 64 editor changes, including pattern operations and regular field edits. Clipboard and group actions operate on turn events only, not audio. While a title, BPM, or other text field is active, these shortcuts do not edit the chart.

**Scored TEST from your current position:** Press **T** or click **TEST** in the editor to start scored gameplay from the currently selected beat. If the editor preview is playing or paused, it starts at the **exact fractional song position** where the preview was stopped (e.g. beat 26.40, not 26.00). Earlier turns are skipped without MISSES, but events from that position onward are graded normally. **R** restarts the scored practice from that same position. Loading a chart from Home still starts at the song beginning.

**Live in-editor preview from any beat:** Click a beat on the bottom timeline (or use **PageUp/PageDown**) and press **Space** or click **PLAY**. The real WAV/MP3 stream and moving marker start at that beat, using `offsetMs / 1000 + beat * 60 / BPM` as the song position. For example, at 120 BPM and offset `173765` ms, beat 0 corresponds to **2:53.765** and beat 16 to **3:01.765**. The red one-bar line, character marker and continuous timeline ruler follow the audible music. Space pauses/resumes and **R** restarts from the originally selected preview beat. When stopped, select another beat tick on the ruler to inspect or play from it. The editor includes beats before positive offset (negative beat indices such as -5, -4...), and dynamically recalculates all boundaries when BPM or offset changes. A chosen beat before physical audio starts uses silent preroll and then starts the audio at time zero. **T** remains a separate scored playthrough using the same DFJK/default gameplay bindings. In ROUTE, the **four direction keys are exactly those configured for gameplay in SETTINGS** (default: **D=Left, F=Down, J=Up, K=Right**). Press adjacent game-direction keys together for a diagonal (default: **J+K = Up-Right**); a single key waits the same **40–160 ms chord window** configured for gameplay, so a diagonal does not insert an extra single note. **←/→**, PageUp/PageDown and the mouse wheel navigate grid lines by default; **↑/↓** increase/decrease the snap subdivision density. The arrow keys also support bar-jumps, moving notes and group shifts using Ctrl/Shift modifiers. **Ctrl+←/→** still jumps bars and **Shift+←/→** still moves notes; modifiers never place a note. Backspace/Delete removes turns (Delete is a placement key instead if rebound as a game direction). Other editor hotkeys also yield to a rebound direction key.

**Automatic start position:** ROUTE shows a scrolling square lattice centred on the moving marker. There is **no editable starting X/Y coordinate** and clicking the grid does not move the authored origin. A new chart starts with a centered internal origin for its default 8×8 view. FIELD keeps the initial **heading** control (Q/E rotates by 45°), which affects route directions rather than audio timestamps. Previously saved chart origins remain compatible, but changing the square size recenters the hidden origin.

**Eight-way movement and chord input:** Routes can now travel in any of **eight compass directions**, including diagonals. Every beat moves one logical cell along each active axis (diagonal = one cell horizontally and one vertically per beat); the wrap seam is clipped correctly even at corners. Gameplay controls are **absolute configurable directions** (default DFJK): press a single arrow for an orthogonal outgoing step, or **hold/press two arrows together** for a diagonal, e.g. **J + K = UP-RIGHT**. Two key-downs within **85 ms** count as one chord; the first half of an expected diagonal is not scored incorrectly before the second arrives. Chords are graded once at the later key-down. Wrong chords, straight-on or opposite directions cannot score a correct turn; missing/wrong input never steers the automatic route. Existing charts remain valid; arrow keys can be rebound to gameplay directions if desired.

**Editing diagonals on ROUTE:** To the right of the field is a **3×3 direction pad**: `UL U UR / L DEL R / DL D DR`. Click the desired **outgoing heading** at the selected beat. For example, when coming from Right, choose UR to create a Left 45° turn, then choose U from UR to create another Left 45°. Click the straight-ahead direction or DEL to remove a note; a direct 180° reversal is not allowed. The shortcut labels on **all nine direction-pad buttons reflect the current gameplay binds**, including diagonals; the middle **Backspace** button remains a delete action. Rebinding Up/Right/Down/Left in SETTINGS updates both gameplay and editor immediately, without modifying saved charts. Keys select **absolute outgoing headings**, not fixed relative angles; turn magnitude depends on the incoming direction. **Q/E** rotate the initial heading **45°** at a time, allowing diagonal starts. Group copy/paste, inversion, shifting and Undo/Redo work on all six turn types; inversion preserves the angle. The timeline distinguishes L45/L90/L135 from R45/R90/R135, with teal/violet backgrounds.

**Chart compatibility:** The writer now emits **`formatVersion: 2`**. Each turn has **`timeMs`**, an absolute integer timestamp from the audio file's beginning, rather than a `beat` index; **`offsetMs`** is also an integer. `bpm`, `ticksPerBeat`, and `snapDivision` define the editor's musical ruler and snap grid, not a different audio timestamp. Version 1 charts remain readable and are upgraded when saved. All six 45°/90°/135° turn types and eight initial headings remain supported.

**One-bar red route:** Only **four beats total** are visible (the current step plus up to three future steps). Red line opacity rises smoothly as the next parts of the path approach, over a four-beat window. Turn junction dots start glowing red **as soon as the preview line reaches their cell**, before the character arrives. They are shown **only while that junction remains in the visible one-bar route**, so old dots do not accumulate across the board. Full, semi and combined red styles remain defined in `src/app/visual_style.h` for later skins.

**Slow practice speeds:** On **ROUTE**, click the vertically stacked speed buttons **100%, 75%, 50%, 25%, 10%**, or press **1–5** respectively, in Editor preview or during custom-song Play. Audio and route time share the selected speed. **raylib's pitch adjustment changes musical pitch as well as speed**; this is not pitch-preserving time stretch, and extreme slowdowns may sound different. A speed change does not edit the saved chart or its original BPM.

**Future-route contrast:** The next step is now a bright, opaque blue stroke with a dark outline, both in the editor and during gameplay. Shared values live in `src/app/visual_style.h`, the first separation between presentation and route logic; this is groundwork for future skin colors/layouts, **not** an editable skin system yet. See [visual customization architecture](mvp/architecture/VISUAL_SKINS.md).

**Save and test:** Ctrl+S saves; Ctrl+Shift+S uses Save As; Ctrl+N starts a fresh chart; **T** runs a normal scored playthrough under the attached song; **H** returns Home. Unsaved changes retain the earlier Y/N confirmation. Editing a numeric field prevents conflicting game shortcuts until Enter/Esc. The [full save/reopen instructions](#save-reopen-and-test-play-your-chart-m-04) remain valid.

CI tests chart editing, speed-adjusted/pause-resume song clocks, JSON round trips and Windows compilation. The visual editor and 10% audio playback **still require actual Windows playtesting** before M-05 can be accepted.

## Load and play your own JSON chart (M-02)

On **Home**, press **C** to open a local `.locus.json` / `.json` chart. The chart must contain an `audioPath` pointing to a **WAV/MP3 in the chart folder or a subfolder**, plus its beat map. LOCUS loads the song, centers the marker inside the square viewport, judges **single and two-arrow direction inputs**, and shows the normal Results screen at song end. **Space** pauses/resumes both audio and chart, **R** restarts the selected chart, and **H** returns Home. From Results, **Enter/R** plays the same chart again. **Enter on Home always starts the built-in demo**; **O** remains the listening-only music picker.

**Try the included sample chart:** Copy [`mvp/examples/first-song.locus.json`](mvp/examples/first-song.locus.json) into a folder with your own MP3 (at least 16 seconds long), rename that file to `song.mp3`, then press **C** and choose `first-song.locus.json`. This sample is authored at **120 BPM**, so its F/J turns will align musically only if the chosen song actually matches 120 BPM and beat zero at its start. Press **E** on Home to open the in-game editor for BPM, millisecond offset, square size, starting heading and turns. Save with **Ctrl+S** or **Ctrl+Shift+S**, then test with **T**; no external JSON editor is needed. For another filename, change `audioPath` accordingly. No music is included in the repository or uploaded.

**Historical format v1 (readable; new saves are v2):** Required fields are `formatVersion: 1`, `title`, `audioPath`, `bpm`, **`offsetMs`**, `grid: {width,height}` (each 2..16; new square editor default is 8×8), `start: {x,y,heading}`, `previewLeadBeats`, and an ordered `turns` array. A previously saved chart with `offsetSeconds` also opens normally, but **exactly one** of the two offset fields may be present. Each event is `{"beat": 2, "turn": "right"}`, with unique, ascending integer beats (negative beats are possible before positive offset). Turn at beat zero takes effect **before** the outgoing step. `offsetMs` is the signed audio time in **milliseconds** at chart beat zero (e.g. `250` = `0.25` seconds): positive values delay the chart, negative values create silent preroll before the song starts. See [the original v1 format specification](mvp/architecture/CHART_FORMAT.md).

Malformed versions, unknown or duplicate fields, invalid beats, missing WAV/MP3 files, unsafe relative paths and songs too short for scheduled turns produce a readable error instead of starting a broken level. Junctions/symlinks that escape the chart folder are rejected. Playback remains **local**; no copying or uploading. Windows CI tests parsing, beat timing and compilation headlessly; the exact feel with your music must still be checked on a real Windows computer.

## Listen to your own WAV or MP3 (M-01)

**The original demo still works.** Launch LOCUS with `build_and_run.bat`. On **Home**, press **O** to open the native Windows music picker. Select a **WAV or MP3** from your computer. Playback starts on the separate **YOUR MUSIC / LISTEN MODE** screen; it shows the song name and a progress bar.

| Key | Local song action |
| --- | --- |
| **O** | Choose a WAV/MP3, including a replacement for the current song |
| **Space** | Pause or resume the local song |
| **R** | Replay from the beginning |
| **Enter** | Replay after the track has ended |
| **H** | Return Home (stops playback) |
| **F11** | Toggle fullscreen |
| **Esc** | Close the application |

To play the original rhythm demo instead, return **Home** and press **Enter**. **F/J turn judgements do not apply in Listen mode**; choose **C** on Home with a corresponding JSON chart to play a custom rhythm level. No chart is invented for a song selected with O.

The file remains local: the game does not upload it, copy it into the repository or send it anywhere. It uses an in-memory streaming decoder to handle Unicode file paths, including Russian filenames. The initial limit is **256 MiB per file**; very large WAVs can be converted to MP3, and unsupported/corrupt files produce a readable message. **Windows CI checks compilation and file-type handling, not physical sound output**; a normal Windows playback check is needed to confirm the file is audible on the owner's device.

## Windows: double-click to build and run

**Launcher:** [build_and_run.bat](build_and_run.bat) — a native **Windows batch file**. **No Git Bash, WSL, or Bash shell is required.**

### First-time setup

- Windows 10 or Windows 11. An audio output device is needed for the demo's audible 120 BPM click track and the optional A-07 diagnostic; the window still opens and explains the issue if audio output is unavailable.
- **Git for Windows**, **CMake 3.25+** (required by raylib 6.0's CMake source build), and **Visual Studio 2022 with C++ tools**. A free Visual Studio 2022 Build Tools installation with the *Desktop development with C++* workload and Windows SDK is sufficient; the full IDE is not required.
- Internet access for the first configuration: CMake downloads raylib 6.0 source.

**The BAT detects missing tools.** If Windows Package Manager (`winget`) is available, it offers to install Git, CMake, or Visual Studio 2022 Build Tools with your explicit consent. Installing Visual Studio C++ tools can download **several gigabytes**, may request administrator approval, and may require restarting the BAT afterward. The launcher never silently installs them.

If you have not cloned the repository, open **Command Prompt** in a folder and run once:

~~~bat
git clone https://github.com/VadimAlekseyevich/LOCUS.git
cd LOCUS
build_and_run.bat
~~~

If you already have the LOCUS repository, **double-click `build_and_run.bat` in its root directory**. You can also launch it from Windows Command Prompt.

**If your local BAT is an older A-07 version:** its first launch will update itself through Git, but may still open the old metronome because the currently running copy was created before the pull. **Close that test and double-click the BAT once more** to launch the new blank LOCUS window. This is an upgrade-only issue; future runs use the updated launcher.

### What the BAT does

1. Copies itself to a temporary location so a **Git update can safely replace the original BAT**.
2. Checks for local uncommitted files; runs `git pull --ff-only` only if safe. It does **not** delete or overwrite your changes.
3. Checks build prerequisites and asks before installing anything missing.
4. **By default:** configures the root production CMake project and builds `locus.exe` in `build-a07/foundation/Release/`.
5. Opens **LOCUS Home**. Press **Enter** to start the full moving-route demo with click track and F/J timing judgements. **Space** pauses/resumes, **R** retries, **H** returns Home, and **Esc/X** closes. Results appear automatically at the end. F1–F4 remain developer shortcuts.
6. Saves compile errors and window-open/close status to **`LOCUS-game-logs.zip`** if available; a ZIP is produced even when the build fails after log-folder creation.
7. **Optional `audio` mode:** runs the previously tested A-07 metronome, transport tests, key-press CSV, and its separate `LOCUS-a07-logs.zip` bundle.

All build output is kept locally and ignored by Git. Nothing is uploaded automatically.

### Launch modes

| Command | Behavior |
| --- | --- |
| `build_and_run.bat` | **Default:** pull, build and open playable LOCUS demo (Enter to start) |
| `build_and_run.bat run` | Same as double-click |
| `build_and_run.bat build` | Pull and compile the production game without launching a window |
| `build_and_run.bat game` | Open LOCUS with F1–F4 debug screen switching |
| `build_and_run.bat baseline` | Build LOCUS, measure startup and 12 seconds of GUI idle CPU/memory; then close with Esc/X |
| `build_and_run.bat audio` | Run the original A-07 metronome/audio-input experiment |
| `build_and_run.bat test` | Run A-07 WAV self-test without a graphical window |
| `build_and_run.bat smoke` | Run A-07 WAV and audio-device smoke tests |

**Advanced/CI only:** Set `LOCUS_SKIP_PULL=1` to skip Git updates or `LOCUS_NO_PAUSE=1` to avoid waiting for a key at the end. These are not needed for normal use.

### Original A-07 audio diagnostic (`build_and_run.bat audio`)

The application generates a **16-second 120 BPM** metronome (one click every half-second).

| Key | Action |
| --- | --- |
| A / D | Record Left/Right test presses with monotonic poll timestamps |
| Space / P | Pause and resume |
| J / L | Jump 2 seconds back / forward |
| R | Restart the metronome |
| Esc | Close |

Listen for an even beat. Tap A or D on ticks, pause for a few seconds and resume, then try seeking and restarting. After you exit, answer the optional questions in the console.

This is **not yet a playable rhythm game**. The raylib reported streaming position is **not** a calibrated audio-output timestamp; frame-polled keyboard events also have uncertainty. A successful demonstration helps evaluate the API but does not prove that end-to-end physical latency is low.

## Sending diagnostic logs to ChatGPT

When the BAT finishes, it prints the path to the archive:

~~~text
LOCUS\.locus-runs\game-YYYYMMDD-HHMMSS-12345\LOCUS-game-logs.zip
~~~

**If troubleshooting is needed, you can attach this ZIP to the ChatGPT conversation.** In default game mode, it contains Git/build logs and output from the window; in `audio` mode it also includes the audio/device logs and A/D timestamps. **No ZIP is required if the new blank window opens and closes normally.** The owner has already confirmed the original A-07 audio diagnostic.

- The archive excludes both the executable and generated WAV.
- Logs are stored in a Git-ignored local directory, never committed.
- The BAT does **not** upload data anywhere.
- Review the archive before sharing; compiler paths can reveal your Windows user folder.

**If something fails**, send the ZIP. It will include available failure details (provided the launcher was able to create its log folder).

## Troubleshooting

- **Missing Git or CMake:** Accept the offered winget installation, or install them manually. Close/reopen Command Prompt if the installer updated PATH.
- **Missing Visual Studio 2022 C++ Build Tools:** Accept the optional winget installation or install the *Desktop development with C++* workload and Windows SDK manually. This may require a few GB and a restart.
- **CMake generator mismatch:** If an old build directory was made with another generator, delete only the generated `build-a07` folder and rerun.
- **Git pull blocked by local edits:** Manually review/commit/stash the edits. The BAT deliberately refuses to overwrite them.
- **No audio output device:** Choose a functioning Windows sound device, then rerun. If it persists, share the ZIP showing `AUDIO_SMOKE_NO_DEVICE`.
- **Window fails to open:** Share the ZIP and any visible error text.

The final playable game and its Windows installer are future roadmap tasks. The owner **approved the raylib 6.0 production stack** at Stage 1. The separate A-07 audio diagnostic remains available through BAT `audio` mode.

## Production foundation and first blank window (F-01–F-05)

**Stage 1 architecture/stack approved by the owner on 2026-11-04.** The root `CMakeLists.txt` builds the production Windows executable (`locus.exe`) and pure C++17 game-core library (`locus_core`). **G-06 now implements the complete 16-second demo session** in a 960×540 raylib window; Home, Play, Results and the M-03 Editor are functional; editor changes are in memory until M-04 adds saving. **Esc/X** closes cleanly. The `--foundation-check` and `--session-check` flags run headless diagnostics in CI.

For a direct Windows Visual Studio 2022 build in Command Prompt:

~~~bat
cmake -S . -B build-a07/foundation -G "Visual Studio 17 2022" -A x64 -DLOCUS_BUILD_TESTS=ON
cmake --build build-a07/foundation --config Release
build-a07\foundation\Release\locus.exe
~~~

**F-05 acceptance (owner-confirmed on 2026-11-04):** The owner ran the Windows application and confirmed that the blank window opened and closed normally. F-05 is complete; no ZIP or repeat manual check is required. The GitHub-hosted Windows runner still cannot initialize a GUI (`WINDOW_INIT_FAILED`), so GUI success in CI is not claimed. `locus.exe --window-smoke` remains available for manual diagnostics and automatically exits after 60 frames on machines with a display.


**F-06 application loop:** Input polling, state update, and drawing are separate functions in `src/app/main.cpp`. They run in this exact order on each iteration:

~~~text
PollInput() -> UpdateState() -> (exit if requested) -> RenderFrame()
~~~

raylib pumps native OS/window events during `EndDrawing()`; the next `PollInput()` reads the resulting close/input state via `WindowShouldClose()`. Do not explicitly pump native events a second time. The update phase changes only application state and never draws; the render phase paints the blank screen and does not modify application state. Future gameplay logic must be coordinated in updates using musical time **not frame count**. The 60-frame `--window-smoke` limit exists only for this diagnostic, not for rhythm timing.

**F-06 headless check:** `locus.exe --loop-check` exercises the update/stop rules for exactly 60 simulated frames and an immediate close event, printing `LOOP_STATE_PASS frames=60` without opening a window. Windows CI runs this on every source change; it is a foundation check, not a timing or real-input latency benchmark.

**F-07 debug navigation (owner-confirmed on 2026-11-04):** The owner previously confirmed F1–F4 changed screens in one window. Double-clicking the BAT retains those debug shortcuts: **F1 Home, F2 Play, F3 Editor, F4 Results**; they are disabled when launching `locus.exe` normally. The real demo can now be started with **Enter**, and the Results screen appears automatically after the track. Editor is now a usable grid/timeline view (also opened by **E** on Home). CI checks route editing, screen transitions and the full start/Results/retry flow headlessly.

**Text legibility improvement (2026-11-04):** The owner requested clearer placeholder text. On Windows, LOCUS now loads the **installed Segoe UI TrueType font** (Arial fallback), uses larger type and higher-contrast colors, and frees the font resource before closing the raylib window. If the system font is missing or cannot be loaded, raylib's default remains available. No Microsoft font files are copied into the repository, assets, or game package. Successful compilation does not establish that the visual appearance is satisfactory; ordinary feedback is welcome whenever the playable demo is tested.

**F-08 dedicated Windows CI:** The independent [`.github/workflows/windows_ci.yml`](.github/workflows/windows_ci.yml) runs on a clean public GitHub Actions `windows-2022` worker. It configures CMake with pinned dependencies, builds Windows x64 Release, runs **all CTest tests**, and runs headless foundation, loop, screen-transition and dependency diagnostics. It checks that generated files do not dirty the repository and uploads `locus.exe`, the test binaries and machine-readable test results as `locus-windows-foundation-x64`. It needs no private credentials. Hosted Actions runners do **not** provide an interactive display, so F-08 intentionally does **not** claim a visual or real-audio-device test. The older A-07 workflow remains available for isolated audio-probe regression checks.

**Current direction:** Foundation is complete enough to start gameplay; the owner explicitly cancelled the former F-09 and Stage 2 measurement gate. Work on the [short playable roadmap](mvp/ROADMAP.md) now. Only request hands-on testing when there is something playable, or an actual Windows-specific blocker.

Build output stays inside the Git-ignored `build-a07/` tree for the existing Windows CI and audio probe. The F-03 `.gitignore` also supports future `build/`, `build-*/`, `cmake-build-*/`, and `out/` build directories. The `LOCUS_BUILD_TESTS=ON` option now builds `locus_core_tests` (F-04), which runs headlessly through CTest; additional gameplay tests will be added as core modules arrive.

**F-04 core smoke test (Windows Command Prompt):** After configuring and building with `-DLOCUS_BUILD_TESTS=ON`, run:

~~~bat
ctest --test-dir build-a07/foundation -C Release --output-on-failure
~~~

The first test (`locus_core_foundation`) checks a small pure C++ core build/link contract and does not open a window or audio device. This is an **infrastructure placeholder**, not a gameplay correctness or latency test. Windows CI runs it automatically.

**F-02 dependencies:** Production CMake downloads exact commit revisions of **raylib 6.0**, **raygui 5.0**, and **nlohmann/json 3.12.0** through `cmake/dependencies.cmake`. CMake does not need machine-specific library install paths. The first configure requires Internet access; later builds use the fetched sources. The `locus_dependencies_smoke.exe` target verifies that the pinned headers and implementations compile and link headlessly. **It is not the F-04 unit-test runner.** The BAT now defaults to building the production window with F-07 debug navigation, while `build_and_run.bat audio` preserves the A-07 diagnostic.

**Keep `build_and_run.bat`.** With the owner's permission, F-05 switches its default to the production blank window and preserves the original A-07 diagnostic via `audio` mode. Future tasks may refine the launcher as LOCUS becomes playable, retaining error logs.

**Latest G-07 playtest improvements (2026-11-05):** **F = Left, J = Right** (replacing A/D). Press **F11** to toggle fullscreen borderless-windowed mode; press it again to return to a normal window, which can also be resized or maximized with the title bar. All screens are laid out on a 960×540 virtual canvas and scaled uniformly to the available window/screen area, preserving the 4×4 grid's geometry. The active cyan line now **grows continuously with the moving marker** rather than appearing whole on the metronome beat; the newest far-blue look-ahead step also reveals gradually. Wrapped route segments continue to grow on their own edges without crossing the grid. These changes are made in response to owner feedback; the G-07 final real-device acceptance remains pending.

**Gameplay progress (G-01 through G-06):** The built-in 32-beat, 120 BPM chart defines the exact authored Left/Right route across the 4×4 grid. **Play now moves the bright marker along that route** in time with the generated click track. The current route is red; the next beats fade in along the shared one-bar red preview. When a step wraps at a grid edge, the preview and marker disappear/reappear across **two short edge segments**, never drawing a false line through the middle of the board. `src/core/route_view.*` computes that display geometry in a testable, platform-independent way. **Enter** from Home starts playback, returning to another screen stops it, and the marker ends on the chart's final cell after 32 beats. Its position and the preview use the same fractional beat from the song clock, never accumulated frame delta or player keys. **G-05 now responds to F = Left and J = Right**, judging each scheduled turn as Perfect (within 50 ms), Good (within 110 ms), Bad (within 180 ms), or Miss (wrong button or no valid hit). These are **provisional gameplay windows**, not a measured latency calibration. Only fresh key presses count; stray presses outside event windows are ignored. A wrong first button consumes the turn as Miss. The Play screen displays the next required turn, counts, and latest result. **No Fail:** the marker always follows the authored route regardless of input, and all nine demo turns resolve exactly once. **G-06: complete demo session.** From Home press **Enter** to start the 32-beat (16-second) demo. Press **Space** to pause or resume the music, marker and timing judgements together; while paused, F/J are ignored. Press **R** to restart and clear all results or **H** to return Home. When the track finishes, the game automatically resolves all nine turns and opens **Results** with Perfect/Good/Bad/Miss totals. From Results, press **Enter/R** to play again or **H** to return Home. Esc or the window X closes the application. F1–F4 remain debug-only screen shortcuts. Windows CI checks clock pause/resume and the session state transitions headlessly; physical output timing and visual feel await the first real-device playtest (G-07).

## Optional diagnostics — not required for gameplay

The historical [baseline report](mvp/BASELINES.md) and `build_and_run.bat baseline` mode remain available **only if a real performance problem needs investigation**. The former F-09 measurement task and Stage 2 baseline approval gate have been **cancelled** as prerequisites. Do not ask the owner to run measurements or send ZIPs before working on the game.

**Current priority:** [G-01 through G-07](mvp/ROADMAP.md), ending with a playable built-in demo. Custom-song loading and chart editing come after the first playable. CI should verify functional code, not spend time gathering idle CPU statistics.

## Repository hygiene and formatting (F-03)

The root [`.gitignore`](.gitignore) excludes generated build trees, fetched CMake dependencies, local audio/charts, editor temporary saves, Windows IDE state, and local diagnostic files. These rules intentionally **do not ignore every WAV or JSON file**: original or appropriately licensed test fixtures may be committed deliberately.

The root [`.clang-format`](.clang-format) is the C++17 formatting policy (LLVM-based, four-space indentation, 100-column limit). Developers can run `clang-format -i src/app/main.cpp` with an installed LLVM formatter; formatting is intentionally **not applied automatically** to source in F-03. Windows CI checks that the policy is applied consistently and idempotently, and that build artifacts leave the Git worktree clean.

The owner has explicitly authorized updating `build_and_run.bat` when later development needs a different executable or better diagnostics. F-03 left it unchanged; **F-05 has now updated it** to launch the blank production window by default, with the probe available via `audio` mode.

## Engineering priorities

1. **Correct audio/input synchronization and minimal end-to-end latency.**
2. **Consistent performance and frame pacing**, with verifiable measurements.
3. Manageable code, dependencies and feature scope; avoid unnecessary architecture or runaway code growth.
4. Package size is **secondary**. A release may be tens or hundreds of megabytes if justified; no tiny-file target is imposed.

All project documentation, code, comments and UI strings are in **English**.

## HUMAN GATE REQUIRED

Whenever an owner-side check or decision is needed early, the assistant must state **HUMAN GATE REQUIRED** clearly and immediately, describing the required action rather than silently advancing. See [GDD §10](mvp/GDD.md) and [Roadmap](mvp/ROADMAP.md).

**A-07's early human gate has been satisfied.** The owner ran the probe on Windows and reported that everything worked; **no ZIP was requested or required afterward**. This is a qualitative check, not a numerical latency benchmark.

**The owner simplified the roadmap on 2026-11-05.** The previous architecture/Foundation approval stands, and the F-09/Stage 2 infrastructure gate is no longer required. The next planned hands-on check is the **first playable G-07 demo**; ask earlier only if a concrete blocker requires real Windows testing.
