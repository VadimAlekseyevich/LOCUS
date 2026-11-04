# LOCUS

A rhythm game currently in development. The **MVP game is not playable yet**. Two Windows executables exist: the **F-05 blank production window** (`locus.exe`, launched by double-clicking `build_and_run.bat`) and the original **A-07 audio/input timing probe** (select the BAT's `audio` mode).

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

### What the BAT does

1. Copies itself to a temporary location so a **Git update can safely replace the original BAT**.
2. Checks for local uncommitted files; runs `git pull --ff-only` only if safe. It does **not** delete or overwrite your changes.
3. Checks build prerequisites and asks before installing anything missing.
4. **By default:** configures the root production CMake project and builds `locus.exe` in `build-a07/foundation/Release/`.
5. Opens the **blank dark LOCUS window**. Close using Esc or the X button.
6. Saves compile errors and window-open/close status to **`LOCUS-game-logs.zip`** if available; a ZIP is produced even when the build fails after log-folder creation.
7. **Optional `audio` mode:** runs the previously tested A-07 metronome, transport tests, key-press CSV, and its separate `LOCUS-a07-logs.zip` bundle.

All build output is kept locally and ignored by Git. Nothing is uploaded automatically.

### Launch modes

| Command | Behavior |
| --- | --- |
| `build_and_run.bat` | **Default:** pull, build and open blank LOCUS production window |
| `build_and_run.bat run` | Same as double-click |
| `build_and_run.bat build` | Pull and compile the production game without launching a window |
| `build_and_run.bat game` | Open blank production LOCUS window |
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

**Stage 1 architecture/stack approved by the owner on 2026-11-04.** The separate production root `CMakeLists.txt` now builds a **minimal console executable** (`locus.exe`) and a **pure C++17 core static library** (`locus_core`). As of **F-05**, normal execution of `locus.exe` opens a **960×540 blank dark raylib window**. Use **Esc** or the window **X** to close it cleanly. This is not yet a playable game or menu; no gameplay, audio transport, or state machine has been added. The `--foundation-check` flag still reports the core linkage status headlessly.

For a direct Windows Visual Studio 2022 build in Command Prompt:

~~~bat
cmake -S . -B build-a07/foundation -G "Visual Studio 17 2022" -A x64 -DLOCUS_BUILD_TESTS=ON
cmake --build build-a07/foundation --config Release
build-a07\foundation\Release\locus.exe
~~~

**F-05 window check:** CI attempts `locus.exe --window-smoke`, which would draw **60 frames** and exit automatically. However, the GitHub-hosted Windows runner returned `WINDOW_INIT_FAILED` because it lacks an interactive desktop. **This is not a verified successful window test.** The app compiles, but the F-05 acceptance check requires an owner run on Windows. **HUMAN GATE REQUIRED EARLY:** double-click `build_and_run.bat`, confirm the blank window appears, remains responsive and closes with Esc or X; report whether it worked. A ZIP is only needed for failures. **F-05 remains unchecked until that confirmation.**

Build output stays inside the Git-ignored `build-a07/` tree for the existing Windows CI and audio probe. The F-03 `.gitignore` also supports future `build/`, `build-*/`, `cmake-build-*/`, and `out/` build directories. The `LOCUS_BUILD_TESTS=ON` option now builds `locus_core_tests` (F-04), which runs headlessly through CTest; additional gameplay tests will be added as core modules arrive.

**F-04 core smoke test (Windows Command Prompt):** After configuring and building with `-DLOCUS_BUILD_TESTS=ON`, run:

~~~bat
ctest --test-dir build-a07/foundation -C Release --output-on-failure
~~~

The first test (`locus_core_foundation`) checks a small pure C++ core build/link contract and does not open a window or audio device. This is an **infrastructure placeholder**, not a gameplay correctness or latency test. Windows CI runs it automatically.

**F-02 dependencies:** Production CMake downloads exact commit revisions of **raylib 6.0**, **raygui 5.0**, and **nlohmann/json 3.12.0** through `cmake/dependencies.cmake`. CMake does not need machine-specific library install paths. The first configure requires Internet access; later builds use the fetched sources. The `locus_dependencies_smoke.exe` target verifies that the pinned headers and implementations compile and link headlessly. **It is not the F-04 unit-test runner.** The BAT now defaults to building the F-05 production window, while `build_and_run.bat audio` preserves the A-07 diagnostic.

**Keep `build_and_run.bat`.** With the owner's permission, F-05 switches its default to the production blank window and preserves the original A-07 diagnostic via `audio` mode. Future tasks may refine the launcher as LOCUS becomes playable, retaining error logs.

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
