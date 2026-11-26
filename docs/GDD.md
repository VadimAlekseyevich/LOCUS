# LOCUS — Game Design Document

## 1. Identity and design pillars

**One-sentence pitch:** Follow an endlessly moving route, read the next directional event, and perform it in rhythm (including repeated notes on a straight route).

- **Physical execution first, no fail.** A marker follows the authored line regardless of presses. Bad timing and wrong direction alter grades, never the physical route.
- **Music-anchored timing.** Judgement and visuals derive from the authoritative song-time cursor and exact timestamps, not frame count. Visual smoothing is presentation only.
- **Spatial rhythm, not a lane chart.** The player reads an eight-direction path that moves beneath a marker followed by a gently lagging camera (or a fixed-centered marker when camera lag is set to zero).
- **Clear playable window.** The route is clipped to a centered square; outside it, the world still exists but is invisible.
- **Creation inside the game.** A built-in editor, local songs and reusable skins are core features, not someday placeholders.
- **Restraint.** One red forward-only route, modest hit feedback, minimal in-game stats and no clutter.

## 2. Gameplay and spatial rules

### 2.1 Infinite world and camera

- The route lives on an **unbounded two-dimensional plane**. There is **no Wrap, teleport, bounce, physical grid edge or collision** at the viewing boundary.
- During PLAY/TEST the route changes direction at each note's **exact absolute song timestamp** (`timeMs`), not at the nearest editor tick. Between events movement is continuous and constant-speed. Eight compass directions are available: up, up-right, right, down-right, down, down-left, left, up-left. Diagonal displacement is normalized by **1/√2**, so all eight headings have the same speed. Musical ticks remain an editor snapping/navigation tool.
- Every authored event sets an **absolute compass heading**, one of `u/r/d/l/ru/rd/ld/lu`. The next movement direction does not depend on the previous heading. Straight travel needs no event unless a repeated press is authored: the compass may explicitly place a same-heading note without bending the route. Such a note uses the ordinary judgement and junction marker. DEL is the explicit removal command. Reversals retrace exactly the same geometry, without artificial offsets or extra lanes. A reverse junction receives a semicircular cap oriented toward the arriving side; the cap's straight diameter is perpendicular to the line. SKIN → PATH provides a REVERSE CAP SIZE slider (default 2× junction radius, with 0 disabling the cap); junction colour and glow are shared. The SKIN preview includes an example of the cap; the saved profile's cap size also applies to the editor's route preview. This is a rendering-only extension of the normal turn dot, not a new gameplay cue or an alternate route.
- **Lagging camera:** in PLAY and editor TEST the camera tracks the player's current world position using a time-based exponential follower (180 ms catch-up by default, adjustable from SKIN → PLAYER → CAMERA LAG (MS); 0 disables the effect), without inspecting future notes or predicting route turns. The camera slightly trails behind actual movement: the player appears ahead of screen center. On sustained movement the lag grows proportionally with on-screen speed, capped at 72 virtual pixels to keep the avatar visible; at turns it gradually catches up rather than anticipating the new direction. The follower's update uses elapsed real time, is stable across frame rates and practice playback rates, and is independent of the chart's scoring/timing. Pause and the final-note outro freeze the offset; restarting or opening another Play session resets the follower. The player can disable camera lag with SKIN → PLAYER → CAMERA LAG (MS) = 0.
- Missing a note or pressing the wrong direction never changes any later route position.

### 2.2 Square visibility

