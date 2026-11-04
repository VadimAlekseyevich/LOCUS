# LOCUS

A rhythm game currently in development. The **MVP game is not playable yet**. The runnable Windows build at this stage is the **A-07 raylib audio/input timing probe**, used to measure the behavior of audio streaming and keyboard input.

[Game Design Document](mvp/GDD.md) · [MVP Roadmap](mvp/ROADMAP.md) · [Audio Evaluation](mvp/architecture/STACK_EVALUATION.md) · [Stack Decision](mvp/architecture/STACK_DECISION.md)

## Windows: double-click to build and run

**Launcher:** [build_and_run.bat](build_and_run.bat) — a native **Windows batch file**. **No Git Bash, WSL, or Bash shell is required.**

### First-time setup

- Windows 10 or Windows 11; speakers or headphones and a working output device.
- **Git for Windows**, **CMake 3.24+**, and **Visual Studio 2022 with C++ tools**. A free Visual Studio 2022 Build Tools installation with the *Desktop development with C++* workload and Windows SDK is sufficient; the full IDE is not required.
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
3. Checks build prerequisites, asks before installing anything missing, and configures the isolated raylib 6.0 C++ probe.
4. Compiles the **Windows x64 Release** executable in `build-a07`.
5. Runs the generated WAV file self-test and an **audio-device smoke test** (play, pause, seek, resume).
6. Starts the interactive diagnostic window and records timestamps to CSV.
7. Offers optional feedback questions, then collects **all available logs into one ZIP**. A ZIP is produced even when configuration/build/tests fail after log-folder creation.

All build output is kept locally and ignored by Git. Nothing is uploaded automatically.

### Launch modes

| Command | Behavior |
| --- | --- |
| `build_and_run.bat` | Default: pull, build, tests, audio smoke, launch GUI, ZIP logs |
| `build_and_run.bat build` | Pull and compile only |
| `build_and_run.bat test` | Pull, compile, run WAV self-test |
| `build_and_run.bat smoke` | Pull, compile, WAV test, audio-device lifecycle smoke |
| `build_and_run.bat run` | Same as double-click |

**Advanced/CI only:** Set `LOCUS_SKIP_PULL=1` to skip Git updates or `LOCUS_NO_PAUSE=1` to avoid waiting for a key at the end. These are not needed for normal use.

### Controls in the test window

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
LOCUS\.locus-runs\a07-YYYYMMDD-HHMMSS-12345\LOCUS-A07-logs.zip
~~~

**If troubleshooting is needed, you can attach this ZIP to the ChatGPT conversation.** It includes relevant Git, dependency, compiler, self-test and audio-smoke output, plus timestamped CSV and optional feedback if the GUI ran. **The owner has already confirmed the functional A-07 check; no upload is required now.**

- The archive excludes both the executable and generated WAV.
- Logs are stored in a Git-ignored local directory, never committed.
- The BAT does **not** upload data anywhere.
- Review the archive before sharing; compiler paths can reveal your Windows user folder.

**Even if something fails**, send the ZIP. It will include the failure details (when the launcher was able to create its log folder).

## Troubleshooting

- **Missing Git or CMake:** Accept the offered winget installation, or install them manually. Close/reopen Command Prompt if the installer updated PATH.
- **Missing Visual Studio 2022 C++ Build Tools:** Accept the optional winget installation or install the *Desktop development with C++* workload and Windows SDK manually. This may require a few GB and a restart.
- **CMake generator mismatch:** If an old build directory was made with another generator, delete only the generated `build-a07` folder and rerun.
- **Git pull blocked by local edits:** Manually review/commit/stash the edits. The BAT deliberately refuses to overwrite them.
- **No audio output device:** Choose a functioning Windows sound device, then rerun. If it persists, share the ZIP showing `AUDIO_SMOKE_NO_DEVICE`.
- **Window fails to open:** Share the ZIP and any visible error text.

The final game and its Windows installer are separate future roadmap tasks. raylib 6.0 is now the **proposed production stack** (A-09), **pending Stage 1 owner approval**; the A-07 diagnostic remains a separate test executable. The Windows BAT stays in the repository.

## Production foundation (F-01)

**Stage 1 architecture/stack approved by the owner on 2026-11-04.** The separate production root `CMakeLists.txt` now builds a **minimal console executable** (`locus.exe`) and a **pure C++17 core static library** (`locus_core`). The app currently prints a foundation status and exits. The actual game window arrives in **F-05** after external dependencies are pinned in **F-02**. This output is **not a playable game**.

For a direct Windows Visual Studio 2022 build in Command Prompt:

~~~bat
cmake -S . -B build-a07/foundation -G "Visual Studio 17 2022" -A x64 -DLOCUS_BUILD_TESTS=ON
cmake --build build-a07/foundation --config Release
build-a07\foundation\Release\locus.exe
~~~

The build output is intentionally inside the already Git-ignored `build-a07/` tree until repository hygiene task **F-03** establishes the general production build folder convention. The tests option is wired in F-01; **F-04** will add the actual CTest test executable.

**Keep `build_and_run.bat`.** During early Foundation it continues running the A-07 audio-input diagnostic, not the new console stub. A separate, reviewable future task will change its default target to the game when a useful interactive application exists, preserving the audio test and diagnostic ZIP capability.

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
