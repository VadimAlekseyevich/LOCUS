# LOCUS

A Windows rhythm game with a built-in chart editor. Read the moving route and press the outgoing direction in time with your song; missed notes change your grade, never the authored route.

## Run

Double-click **`build_and_run.bat`** in the repository folder. It checks for Git, CMake 3.25+ and Visual Studio 2022 C++ Build Tools, offers to install missing tools, builds the game and launches it. It pulls Git updates with fast-forward only and does not overwrite local changes.

| Command | Action |
| --- | --- |
| `build_and_run.bat` | Build and run LOCUS |
| `build_and_run.bat build` | Build without launching |
| `build_and_run.bat test` | Build and run the game’s CTest suite |

A local log folder is created under `.locus-runs/` (Git-ignored). Logs are not uploaded. CI runs the same production CMake build and tests on Windows.

## Gameplay

The world is infinite. During PLAY, an experimental camera **slightly lags behind the moving player** instead of anticipating the route: the player appears ahead of center as the camera catches up. The lag scales with movement speed (up to a safe limit), and turns are smoothed without reading future notes. The **red forward route moves with the camera**. Only the part inside the centered square is visible. In landscape the square's side equals the **window height**; a thin white frame can be recolored in SKIN. No gameplay grid or text is drawn on the left.

To the right, the game shows **Accuracy** and counts for Perfect, Good, Bad and Miss. Accuracy is `(100×Perfect + 75×Good + 50×Bad) / all judged notes`; a Miss contributes zero. A chart ends **when its final note is judged**, not when the music file ends. Results shows the final accuracy and counts.

| Game action | Default key |
| --- | --- |
| Left / Down / Up / Right | **D / F / J / K** |
| Diagonal | Two adjacent directions together (for example J+K) |
| Pause / retry / Home | **Esc / R / H** |

Gameplay direction bindings are configurable in SETTINGS; editor note placement uses only the clickable direction pad. During PLAY (including editor TEST), a fresh press of any bound direction during a long unmapped intro skips the music forward to the first note minus the smallest whole number of **4/4 measures lasting at least one second**. The triggering press is not scored; near-note presses and normal music with no long intro are unaffected.

## Screens and editor

Home has **PLAY, EDIT, SKIN, SETTINGS, EXIT**. PLAY opens a read-only library with a song tree on the right: selecting a song expands its indented charts below it, sorted by calculated difficulty (easiest first). Up/Down navigate rows, Left/Right switch difficulties; use **EDIT** for content changes. Each chart displays an uncapped **DIFFICULTY V2** rating, with separate speed, rhythm, key coordination, reading and stamina estimates. The rating is also visible live in EDIT → ROUTE / AUDIO. See [Chart Difficulty](docs/CHART_DIFFICULTY.md) for the complete heuristic and its limitations. Installed songs live under `%LOCALAPPDATA%\LOCUS\Songs`. The built-in demo is always available.

EDIT has four tabs and two independent timelines. The **upper note ruler** scrolls with zoom/snap and shows authored turns. The **bottom whole-song overview** always spans the exact audio duration, never zooms and shows the Song Set's shared BPM markers rather than chart notes. Click or drag the bottom bar to jump to distant sections at the closest snap tick without modifying the chart. All compared previews follow the same selected song time:

- **SET:** Song Set, shared audio and difficulties (easiest first). **Click a difficulty to open it directly**; unsaved draft changes require confirmation. Use NEW, COPY, IMPORT, EXPORT, DELETE/RESTORE and OPEN FOLDER. The list scrolls with the mouse wheel, without paging buttons.
- **ROUTE:** larger single-purpose editing buttons, TEST, an enlarged 222×222 route preview and full-size 3×3 mouse-only direction pad. **COMPARE** opens at most two other difficulties of the same Song Set, for three equally sized, exact-time synchronized previews. The **SPD −/+** and **PATH −/+** controls beside the pad set editor-only movement speed (px/s) and visible path length (ms), saved separately from the gameplay SKIN. Click a preview to focus that chart **without moving or resizing its pane or seeking the song**: the upper ruler and lower song overview remain at exactly the same music time, even with different tick resolutions or paused playback. Its notes and tools appear above, while the single audio playback stays shared. Each open chart retains its own Undo/Redo and unsaved route; nonfocused unsaved panes show an asterisk. Practice-speed buttons stay below the whole-song overview. Copy/paste, shift, mirror, rotations and undo/redo are available.
- **AUDIO:** difficulty name, an editable **song-wide BPM table** (absolute offset in whole milliseconds and BPM per row), plus the independent Song Preview Start. Add a marker at the selected song position, edit its time/tempo, or delete a later marker. The first row establishes beat zero; later rows change BPM without restarting beat count. The table and markers apply to every difficulty in the pack.

Chart titles, song/artist names, difficulty labels and saved skin names are stored in **UTF-8**. Input, paste, Backspace and shortened UI labels preserve whole Unicode characters (Cyrillic, accents, etc.). Glyph rendering depends on the installed font; the Windows UI font atlas covers Cyrillic, Latin with accents and Greek.
- **FIELD:** initial compass heading, selectable by its left/right buttons. Current SKIN base speed and preview time are shown read-only; no independent editor grid-size control.

