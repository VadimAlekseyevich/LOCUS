# LOCUS

**Windows rhythm game and local chart editor** · C++17 / raylib 6.0 · 2026-11-06

LOCUS is a rhythm game about **reading a moving route and pressing its outgoing direction in time with the music**. The marker follows the author's route automatically on an **infinite two-dimensional plane**. Your input affects Perfect/Good/Bad/Miss and accuracy — **never the route** (No Fail).

The current game is **not** the early 4×4, edge-wrapping, two-button prototype. For the current concept read the [Game Design Document](mvp/GDD.md); for genuinely unfinished work read the [Roadmap](mvp/ROADMAP.md).

## What is playable

- Eight-direction routes, including diagonals and relative bends of 45° / 90° / 135°.
- **Screen-centered square** whose side equals the actual window height in landscape (width-limited in portrait). Red future route is visually clipped to the square; a thin white outline is customizable in SKIN. The world never wraps or teleports.
- Marker fixed in the square's center; only a fading **forward red path** and upcoming turn nodes appear. No past-route trail or grey gameplay grid.
- **Right-side** live Accuracy and Perfect/Good/Bad/Miss counters. The **left side is deliberately empty** — no cut-off wordmark, logs, song title or status labels.
- Weighted accuracy **(100×Perfect + 75×Good + 50×Bad) / (Perfect + Good + Bad + Miss)**. One of each grade = **56.25%**. Before the first scored note, the displayed value is 100.00%.
- Scored Play/Test ends when the **last playable note is judged**, not when the audio outro finishes. A missing last note receives its full 180-ms late window, including a short silent tail if required. Results shows all four grade counts and final accuracy.
- Built-in demo, local WAV/MP3 listening, installed-song library with multiple chart difficulties, in-game chart editor and independent global skins.

## Start on Windows

Double-click **`build_and_run.bat`** in the repository folder. By default it builds and starts the **production LOCUS game**, not the historical audio probe.

**Developer prerequisites:** Windows 10/11 x64, Git, CMake ≥3.25, Visual Studio 2022 C++ Build Tools with the Windows SDK, and network access to retrieve pinned dependencies on first configure. The BAT checks prerequisites and **asks before using winget** to install missing tools; the large C++ workload may require a restart. It only updates Git with **fast-forward** and refuses to overwrite local uncommitted changes.

Common commands from Command Prompt:

| Command | Purpose |
| --- | --- |
| `build_and_run.bat` | Build and launch the game |
| `build_and_run.bat build` | Build the production Release EXE without launching |
| `build_and_run.bat baseline` | Optional local Windows CPU/RAM sampling; close via X or Esc on Home, then **Y** |
| `build_and_run.bat audio` | Isolated A-07 interactive audio/input diagnostic |
| `build_and_run.bat test` | A-07 WAV probe headless self-test (**not** the production CTest suite) |
| `build_and_run.bat smoke` | A-07 audio-backend smoke test |

These commands may create local diagnostics under the Git-ignored `.locus-runs/` folder; they **never upload them automatically**. To skip the automatic Git pull, set `LOCUS_SKIP_PULL=1` explicitly in the terminal.

### Build and run automated tests manually

~~~powershell
cmake -S . -B build/windows-foundation -G "Visual Studio 17 2022" -A x64 -DLOCUS_BUILD_TESTS=ON
cmake --build build/windows-foundation --config Release --parallel 2
ctest --test-dir build/windows-foundation -C Release --output-on-failure
~~~

The primary CI workflow is [Windows Build and Tests](.github/workflows/windows_ci.yml). CI compiles the game, runs CTest and headless state/session checks. **It cannot inspect a real GUI, listen to audio or prove physical input latency.**

### First steps in the game

1. From **Home**, choose **PLAY** to open the song/difficulty library (the generated demo is always available), **EDIT** to author a chart, **SKIN** to change appearance, **SETTINGS** to rebind keys, or **EXIT** to quit.
2. To play your own song, **IMPORT (C)** a `.locus.json` file referencing a local WAV/MP3. The library installs song and chart together under `%LOCALAPPDATA%\LOCUS\Songs`; the originals are not deleted.
3. Select a song with **↑/↓**, a difficulty with **←/→**, then press **Enter** or click PLAY. Press **E** to edit the selected chart.
4. During gameplay, read the outgoing path and press the corresponding direction. Accuracy and four grade counters stay **to the right** of the central square.
5. Results opens after the final note's grade; **Enter/R** retries and **H/Esc** returns Home.

Use **O** on Home to listen to an arbitrary WAV/MP3 without needing a chart.

## Controls

### Scored gameplay / shared editor note placement

| Direction | Default key |
| --- | --- |
| **Left** | **D** |
| **Down** | **F** |
| **Up** | **J** |
| **Right** | **K** |
| Diagonal | Two adjacent direction keys in the configured chord window, e.g. **J+K** = up-right |
| Pause / resume | **Space** |
| Retry | **R** |
| Home | **H** |

The same four direction binds are used to place notes in the **ROUTE** editor and can all be changed in SETTINGS, together with Pause/Retry/Home. The diagonal chord grace period defaults to **85 ms** and is adjustable from **40–160 ms**.

### Navigation and quitting

