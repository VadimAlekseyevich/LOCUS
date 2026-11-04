# LOCUS

A rhythm game currently in development. The **MVP game is not playable yet**. Two Windows executables exist: the **LOCUS production app** (currently four F-07 placeholder screens, launched by double-clicking `build_and_run.bat`) and the separate **A-07 audio/input timing probe** (BAT `audio` mode).

[Game Design Document](mvp/GDD.md) · [MVP Roadmap](mvp/ROADMAP.md) · [Audio Evaluation](mvp/architecture/STACK_EVALUATION.md) · [Stack Decision](mvp/architecture/STACK_DECISION.md)

## Windows: double-click to build and run

**Launcher:** [build_and_run.bat](build_and_run.bat) — a native **Windows batch file**. **No Git Bash, WSL, or Bash shell is required.**

### First-time setup

- Windows 10 or Windows 11. An audio output device is only needed for the optional A-07 diagnostic, not the blank F-05 window.
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
5. Opens the **LOCUS placeholder Home screen** with debug navigation: F1 Home, F2 Play, F3 Editor, F4 Results. Close using Esc or the X button.
6. Saves compile errors and window-open/close status to **`LOCUS-game-logs.zip`** if available; a ZIP is produced even when the build fails after log-folder creation.
7. **Optional `audio` mode:** runs the previously tested A-07 metronome, transport tests, key-press CSV, and its separate `LOCUS-a07-logs.zip` bundle.

All build output is kept locally and ignored by Git. Nothing is uploaded automatically.

### Launch modes

| Command | Behavior |
| --- | --- |
| `build_and_run.bat` | **Default:** pull, build and open LOCUS with four debug placeholder screens |
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

**Stage 1 architecture/stack approved by the owner on 2026-11-04.** The separate production root `CMakeLists.txt` now builds a **minimal console executable** (`locus.exe`) and a **pure C++17 core static library** (`locus_core`). As of **F-07**, normal execution of `locus.exe` opens a **960×540 dark raylib window** showing the placeholder Home screen. Use **Esc** or the window **X** to close it cleanly. The four-screen state controller is a temporary debug scaffold, **not** a playable game or finished menu; no gameplay or audio transport has been added. The `--foundation-check` flag still reports the core linkage status headlessly.

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

**F-07 screen placeholders (owner-confirmed on 2026-11-04):** LOCUS starts at **Home**. The owner confirmed that F1–F4 visibly change the screen label while keeping the same window. In normal `locus.exe` execution the debug keys are disabled; double-clicking the BAT runs `locus.exe --debug-screens`, which enables **F1 = Home, F2 = Play, F3 = Editor, F4 = Results**. Each screen displays only its name and a placeholder caption. These shortcuts are for development, not final game navigation. The `PollInput()` phase captures the requested screen, `UpdateState()` commits it, and `RenderFrame()` draws that state without modifying it. The headless `locus.exe --screen-check` diagnostic exercises all four transitions, no-input persistence, and close precedence, printing `SCREEN_STATE_PASS transitions=4`.

**Text legibility improvement (2026-11-04):** The owner requested clearer placeholder text. On Windows, LOCUS now loads the **installed Segoe UI TrueType font** (Arial fallback), uses larger type and higher-contrast colors, and frees the font resource before closing the raylib window. If the system font is missing or cannot be loaded, raylib's default remains available. No Microsoft font files are copied into the repository, assets, or game package. Successful compilation does not establish that the visual appearance is satisfactory; review this at the Stage 2 owner gate.

**F-08 dedicated Windows CI:** The independent [`.github/workflows/windows_ci.yml`](.github/workflows/windows_ci.yml) runs on a clean public GitHub Actions `windows-2022` worker. It configures CMake with pinned dependencies, builds Windows x64 Release, runs **all CTest tests**, and runs headless foundation, loop, screen-transition and dependency diagnostics. It checks that generated files do not dirty the repository and uploads `locus.exe`, the test binaries and machine-readable test results as `locus-windows-foundation-x64`. It needs no private credentials. Hosted Actions runners do **not** provide an interactive display, so F-08 intentionally does **not** claim a visual or real-audio-device test. The older A-07 workflow remains available for isolated audio-probe regression checks.