Click the eight compass directions or the center DEL button to place/remove notes; gameplay direction keys do not place editor notes. On the upper note ruler, **←/→** navigate and **↑/↓** adjust snap subdivision; PageUp/PageDown, wheel and modifier combinations also work. Load audio with **O**, preview with **Space**, test-play with **T**; in PLAY, Esc opens pause/menu. Practice speeds in ROUTE are 100/75/50/25/10%. **Ctrl+S** saves ONLY the currently focused difficulty into the permanent Song Set library (renaming a difficulty also renames its JSON file, with a safe suffix if needed), **Ctrl+Shift+S** creates a new difficulty in that set, **Ctrl+O** opens a chart, and **Ctrl+N** starts a new draft. External chart files are not silently overwritten: saving their edited versions installs them into the library.

To share a complete song with all difficulties, use **EDIT → SET → EXPORT** to export a `.locuspack` archive. **EDIT → SET → IMPORT** accepts individual `.locus.json` charts or complete `.locuspack` packages. The archive is a standard ZIP container with stored files, song metadata, shared audio and all difficulties. Imports check paths, checksums and file size limits before installing anything.

Open a song with **EDIT**, then use **SET → NEW / COPY / DELETE / DELETE SET / RESTORE LAST** to manage difficulties and songs. Deletions require **Y** confirmation and can be recovered from LOCUS Trash. **AUDIO → SONG PREVIEW START (MS)** accepts a manually typed whole-millisecond value (Enter saves, Esc cancels, Ctrl+V pastes); the +/− buttons are removed. Copied ROUTE note patterns remain available when switching difficulties or opening a new draft; Ctrl+V pastes using the destination chart's current tick resolution (incompatible tick grids are rejected without modifying the chart). On **ROUTE**, click the **TIME** value above the direction pad to copy its current song position as an integer number of milliseconds; the upper note ruler itself has no time labels. The lower overview shows the song end and all shared BPM-change markers. **ADD/REPLACE BG** and **REMOVE BG** manage shared song artwork. On **ROUTE**, the **ROT -90** and **ROT +90** buttons rotate the selected pattern’s absolute headings counterclockwise/clockwise without shifting timestamps. Select a note and press Backspace/Delete (or the middle **DEL** pad button) to remove it; Ctrl+Z undoes edits. All difficulties in a managed Song Set share exactly one audio file. See [Song Set storage](docs/SONG_SETS.md).

Chart JSON uses **formatVersion 4** only, absolute heading codes `u/r/d/l/ru/rd/ld/lu`, integer `timeMs` and independent snap settings. Older chart versions and full-length headings are not supported. BPM changes do not move already authored notes in song time. See [chart format](docs/CHART_FORMAT.md) and [sample](examples/first-song.locus.json) (requires your own `song.mp3`).

SKIN now has **PATH, PLAYER, COLORS and MY SKINS** tabs. COLORS uses a hue wheel, a saturation/brightness square and an exact `#RRGGBB` / `#RRGGBBAA` field instead of four RGBA sliders; the live gameplay preview stays visible while changing colors. MY SKINS scans `%LOCALAPPDATA%\\LOCUS\\Skins\\Profiles`; **OPEN FOLDER** opens that directory in Explorer; choose **ACTIVATE** to apply a saved profile, **STORE** to create another named profile, **REFRESH** to rescan, **DELETE** to remove the selected profile with confirmation, or **UNDO** to restore the most recent deletion from `Skins\\Trash`. The active look remains in `Skins\\active.locus-skin.json`. **LOAD** imports an external JSON into the draft, **SAVE** applies changes, and **SAVE AS** exports a JSON. Switching profiles refuses to discard unsaved edits. SKIN → PLAYER also provides **CAMERA LAG (MS)** (180 by default; 0 disables the effect; slider 0–500, exact entry up to 1500), saved separately in every skin. SKIN also edits path effects, marker size, **fixed cardinal movement speed** (20–2000 logical pixels/s; default 135), HUD visibility and ten RGBA colors including the square border. Speed is independent of BPM, chart grid size, practice playback rate and heading: diagonal movement is normalized to the same distance per tick as straight movement. The ROUTE editor shows continuous exact-time preview routes without grid lines or dots. All ROUTE panes use separate editor-only movement speed and path-length settings; these do not change the gameplay SKIN. Grid metadata does not scale the preview. The snap ruler remains editable. During PLAY, **[ / ]** or **numpad − / +** adjust the speed by 10 pixels/s and save it for future charts. The [skin sample](examples/default.locus-skin.json) is independent of charts.

**Esc means Back** in Editor and menus, **Pause/Menu** in PLAY. After **EDITOR → TEST**, leaving the level returns directly to the **same unsaved editor draft**; TEST never auto-saves. Normal PLAY requires confirmation before leaving for Song Library. Windows F11 uses a non-topmost borderless desktop window so Alt+Tab can switch applications. Closing an unsaved comparison chart or leaving Editor prompts **S/Save**, **Y/Discard**, or **N/Esc/Cancel**. Saving on exit writes all open changed charts; a failed write does not close any remaining unsaved draft. New unsaved difficulties must be saved once before they can be parked in a comparison pane. **Esc on Home**, EXIT, the window X and Alt+F4 open a quit confirmation; **Y** quits and **N/Esc** cancels.

## Project

- [Game design](docs/GDD.md)
- [Roadmap](docs/ROADMAP.md)
- [Architecture audit](docs/ARCHITECTURE.md)
- [Chart format](docs/CHART_FORMAT.md)
- [Song Set storage and recovery](docs/SONG_SETS.md)

The Windows CI checks compilation, chart/editor behavior and scoring without a GUI. Physical audio latency and desktop appearance still require real-device testing.