- The playable area is a **perfect square centered in the actual game window**. In landscape it has a physical side length equal to the **current window height**, not a hard-coded 540-pixel limit.
- The virtual reference layout is 960×540, yielding a square from x=210 to 750; at 1920×1080 the square is 1080×1080. In portrait windows its side is constrained by the window width.
- Route segments, junctions, marker and effects **outside this square are not rendered**. Clipping is visual only; the route and its scoring continue.
- The square remains a logical clipping boundary, not an opaque panel. Subtle fading lateral edge guides use the skin's FIELD BORDER color where the square ends inside the window.
- START beside the compass edits the initial heading. Grid metadata remains part of the chart file but does **not** determine preview magnification. ROUTE uses its own saved view speed and path length, independent of SKIN. PLAY/TEST continue using global SKIN base speed and PREVIEW TIME (MS). ROUTE draws no grid lines or dots: only the moving route, player and upcoming turns inside clipped squares. BPM/Snap control authoring and timeline navigation, not the route's visual scale. Its one-to-three parallel previews and PLAY/TEST render from exact-time compiled routes. All ROUTE panels use the same song-seconds cursor and editor-only physical px/s travel scale; they never resnap a compared difficulty onto the active chart's grid. Gameplay junctions, player and camera share the indexed route.
- The skin sets movement speed in **logical pixels per second** (default **135** on the virtual 540×540 playfield), identical in all eight directions. The time-derived route scale compensates for BPM, editor tick density and practice playback rate. Physical speed scales with the resizable window's play square.
- Starting X/Y is no longer an exposed control. New charts have an automatic centered internal origin; stored start metadata remains supported by the file reader.

### 2.3 Path preview and feedback

- Only the **future red route** is drawn, beginning at the marker and extending **800 ms of original song time by default** (SKIN → PATH → PREVIEW TIME (MS), adjustable). At 0.5x practice playback this covers the same chart events, giving the player twice as much real time to read them.
- Width and opacity gradually fade towards the far end; **past trajectory is absent**. Upcoming turn junctions and 180° semicircular caps share that time horizon, getting **brighter and larger** as they approach the player.
- The path is clipped at the square edge, even when it would continue beyond it; clipping does not truncate the underlying chart.
- The marker can display a restrained Perfect/Good/Bad flash; Miss has no success flash. The skin customizes path, marker and frame colors.

## 3. Input and judgement

