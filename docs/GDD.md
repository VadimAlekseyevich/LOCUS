# LOCUS — Game Design Document

## 1. Identity and design pillars

**One-sentence pitch:** Follow an endlessly moving route, read the next bend, and perform its outgoing direction in rhythm.

- **Route-first, no fail.** A marker follows the authored line regardless of presses. Bad timing and wrong direction alter grades, never the physical route.
- **Music-anchored timing.** The visual position and judgement share the song clock and authored note timestamps, not frames.
- **Spatial rhythm, not a lane chart.** The player reads an eight-direction path that moves beneath a marker followed by a gently lagging camera (or a fixed-centered marker when camera lag is set to zero).
- **Clear playable window.** The route is clipped to a centered square; outside it, the world still exists but is invisible.
- **Creation inside the game.** A built-in editor, local songs and reusable skins are core features, not someday placeholders.
- **Restraint.** One red forward-only route, modest hit feedback, minimal in-game stats and no clutter.

## 2. Gameplay and spatial rules

### 2.1 Infinite world and camera

- The route lives on an **unbounded two-dimensional plane**. There is **no Wrap, teleport, bounce, physical grid edge or collision** at the viewing boundary.
- At each musical tick the chart's heading determines the next displacement. Eight compass directions are available: up, up-right, right, down-right, down, down-left, left, up-left. Every tick moves **one unit of distance**: cardinals change one axis by 1; diagonals change both axes by **1/√2**. All eight headings therefore have the same speed.
- Every authored event sets an **absolute compass heading**, one of `u/r/d/l/ru/rd/ld/lu`. The next movement direction does not depend on the previous heading. Straight travel needs no event. Reversals retrace exactly the same geometry, without artificial offsets or extra lanes. A reverse junction receives a semicircular cap oriented toward the arriving side; the cap's straight diameter is perpendicular to the line. SKIN → PATH provides a REVERSE CAP SIZE slider (default 3.2× junction radius, twice the former 1.6×, with 0 disabling the cap); junction colour and glow are shared. The SKIN preview includes an example of the cap; the saved profile's cap size also applies to the editor's route preview. This is a rendering-only extension of the normal turn dot, not a new gameplay cue or an alternate route.
- **Experimental lagging camera (2026-11-10):** in PLAY and editor TEST the camera tracks the player's current world position using a time-based exponential follower (180 ms catch-up by default, adjustable from SKIN → PLAYER → CAMERA LAG (MS); 0 disables the effect), without inspecting future notes or predicting route turns. The camera slightly trails behind actual movement: the player appears ahead of screen center. On sustained movement the lag grows proportionally with on-screen speed, capped at 72 virtual pixels to keep the avatar visible; at turns it gradually catches up rather than anticipating the new direction. The follower's update uses elapsed real time, is stable across frame rates and practice playback rates, and is independent of the chart's scoring/timing. Pause and the final-note outro freeze the offset; restarting or opening another Play session resets the follower. The experiment can be disabled by setting `kExperimentalAdaptiveCamera = false` in `src/app/adaptive_camera.h`, or reverted as one Git commit.
- Missing a note or pressing the wrong direction never changes any later route position.

### 2.2 Square visibility

- The playable area is a **perfect square centered in the actual game window**. In landscape it has a physical side length equal to the **current window height**, not a hard-coded 540-pixel limit.
- The virtual reference layout is 960×540, yielding a square from x=210 to 750; at 1920×1080 the square is 1080×1080. In portrait windows its side is constrained by the window width.
- Route segments, junctions, marker and effects **outside this square are not rendered**. Clipping is visual only; the route and its scoring continue.
- A thin rectangular outline is **white by default**, with its color and alpha editable in the global skin.
- The authoring field has **one square side-length value** (2–16 logical cells; **default 8**), for editor viewing metadata. Gameplay scales its route from a global **fixed base movement speed**, not chart field size or BPM. Grid size is not a time signature or a world boundary. The ROUTE preview draws **no grid lines or dots**: only the route, player marker and upcoming turn markers appear inside the clipped square. The numeric field size still adjusts preview magnification, without constraining the trajectory. Editor and PLAY share continuous route coordinates. Visible segments and junctions are sampled in contiguous batches, avoiding repeated full-history route scans on dense charts.
- The skin sets movement speed in **logical pixels per second** (default **135** on the virtual 540×540 playfield), identical in all eight directions. Each chart's visual step compensates for BPM, ticks per beat and practice playback rate. Physical speed scales with the resizable window's play square.
- Starting X/Y is no longer an exposed control. New charts have an automatic centered internal origin; stored start metadata remains supported by the file reader.