**Stage 2 scheduling:** F-09 (repeatable startup/CPU/memory/package baselines) follows F-08. The **Stage 2 owner approval gate is after F-09**, not immediately after CI is added; no owner action is required merely to finish F-08.

Build output stays inside the Git-ignored `build-a07/` tree for the existing Windows CI and audio probe. The F-03 `.gitignore` also supports future `build/`, `build-*/`, `cmake-build-*/`, and `out/` build directories. The `LOCUS_BUILD_TESTS=ON` option now builds `locus_core_tests` (F-04), which runs headlessly through CTest; additional gameplay tests will be added as core modules arrive.

**F-04 core smoke test (Windows Command Prompt):** After configuring and building with `-DLOCUS_BUILD_TESTS=ON`, run:

~~~bat
ctest --test-dir build-a07/foundation -C Release --output-on-failure
~~~

The first test (`locus_core_foundation`) checks a small pure C++ core build/link contract and does not open a window or audio device. This is an **infrastructure placeholder**, not a gameplay correctness or latency test. Windows CI runs it automatically.

**F-02 dependencies:** Production CMake downloads exact commit revisions of **raylib 6.0**, **raygui 5.0**, and **nlohmann/json 3.12.0** through `cmake/dependencies.cmake`. CMake does not need machine-specific library install paths. The first configure requires Internet access; later builds use the fetched sources. The `locus_dependencies_smoke.exe` target verifies that the pinned headers and implementations compile and link headlessly. **It is not the F-04 unit-test runner.** The BAT now defaults to building the production window with F-07 debug navigation, while `build_and_run.bat audio` preserves the A-07 diagnostic.

**Keep `build_and_run.bat`.** With the owner's permission, F-05 switches its default to the production blank window and preserves the original A-07 diagnostic via `audio` mode. Future tasks may refine the launcher as LOCUS becomes playable, retaining error logs.

## Baseline performance measurement (F-09)

The dedicated Windows CI records **release executable size** and **headless process startup times** using [`tools/measure-baseline.ps1`](tools/measure-baseline.ps1). A hosted runner has no interactive desktop; it cannot provide genuine idle GUI CPU/RAM or a keyboard-to-screen latency measurement. See [`mvp/BASELINES.md`](mvp/BASELINES.md) for the exact methods, environment, measurement definitions and current evidence.

To measure the **actual LOCUS window** on your Windows PC, **first update the cloned repository** if your current BAT predates the new `baseline` mode:

~~~bat
git pull --ff-only
build_and_run.bat baseline
~~~

On subsequent runs, `build_and_run.bat baseline` alone is enough: the BAT checks Git updates itself. Running the old BAT with `baseline` **before** updating would simply print its usage instructions.

Leave the window alone for **12 seconds** while sampling. When it says sampling is complete, press **Esc** or **X** to close the window. The BAT prints a text report with the EXE size, a window-ready proxy and mean/peak CPU/memory observations; it also keeps `baseline-summary.txt` and `baseline-idle-samples.csv` in a local Git-ignored archive. **No ZIP upload is required** when it works: you can simply tell the assistant the report values. The report contains your Windows hardware model and path details, so review it before sharing.

**HUMAN GATE REQUIRED — F-09 / Stage 2:** The owner needs to run this measurement on a genuine interactive Windows desktop, confirm the updated font is readable, and approve Stage 2 once the results have been documented. Until then F-09 stays unchecked; chart-domain C-01 must not begin.

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

**STAGE 1 HUMAN GATE APPROVED (2026-11-04):** The owner explicitly approved the [stack and architecture](mvp/architecture/STACK_DECISION.md). Production Foundation F-01 is authorized. The next scheduled owner gate is **after Stage 2**, once a working window and Windows build/test evidence are available; no early owner action is required for the F-01 console stub.