- The player presses the **absolute outgoing direction**, not a fixed relative "turn left/right" button. The required direction equals the note's absolute heading.
- Four keys cover the cardinal directions. An authored diagonal turn is **two independent arrow notes at the same target timestamp**: each of its two constituent directions is graded separately when freshly pressed. The keys may be pressed in either order or more than 85 ms apart, provided each falls in its own hit window.
- **Default bindings:** **D=Left, F=Down, J=Up, K=Right**. Esc=Pause/Menu in PLAY, R=Retry, H=Leave. Editor preview uses Space. Only the mouse compass pad places notes in ROUTE; the initial heading is edited with START beside the compass.
- There is no gameplay chord-skew window. Diagonal scoring evaluates each of its two constituent directions independently.
- In slowed practice playback, the rendered marker, route and lagging camera can use a smoothed visual cursor; judgement stays tied to the unsmoothed authoritative transport cursor (rate clock at normal speed and in the SoundTouch branch; consumed cursor when supported in ordinary slow playback). The visual cursor tracks sustained audio drift gradually and resets on a song restart or seek. This avoids visible 200–600 px/s stepping at 50% practice speed without modifying hit windows.
- **SETTINGS → AUDIO / OFFSET → GLOBAL AUDIO OFFSET (MS)** compensates for output-device latency independently of a chart's authored `offsetMs`. Positive milliseconds mean the sound arrives after the raw transport clock; the visual song-time cursor and input judgements use `rawSongSeconds - globalOffsetMs / 1000 * playbackRate` in scored PLAY and editor TEST (including the demo). The underlying audio, chart events, BPM, editor timeline and saved song metadata never change. Users can type a signed value (-1000..+1000 ms), or calibrate with the generated **100-BPM** WAV click track using **any configured gameplay direction key** on **every audible beat**. The player may start tapping on any beat; **18 recorded taps** are collected, with repeated taps per beat suppressed. The result uses an outlier-resistant median and rejects unstable attempts. Applying persists the value to optional settings v3 `globalOffsetMs`; earlier configs and key bindings remain valid. Audio device latency and human tapping bias are both measured, so repeated trials are advisable.
- **SETTINGS → DISPLAY** controls VSync (on by default to prevent screen tearing), FPS cap (30, 60, 90, 120, 144, 165, 240 or unlimited; default 240), FPS counter overlay (off by default), and borderless fullscreen. The FPS cap is a ceiling; with VSync enabled, the monitor's refresh rate normally limits actual FPS. Rendering options are persisted and applied without restarting; rhythm judgement still uses the audio clock.
- A correctly aimed press is Perfect within **±50 ms**, Good within **±110 ms**, Bad within **±180 ms**. The matching physical-key queue takes precedence. A wrong direction can resolve an outstanding cardinal target as Miss under the judge's wrong-direction rule; a diagonal's unpressed component expires independently. Ignored stray presses do not create extra grades.
- A **cardinal turn counts as one note**; a **diagonal turn counts as two notes** in Perfect/Good/Bad/Miss statistics, accuracy and Results completion. Hitting one diagonal arrow cannot automatically grade the other: its missing half receives Miss after its own late window. Each physical arrow is judged at most once; holding it or re-pressing an already-scored component cannot farm score. Normal play has no fail; Sudden Death fails on Miss and Perfect fails on any grade below Perfect. Failed attempts are not saved. The MODS panel configures HT/DT with separate sliders and EZ/HR, HD/FL, Mirror/Random, Autoplay. Mirror and Random transform an isolated session chart; Random preserves timestamps and cardinal/diagonal key counts and records its seed. Autoplay never saves scores. Both Autoplay and replay support timeline scrubbing and five-second Left/Right seeking.

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
- **HUD:** current combo at lower left, accuracy and non-default practice rate at upper right. Perfect, Good, Bad and Miss counts form a compact column strictly outside the right edge of the world clip, hidden when the gutter is too narrow. Perfect, Good and Bad extend combo; any Miss breaks it. Diagonals count per physical arrow. Current combo is live; the highest combo is stored with completed results.
- HUD has no background scrims, progress bar or last-hit readout. The essential corner values remain visible in narrow windows; detailed grades never cover the route. HUD can be hidden in the skin. An optional HIT ERROR BAR (off by default) in SKIN → PLAYER draws successful early/late timings with fading marks; the same rendering is used in skin preview. PLAYER adjusts HIT ERROR LENGTH and COLORS provides one HIT ERROR BAR color (RGBA).
- PLAY hotkeys **[ / ]** or numpad **- / +** adjust base speed by **10 logical px/s**; **, / .** decrease/increase PREVIEW TIME by **50 ms**. Both repeat while held (230 ms initial delay, faster after 1.2 s), never override rebound gameplay keys, and persist to the global skin after a short idle period (or on exit). Changes are shown briefly near the top center, including when the HUD is hidden.
- A centered pause dialog offers Resume, Retry and Leave without editing the chart. The mouse cursor is hidden during focused active gameplay and restored for pause, confirmations and other screens. The built-in demo and local charts use the same gameplay and results presentation.

### 4.2 Results

Show the same four judgement counts plus **final Accuracy** and resolved-note count. New completed attempts also store maximum combo, mean hit error, unstable rate and a fixed-bin hit error distribution. WATCH REPLAY in native Results reconstructs recorded scoring operations on the matching chart revision; spectator playback does not save another result. See [Results](RESULTS.md) for format and playback limitations. Retry, Home and **E to edit the played chart** remain available. Results is not delayed to the end of a long audio outro.

### 4.3 Menus and safe exit