### 2.3 Path preview and feedback

- Only the **future red route** is drawn, beginning at the marker and extending **800 ms of original song time by default** (SKIN → PATH → PREVIEW TIME (MS), adjustable). At 0.5x practice playback this covers the same chart events, giving the player twice as much real time to read them.
- Width and opacity gradually fade towards the far end; **past trajectory is absent**. Upcoming turn junctions and 180° semicircular caps share that time horizon, getting **brighter and larger** as they approach the player.
- The path is clipped at the square edge, even when it would continue beyond it; clipping does not truncate the underlying chart.
- The marker can display a restrained Perfect/Good/Bad flash; Miss has no success flash. The skin customizes path, marker and frame colors.

## 3. Input and judgement

- The player presses the **absolute outgoing direction**, not a fixed relative "turn left/right" button. The required direction equals the note's absolute heading.
- Four keys cover the cardinal directions. An authored diagonal turn is **two independent arrow notes at the same target timestamp**: each of its two constituent directions is graded separately when freshly pressed. The keys may be pressed in either order or more than 85 ms apart, provided each falls in its own hit window.
- **Default bindings:** **D=Left, F=Down, J=Up, K=Right**. Space=Pause, R=Retry, H=Home. The same four directional binds place notes in the editor. All seven bindings are configurable in SETTINGS.
- There is **no gameplay chord-skew window**. SETTINGS → GAME → EDITOR DIAGONAL CHORD WINDOW (85 ms by default, 40–160 ms) affects **only authoring a two-key diagonal in the editor**, not scoring diagonal turns in PLAY/TEST.
- In slowed practice playback, the rendered marker, route and lagging camera use a smoothly interpolated audio cursor between decoder-buffer updates; judgement stays tied to the real consumed-audio clock. The visual cursor tracks sustained audio drift gradually and resets on a song restart or seek. This avoids visible 200–600 px/s stepping at 50% practice speed without modifying hit windows.
- **SETTINGS → DISPLAY** controls VSync (on by default to prevent screen tearing), FPS cap (30, 60, 90, 120, 144, 165, 240 or unlimited; default 240), FPS counter overlay (off by default), and borderless fullscreen. The FPS cap is a ceiling; with VSync enabled, the monitor's refresh rate normally limits actual FPS. Rendering options are persisted and applied without restarting; rhythm judgement still uses the audio clock.
- A correctly aimed press is Perfect within **±50 ms**, Good within **±110 ms**, Bad within **±180 ms**; a wrong direction in the hit window or an expired note is Miss. Ignored stray presses do not create extra grades.
- A **cardinal turn counts as one note**; a **diagonal turn counts as two notes** in Perfect/Good/Bad/Miss statistics, accuracy and Results completion. Hitting one diagonal arrow cannot automatically grade the other: its missing half receives Miss after its own late window. Each physical arrow is judged at most once; holding it or re-pressing an already-scored component cannot farm score. No Fail remains active for all grades.

### 3.1 Accuracy and completion

The accuracy percentage uses **judged arrow notes**, not the number of keyboard presses:

```text
Accuracy (%) = (100 × Perfect + 75 × Good + 50 × Bad)
               / (Perfect + Good + Bad + Miss)
```

- Miss is worth zero and **counts in the denominator**. One of each grade = **56.25%**.
- Before the first graded note, the UI shows **100.00%**; the ongoing and final value use the same formula with two decimals.
- A scored chart triggers a short visual outro **when its last scorable note receives its grade**, even if the song has unused audio afterwards. The camera freezes, the avatar eases offscreen along the last direction, the view fades to black and Results fades in. Audio continues through Results to its natural end (no looping).
- A last unpressed note becomes Miss **only after its late ±180-ms window**. A short silent scoring tail is allowed if audio ends before this window expires.
- An empty chart, or an editor TEST starting after all events, falls back to ending at audio end.
- Editor TEST excludes earlier skipped notes from grades and accuracy.

