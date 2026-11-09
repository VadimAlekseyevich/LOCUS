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

The world is infinite. A stationary marker stays at the center while the **red forward route moves beneath it**. Only the part inside the centered square is visible. In landscape the square's side equals the **window height**; a thin white frame can be recolored in SKIN. No gameplay grid or text is drawn on the left.

To the right, the game shows **Accuracy** and counts for Perfect, Good, Bad and Miss. Accuracy is `(100×Perfect + 75×Good + 50×Bad) / all judged notes`; a Miss contributes zero. A chart ends **when its final note is judged**, not when the music file ends. Results shows the final accuracy and counts.

| Game action | Default key |
| --- | --- |
| Left / Down / Up / Right | **D / F / J / K** |
| Diagonal | Two adjacent directions together (for example J+K) |
| Pause / retry / Home | **Space / R / H** |

Gameplay direction bindings are configurable in SETTINGS and are **shared with editor note placement**. The default two-key chord window is 85 ms.

## Screens and editor

Home has **PLAY, EDIT, SKIN, SETTINGS, EXIT**. PLAY opens a read-only library with a song tree on the right: selecting a song expands its indented charts below it. Up/Down navigate rows, Left/Right switch difficulties; use **EDIT** for content changes. Installed songs live under `%LOCALAPPDATA%\LOCUS\Songs`. The built-in demo is always available.

EDIT has four tabs and a persistent scrolling timeline:

- **SET:** currently selected Song Set, shared audio and difficulties; open, create, copy, import, export, delete/restore from within the editor. Confirmations protect unsaved charts and deletions.
- **ROUTE:** moving route preview, nine-way direction pad, note placement, selection, copy/paste, shift, invert and undo/redo.
- **AUDIO:** difficulty name, song-wide BPM and offset (whole milliseconds), and an exact manual preview start in milliseconds.
- **FIELD:** one square size 2–16 (**default 8**) and initial heading. No editable start coordinates.

Use the configured DFJK directions to place notes. In the timeline, **←/→** navigate and **↑/↓** adjust snap subdivision; PageUp/PageDown, wheel and modifier combinations also work. Load audio with **O**, preview with **Space**, test-play with **T**; practice speeds are 100/75/50/25/10%. **Ctrl+S** saves directly into the permanent Song Set library, **Ctrl+Shift+S** creates a new difficulty in that set, **Ctrl+O** opens a chart, and **Ctrl+N** starts a new draft. External chart files are not silently overwritten: saving their edited versions installs them into the library.

To share a complete song with all difficulties, use **EDIT → SET → EXPORT** to export a `.locuspack` archive. **EDIT → SET → IMPORT** accepts individual `.locus.json` charts or complete `.locuspack` packages. The archive is a standard ZIP container with stored files, song metadata, shared audio and all difficulties. Imports check paths, checksums and file size limits before installing anything.

Open a song with **EDIT**, then use **SET → NEW / COPY / DELETE / DELETE SET / RESTORE LAST** to manage difficulties and songs. Deletions require **Y** confirmation and can be recovered from LOCUS Trash. **AUDIO → SONG PREVIEW START (MS)** accepts a manually typed whole-millisecond value (Enter saves, Esc cancels, Ctrl+V pastes); the +/− buttons are removed. On **ROUTE**, click the **TIME** value beneath the route preview to copy its current song position as an integer number of milliseconds; the ruler itself has no time labels. **ADD/REPLACE BG** and **REMOVE BG** manage shared song artwork. On **ROUTE**, select a note and press Backspace/Delete (or the middle **DEL** pad button) to remove it; Ctrl+Z undoes edits. All difficulties in a managed Song Set share exactly one audio file. See [Song Set storage](docs/SONG_SETS.md).

New chart files use **formatVersion 2**, integer `timeMs` timestamps and independent snap settings. BPM changes do not move already authored notes in song time. See [chart format](docs/CHART_FORMAT.md) and [sample](examples/first-song.locus.json) (requires your own `song.mp3`).

SKIN now has **PATH, PLAYER, COLORS and MY SKINS** tabs. COLORS uses a hue wheel, a saturation/brightness square and an exact `#RRGGBB` / `#RRGGBBAA` field instead of four RGBA sliders; the live gameplay preview stays visible while changing colors. MY SKINS scans `%LOCALAPPDATA%\\LOCUS\\Skins\\Profiles`; choose **ACTIVATE** to apply a saved profile, **STORE** to create another named profile, **REFRESH** to rescan, **DELETE** to remove the selected profile with confirmation, or **UNDO** to restore the most recent deletion from `Skins\\Trash`. The active look remains in `Skins\\active.locus-skin.json`. **LOAD** imports an external JSON into the draft, **SAVE** applies changes, and **SAVE AS** exports a JSON. Switching profiles refuses to discard unsaved edits. SKIN also edits path effects, marker size, **fixed cardinal movement speed** (default 135 logical pixels/s), HUD visibility and ten RGBA colors including the square border. Speed is independent of BPM, chart grid size, practice playback rate and heading: diagonal movement is normalized to the same distance per tick as straight movement. The ROUTE editor shows the continuous route without grid lines or dots, while retaining its preview square and magnification control. During PLAY, **[ / ]** or **numpad − / +** adjust the speed by 10 pixels/s and save it for future charts. Old v1 skins are loaded with their previous camera zoom converted into base speed. The [skin sample](examples/default.locus-skin.json) is independent of charts.

**Esc means Back** outside Home. After **EDITOR → TEST**, Esc/H returns directly to the **same unsaved editor draft**; TEST never auto-saves. In normal PLAY, Esc/H returns to Song Library. Windows F11 uses a non-topmost borderless desktop window so Alt+Tab can switch applications. Unsaved chart changes require confirmation. **Esc on Home**, EXIT, the window X and Alt+F4 open a quit confirmation; **Y** quits and **N/Esc** cancels.

## Project

- [Game design](docs/GDD.md)
- [Roadmap](docs/ROADMAP.md)
- [Architecture audit](docs/ARCHITECTURE.md)
- [Chart format](docs/CHART_FORMAT.md)
- [Song Set storage and recovery](docs/SONG_SETS.md)

The Windows CI checks compilation, chart/editor behavior and scoring without a GUI. Physical audio latency and desktop appearance still require real-device testing.