- Home provides **PLAY, EDIT, SKIN, SETTINGS, EXIT**. **Home → EDIT** always creates a clean draft, not the last-played song. Song Library EDIT opens its selected chart; **Results → E** opens the actual played chart; returning from Editor TEST keeps the unsaved editor document.
- PLAY is the song-selection screen: selecting a song expands its indented difficulties in ascending Difficulty V4 order. The left panel shows metadata, physical key counts, rating components and saved results. A second click on the selected chart or Enter starts play when search is not focused. EDIT, MODS, IMPORT and RESCAN share the library footer; the MODS overlay does not occupy result-history space. Song Setup owns imports, deletion/restoration, artwork, audio, song-wide tempo table and exact manual preview start. A background is shared by all difficulties of its song.
- **Esc goes Back** in the Editor and menus; in PLAY, Esc toggles Pause/Menu and leaving the level requires confirmation. After Editor TEST, leaving returns to the original unsaved draft. Native menus have no persistent title/back strip; Esc returns from Chart Editor to Song Setup. Unsaved changes require explicit confirmation.
- **Esc on Home**, EXIT, window X and Alt+F4 open one confirmation. **Y quits; N/Esc stays**. An unsaved chart or skin triggers a warning. Active playback pauses while the confirmation is shown.

## 5. Music, charting and practice

- Local WAV/MP3 playback is supported, without any account or remote chart service. Installed songs/difficulties live under `%LOCALAPPDATA%\LOCUS\Songs`.
- **Song Setup** combines song metadata, audio, shared tempo map, menu preview start, artwork and difficulty selection. Select a difficulty or draft and click again or press Enter to open **Chart Editor**. Opening a new draft with attached audio installs its initial Song Set if needed; managed metadata and background edits are then available. Its ROUTE workspace contains the mouse-only compass, START heading, note and group actions, and synchronized previews. Esc returns to Song Setup without discarding the document.
- The zoomable **upper note ruler** is above the Chart Editor workspace. It follows the selected position, displays authored notes and supports snap, zoom, interval drag selection, keyboard navigation and wheel scrolling. It is separate from the **fixed whole-song overview** at the bottom: this overview spans the complete audio duration in seconds and ignores beat zoom and snap scale. Click/drag it to navigate to the nearest available snap tick anywhere in the song without changing chart data. It shows the Song Set's full shared tempo map (one or many markers), never per-difficulty notes. The first marker defines beat zero; each following marker starts a new local downbeat at its exact timestamp. The bottom transport contains Play/Pause and Stop, the overview, a numeric rate field and an editable millisecond position field. Enter or blur seeks to the entered time; Escape cancels. Stop restores the preview origin.
- Snap subdivision choices: **1, 2, 3, 4, 6, 8, 12, 16, 24** per musical beat. Snap, zoom and BPM are **different controls**.
- Chart **v4** only writes each note at its **absolute integer `timeMs` audio timestamp** and the signed integer `offsetMs`. Changing BPM/snap must not silently move existing notes in song time. Distinct exact timestamps may share a projected grid tick; select and edit them individually. Ctrl+click stages an exact time; duplicate timestamps and invalid projections are rejected rather than dropping notes.
- The writer stores an abbreviated absolute `heading` for each note and for the initial route direction. A 180° reversal is detected from the incoming and outgoing headings, not from a special relative note type.
- Editor can add/edit/cut/copy/paste/mirror horizontally or vertically/rotate a selected pattern by ±90°/shift/undo directions, open JSON and TEST from the selected audio position. The ROUTE comparison workspace shows up to three enlarged, equally sized, synchronized charts; clicking a pane transfers edit focus without losing the other open documents' Undo/Redo or dirty state. ROUTE speed (20–2000 px/s) and visible path length (50–10000 ms) can be changed independently of SKIN and persist in settings.json. Ctrl+S writes the focused chart only. Closing an unsaved pane or exiting Editor offers Save, Discard and Cancel; Save on exit writes all dirty documents, and a failed save blocks closing. Ctrl+Shift+S creates another difficulty. Backwards compatibility before the first release is not guaranteed.
- Editor preview accepts an exact 1–100% rate through its bottom transport field; scored custom-song practice uses 100/75/50/25/10%. SoundTouch preserves pitch on supported slow playback, with a pitch-shifting fallback. The stretched stream uses a synthetic rate clock, not a verified consumed-sample hardware cursor. Visual smoothing is separate from judgement; screen-space movement speed remains fixed. See [Timing contract](TIMING_CONTRACT.md).