## 4. Screen layout and navigation

- Moving between Home, Song Library, Editor, Skin, Settings and Listen uses a brief eased **fade through black**: approximately 65 ms to dark and 95 ms back to visible. Navigation and music switching are applied immediately; the incoming PLAY screen reveals directly over 100 ms so gameplay input and the music clock are never delayed. The scored PLAY → Results outro retains its own longer black/reveal and does not receive a redundant second fade.

### 4.1 During Play

- **Center:** the square, route, marker, configurable border and optional hit effects. A song-wide PNG/JPEG background may fill the physical window behind the game, preserving its aspect ratio and center-cropping excess rather than distorting pixels. Backgrounds also appear dimmed behind Song Select and Results.
- **Right of square:** live **Accuracy**, **Perfect**, **Good**, **Bad**, **Miss**. On narrower windows values fit the actually visible right region.
- **Left of square:** intentionally **empty**. No cut-off LOCUS logo, latest-hit readout, song title, field-size log, labels or debug text.
- PLAY hotkeys **[ / ]** or numpad **- / +** adjust base speed by **10 logical px/s**; **, / .** decrease/increase PREVIEW TIME by **50 ms**. Both repeat while held (230 ms initial delay, faster after 1.2 s), never override rebound gameplay keys, and persist to the global skin after a short idle period (or on exit). Both values are shown beneath the right-side grades, or briefly within the square when no sidebar fits.
- A pause overlay offers pause/retry/home information without editing the chart. The built-in demo and local charts use the same gameplay and results presentation.

### 4.2 Results

Show the same four judgement counts plus **final Accuracy** and resolved-note count. Retry, Home and **E to edit the played chart** remain available. Results is not delayed to the end of a long audio outro.

### 4.3 Menus and safe exit

- Home provides **PLAY, EDIT, SKIN, SETTINGS, EXIT**. **Home → EDIT** always creates a clean draft, not the last-played song. Song Library EDIT opens its selected chart; **Results → E** opens the actual played chart; returning from Editor TEST keeps the unsaved editor document.
- PLAY is a read-only song tree on the right: selecting a song expands its indented difficulties in ascending calculated difficulty order. EDIT → SET owns imports, deletion/restoration and artwork. EDIT → AUDIO owns song-wide BPM, offset and exact manual preview start (whole milliseconds). A background is shared by all difficulties of its song.
- **Esc goes Back** from Editor, PLAY, Results and other screens. Normal PLAY/Results return to Song Select; after Editor TEST, Esc/H returns to the original unsaved editor draft. Editor also has a visible BACK button; unsaved chart changes require Y to discard or N/Esc to cancel.
- **Esc on Home**, EXIT, window X and Alt+F4 open one confirmation. **Y quits; N/Esc stays**. An unsaved chart or skin triggers a warning. Active playback pauses while the confirmation is shown.

## 5. Music, charting and practice

- Local WAV/MP3 playback is supported, without any account or remote chart service. Installed songs/difficulties live under `%LOCALAPPDATA%\LOCUS\Songs`.
- Chart editor tabs: **SET** (song/difficulty management and artwork), **ROUTE** (grid-free moving preview, nine-direction pad, note and group actions), **AUDIO** (difficulty title, shared song BPM/offset and precise manually entered preview milliseconds), **FIELD** (square visibility size and initial heading).
- The scrolling timeline is visible on all editor tabs. Clicking the single TIME readout below the ROUTE preview copies its current song position in whole milliseconds, ready for Ctrl+V into the Song Preview field. The ruler itself stays uncluttered. Left/Right arrows navigate ticks; Up/Down change snap subdivision; PageUp/PageDown and the mouse wheel offer navigation. Modifier shortcuts support bar jumps, moving notes and groups.
- Snap subdivision choices: **1, 2, 3, 4, 6, 8, 12, 16, 24** per musical beat. Snap, zoom and BPM are **different controls**.
- Chart **v3** writes each note at its **absolute integer `timeMs` audio timestamp** and the signed integer `offsetMs`. Changing BPM/snap must not silently move existing notes in song time. The editor rejects a projection collision rather than dropping notes.
- The writer stores an abbreviated absolute `heading` for each note and for the initial route direction. A 180° reversal is detected from the incoming and outgoing headings, not from a special relative note type.
- Editor can add/edit/cut/copy/paste/mirror horizontally or vertically/rotate a selected pattern by ±90°/shift/undo directions, open JSON and TEST from the selected audio position. Ctrl+S saves directly into the installed Song Set; Ctrl+Shift+S creates a separate difficulty. Backwards compatibility before the first release is not guaranteed.
- Editor preview and scored custom-song play can use **100%, 75%, 50%, 25%, 10%** audio speeds. The present slowdown changes pitch; it is **not** pitch-preserving time stretch. Below 100% the preview timeline follows consumed audio-stream frames rather than extrapolating a separate wall-clock position, preventing accumulating visual/audio drift when buffering fluctuates. In scored practice, screen-space movement speed remains fixed even as playback slows.