- **Esc** in Editor/Play/Results/Library/Listen/Skin/Settings: **Back to Home**, never immediate application exit.
- In the editor, **Esc**, **H** or the **BACK** button leaves for Home; if the chart is dirty, **Y** discards and **N/Esc** stays. First Esc in a text field cancels field editing.
- **Esc on Home**, **EXIT**, the window **X**, or **Alt+F4** anywhere opens **EXIT LOCUS?**. **Y/QUIT** confirms; **N/Esc/STAY** cancels. Unsaved chart/skin changes trigger an additional warning.
- **F11** toggles fullscreen. The diagnostic **F1–F4** shortcuts work only when launching `locus.exe --debug-screens` explicitly, not in normal gameplay.

## Creating a chart

Press **E** from Home, or **E/EDIT** on an installed difficulty.

- **ROUTE:** scrolling grid preview and nine-direction compass pad; click the outgoing direction or use your bound D/F/J/K keys. Copy/cut/paste/invert/move groups, undo/redo and practice at **100%, 75%, 50%, 25%, 10%** speed.
- **AUDIO:** direct title, BPM and signed **whole-millisecond offset** entry. Changes to BPM/offset **do not move previously saved note timestamps**.
- **FIELD:** enter **one size** from 2–16 (**8** by default) for a square NxN view and select starting heading. **No separate width/height or editable starting X/Y**.
- The timeline remains visible on every tab. **←/→** navigates one snap increment; **↑/↓** changes subdivisions. PageUp/PageDown, mouse wheel and Ctrl/Shift combinations provide bar navigation, note moves and group moves. Snap values: **1, 2, 3, 4, 6, 8, 12, 16, 24** per musical beat. Timeline zoom is independent.
- Click **LOAD SONG** or press **O** to attach WAV/MP3. **Space** previews, **T** TEST-plays from the selected or current fractional song position, and **R** restarts playback.
- **Ctrl+S** saves (first save opens a file dialog), **Ctrl+Shift+S** saves elsewhere, **Ctrl+O** opens JSON, **Ctrl+N** creates a fresh draft. Saving preserves your beat selection and undo history. Dirty exits require confirmation.

Scored TEST excludes notes before its selected origin. Slow practice uses audio pitch changes (not pitch-preserving time stretch).

### Chart format and sample

New files use **`formatVersion: 2`**, **absolute integer `turns[].timeMs`** and integer `offsetMs`. BPM and snap describe the musical ruler without altering those timestamps. Old `formatVersion: 1` charts remain readable and migrate on save.

The current sample is [`mvp/examples/first-song.locus.json`](mvp/examples/first-song.locus.json). Copy it beside a local audio file named `song.mp3` (**at least ~16 seconds long**) before importing; the sample does not include copyrighted audio. It is charted at 120 BPM and will only sound aligned to a track at that tempo/offset.

For the full storage rules, legacy migration and safe relative audio paths, see [CHART_FORMAT.md](mvp/architecture/CHART_FORMAT.md).

## Visual skins

The **SKIN** menu is implemented, not a placeholder. It offers global path/fade/junction controls, player and camera size, optional HUD/glow effects, and **ten RGBA colors** including **FIELD BORDER**. Changes are drafted until SAVE; LOAD, SAVE AS and RESET are available. The skin is saved independently from songs and difficulties.

The active skin normally lives at `%LOCALAPPDATA%\LOCUS\Skins\active.locus-skin.json`. Older skins without `colors.fieldBorder` load with the default white border. See [VISUAL_SKINS.md](mvp/architecture/VISUAL_SKINS.md) and [sample skin](mvp/examples/default.locus-skin.json).

## Local files and troubleshooting

- User settings: `%LOCALAPPDATA%\LOCUS\settings.json`. Installed chart/audio library: `%LOCALAPPDATA%\LOCUS\Songs`. Active skin: `%LOCALAPPDATA%\LOCUS\Skins\active.locus-skin.json`.
- The editor saves a chart's audio path **relative to the chart file**. A missing, unsafe, or out-of-folder audio reference is rejected; move the audio into the same folder or an allowed subfolder when saving.
- If building fails, check `.locus-runs` logs for missing Git/CMake/MSVC, a first-run FetchContent download error, or an unsupported audio file. The BAT does not intentionally overwrite dirty repository files.
- The original raylib audio spike remains under [`mvp/spikes/raylib_audio`](mvp/spikes/raylib_audio/README.md) and is launched only through `build_and_run.bat audio`. Historical performance measurements are preserved in [BASELINES.md](mvp/BASELINES.md); they do not constitute mandatory targets.
- Windows CI is authoritative for **automated build and headless tests**, not for what pixels appear or sound comes out of an actual machine.
- The source repository currently has **no explicit license file**. A release license and clean-machine distributable remain open product/release decisions; see the [Roadmap](mvp/ROADMAP.md).

## Project reference documents

- [Current game concept](mvp/GDD.md)
- [Active roadmap and remaining work](mvp/ROADMAP.md)
- [UI and navigation](mvp/architecture/UI_FLOW.md)
- [Absolute-ms timing and judgement](mvp/architecture/TIMING.md)
- [Chart format v2 and v1 compatibility](mvp/architecture/CHART_FORMAT.md)
- [Skin specification](mvp/architecture/VISUAL_SKINS.md)
- [Current module boundaries](mvp/architecture/BOUNDARIES.md)
- [Historical stack choice](mvp/architecture/STACK_DECISION.md) and [audio evaluation](mvp/architecture/STACK_EVALUATION.md)

The old first-prototype Wrap and two-button design survives in Git history as **completed historical work**, not active scope.