## 6. Skins and accessibility

- Each managed Song Set stores one shared audio file, a `set.locus.json` manifest, and independent difficulty charts. Importing a chart never silently replaces the shared audio. Folder locks serialize writers, chart overwrites leave backups in `.locus-history/`, and deleted songs/difficulties go to recoverable `%LOCALAPPDATA%/LOCUS/Trash`. New difficulties are created in Song Setup; notes can be removed on ROUTE with Backspace/Delete and Ctrl+Z restores them. See [Song Sets](SONG_SETS.md).
- One independent **global skin** affects presentation across Play and editor TEST; no skin is embedded in a chart.
- SKINS is the first/default profile page and owns rename. All tabs share the same preview rectangle. PATH, PLAYER and COLORS expose path width/opacity/fade, marker size, fixed base speed (20–2000 logical px/s), **CAMERA LAG (MS)** (0 = fixed center, 180 default, 0–500 slider / 0–1500 typed), hit-effect scale, gameplay HUD visibility, and eleven editable RGBA colors including **FIELD BORDER**. COLORS uses a hue wheel, a saturation/value square, and exact `#RRGGBBAA` entry; the live preview is visible on every page. Only the non-color numeric settings retain sliders. Numeric controls round to sensible precision (whole milliseconds for PREVIEW TIME, hundredths for other numeric fields) and omit trailing zeroes/floating-point noise in their UI labels.
- The SKINS profile library is at `%LOCALAPPDATA%/LOCUS/Skins/Profiles`. STORE saves a new named JSON profile, ACTIVATE selects it without losing unsaved drafts, and REFRESH discovers external additions. DELETE moves a selected profile to a separate `Skins/Trash` after confirmation; UNDO restores it. The applied appearance is preserved when its profile is deleted. The chosen skin is copied to the global active file so gameplay and chart testing use the same appearance.
- Skin JSON v2 saves `player.speedPxPerSecond`, `player.cameraLagMs` (default 180) and `path.lookaheadMs` (default 800). Old skin formats are not loaded.
- The skin cannot alter chart timestamps, judgement rules or authored route geometry.
- Windows system fonts are used when available; gameplay readability at non-16:9 aspect ratios must be checked on actual displays.

## 7. Difficulty

Difficulty V4 is an uncapped estimate of physical execution: speed,
individual finger retriggers, chord coordination, rhythm and stamina.
It aggregates decaying strain in 400 ms sections. Reading is diagnostic
only and never contributes to the overall rating. Ratings are computed in
PLAY and EDIT, not stored in chart JSON. Parameters remain provisional until
calibrated against real attempts. See [formulas](CHART_DIFFICULTY.md).

## 8. Remaining scope

The following areas remain open:

- A tagged public release and clean-machine interactive acceptance of the already-built CI portable ZIP.
- Hands-on display, keyboard/audio synchronization and accessibility checks on multiple real machines and aspect ratios. Windows CI cannot judge these.
- Measured input/audio latency and further refinement of editor waveform rendering.
- Online hosting/accounts/leaderboards, multiplayer and non-Windows ports.

Remaining work is tracked in [ROADMAP.md](ROADMAP.md).


Optional skin hitsounds: one user WAV, MP3 or OGG Vorbis in SKINS → AUDIO; volume is global and independent of music in SETTINGS → AUDIO / OFFSET; successful physical arrows, including individual diagonal components and replayed hits, trigger the sound. This is presentation-only and does not change the judge or replay format.

The editor displays a cached peak/RMS audio waveform at 5 ms resolution in the upper note ruler only. WAVE ON/OFF below Snap and Zoom toggles visibility without changing chart data. A subtle peak envelope preserves transients; the brighter RMS envelope shows local signal energy even on heavily limited recordings. The metronome plays a click at each beat of the manually authored tempo map, with a 4/4 accent. The lower overview shows physical difficulty only in ROUTE; Song Setup displays shared timing and transport position without waveform or any difficulty-specific graph. Tempo changes preserve all exact authored note timestamps.