## 6. Skins and accessibility

- Each managed Song Set stores one shared audio file, a `set.locus.json` manifest, and independent difficulty charts. Importing a chart never silently replaces the shared audio. Folder locks serialize writers, chart overwrites leave backups in `.locus-history/`, and deleted songs/difficulties go to recoverable `%LOCALAPPDATA%/LOCUS/Trash`. New difficulties are created in EDIT → SET; notes can be removed on ROUTE with Backspace/Delete and Ctrl+Z restores them. See [Song Sets](SONG_SETS.md).
- One independent **global skin** affects presentation across Play and editor TEST; no skin is embedded in a chart.
- PATH, PLAYER and COLORS pages expose path width/opacity/fade, marker size, fixed base speed (20–2000 logical px/s), **CAMERA LAG (MS)** (0 = fixed center, 180 default, 0–500 slider / 0–1500 typed), hit-effect scale, gameplay HUD visibility, and ten editable RGBA colors including **FIELD BORDER**. COLORS uses a hue wheel, a saturation/value square, and exact `#RRGGBBAA` entry; the live preview is visible on every page. Only the non-color numeric settings retain sliders. Numeric controls round to sensible precision (whole milliseconds for PREVIEW TIME, hundredths for other numeric fields) and omit trailing zeroes/floating-point noise in their UI labels.
- MY SKINS is a personal profile library at `%LOCALAPPDATA%/LOCUS/Skins/Profiles`. STORE saves a new named JSON profile, ACTIVATE selects it without losing unsaved drafts, and REFRESH discovers external additions. DELETE moves a selected profile to a separate `Skins/Trash` after confirmation; UNDO restores it. The applied appearance is preserved when its profile is deleted. The chosen skin is copied to the global active file so gameplay and chart testing use the same appearance.
- Skin JSON v2 saves `player.speedPxPerSecond`, `player.cameraLagMs` (default 180, optional when loading existing profiles) and `path.lookaheadMs` (default 800). Legacy skins with `path.lookaheadBeats` still load; their preview duration resets to 800 ms because BPM-independent conversion from beats is impossible. Legacy v1 `cameraStep` is converted on load to the same approximate speed at 120 BPM on the default 8×8 chart.
- The skin cannot alter chart timestamps, judgement rules or authored route geometry.
- Windows system fonts are used when available; gameplay readability at non-16:9 aspect ratios must be checked on actual displays.

## 7. Not yet committed as finished

The following areas are not implemented or verified:

- A verified, straightforward Windows distributable/installer independent of local build tools.
- Hands-on display, keyboard/audio synchronization and accessibility checks on multiple real machines and aspect ratios. Windows CI cannot judge these.
- Tempo maps, hold notes, richer note mechanics, waveform editing, pitch-preserving slowdown and measured input/audio latency.
- Online hosting/accounts/leaderboards, multiplayer and non-Windows ports.

Remaining work is tracked in [ROADMAP.md](ROADMAP.md).


Difficulty v2 estimates each chart on an uncapped scale using exact timestamps, sustained and peak note density, key combinations, rhythm, route readability and stamina. It is displayed in PLAY and EDIT and never written into chart JSON. Full methodology: [CHART_DIFFICULTY.md](CHART_DIFFICULTY.md).
