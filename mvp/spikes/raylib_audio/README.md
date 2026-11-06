# A-07 — Disposable raylib Audio Timing Probe

This folder is an **isolated technical experiment** for LOCUS roadmap item A-07. It is **not** the main game, not production architecture, and should not be linked as a dependency by future LOCUS modules.

## Purpose

Test the practical ergonomics of raylib's music stream API on **Windows**:

- Generate and play an **original** 16-second 120 BPM metronome WAV (48 kHz, mono, 16-bit PCM).
- Display GetMusicTimePlayed() alongside a steady-clock estimate anchored on Play/Seek/Resume.
- Capture A/D key-down **poll timestamps** and save an event/periodic sample CSV.
- Exercise pause, resume, seek and restart.
- Record Windows release executable and generated WAV sizes.

**Do not interpret** the difference between music-reported time and a steady clock as measured speaker/DAC latency. GetMusicTimePlayed() is a stream-position estimate and raylib's GetKeyPressed() is frame-polled. The probe is useful for identifying discontinuities, drift, pause/seek bugs, and dependency overhead, not establishing sample-accurate input-to-output latency.

## Recommended local run (double-click Windows BAT)

The default launcher starts the **production game**, not this isolated spike. To run the optional A-07 probe, open Windows Command Prompt in the repository root and run:

~~~bat
build_and_run.bat audio
~~~

This fast-forward pulls the latest Git commits, configures/builds the optional probe, runs the WAV self-test and backend smoke, launches the interactive probe, and creates a local `LOCUS-A07-logs.zip` diagnostic archive. It is **not uploaded automatically**. Use `build_and_run.bat test` for only the headless WAV self-test. See the [root README](../../../README.md) for prerequisites, troubleshooting, and the exact log location. The manual commands below are alternatives for debugging.

**A-07 EARLY HUMAN GATE SATISFIED:** The owner reported the Windows interactive probe worked correctly. No diagnostic archive is required to close the functional spike. **No objective physical audio/input latency measurement has been made**, and a later stack or timing review may request targeted testing. The original BAT remains available for voluntary diagnostics.

## Windows prerequisites

- Windows 10/11, CMake 3.24 or newer, Visual Studio 2022 C/C++ tools, Git, a working audio output device.
- Network access on first configure: CMake FetchContent retrieves **raylib 6.0** from its pinned Git tag.
- No Python installation or preexisting WAV needed.

## Build

From the repository root:

~~~powershell
cmake -S mvp/spikes/raylib_audio -B build-a07 -G "Visual Studio 17 2022" -A x64
cmake --build build-a07 --config Release --parallel 2
~~~

## Headless self-test (safe in CI)

From the repository root:

~~~powershell
.\build-a07\Release\locus_raylib_audio_spike.exe --self-test
~~~

This creates \`metronome.wav\`, checks the RIFF/WAVE magic and expected byte size, and prints executable size. **It does not open a window/audio device and cannot validate playback timing.**

## Optional headless audio-backend smoke test

~~~powershell
.\build-a07\Release\locus_raylib_audio_spike.exe --audio-smoke
~~~

This opens the raylib **audio device only**, without a window, attempts ~0.85 seconds of playback, ~0.35 seconds of pause, a seek to 4 seconds, and ~0.45 seconds of resumed playback. It prints reported stream positions. On hosted CI runners, the test can fail because no audio device is available; that is an environment limitation, **not proof of a bug in the game's timing**. Even a passing result **cannot prove sound was audible** or that input latency is low.

## Interactive audio test

~~~powershell
.\build-a07\Release\locus_raylib_audio_spike.exe
~~~

| Control | Action |
| --- | --- |
| A / D | Capture a Left/Right **test** key-down time (not a judgement) |
| Space / P | Pause/resume music |
| J / L | Seek backward/forward by two seconds |
| R | Restart playback |
| Esc | Exit |

The probe displays raylib music position, a monotonic clock estimate, their difference, and last captured key-down time. It also writes **raylib_probe_log.csv** to the working directory with 100 ms periodic samples and actions, including timestamps and stream position.

**Recommended manual sequence:** listen to the first four ticks; tap A on ticks; pause for ~2 seconds and confirm reported time remains stable; resume; seek forward/backward; repeat several times; restart; examine CSV for jumps, drift, or inconsistent seek return positions. Repeat with render load or frame-rate caps before approving the clock approach. A human must verify the sound: a CI self-test cannot do so.

For a meaningful real-input-latency claim, a later experiment must compare recorded sound output against timestamped physical input using a loopback/measurement device. This probe by itself does not establish hardware latency.

## Packaging metrics

The dedicated GitHub Actions workflow \`.github/workflows/a07_raylib_probe.yml\` builds on a Windows runner, runs the headless self-test, captures byte counts and the build artifact. Measured values belong in [STACK_EVALUATION.md](../../architecture/STACK_EVALUATION.md). Do not invent Windows numbers on a non-Windows host.

The executable is **not** a portable LOCUS release. Measure required DLLs and runtime behavior before claiming that static raylib removes every system dependency.
