# LOCUS

A minimalist rhythm game in development. The **MVP game is not yet playable**. The currently runnable Windows program is an **A-07 technical audio/input timing probe** used to evaluate raylib.

Design and planning: [GDD](mvp/GDD.md) · [Roadmap](mvp/ROADMAP.md) · [Architecture](mvp/architecture/BOUNDARIES.md).

## Run the Windows audio/input test (Git Bash)

### Prerequisites (install once)

- **Windows 10/11** and a working speaker/headphone audio output device.
- **Git for Windows**, including **Git Bash**.
- **CMake 3.24+** available on PATH.
- **Visual Studio 2022 Community** or **Visual Studio 2022 Build Tools**, with the **Desktop development with C++** workload and Windows SDK installed. A full Visual Studio IDE is **not** required if Build Tools is installed.
- Internet access for the first setup. CMake downloads the pinned **raylib 6.0** source into the local build directory.

Do not run the shell script from WSL/Linux: it targets native Windows Visual Studio builds.

### First run

Open **Git Bash** and enter:

~~~bash
git clone https://github.com/VadimAlekseyevich/LOCUS.git
cd LOCUS
bash run-a07.sh
~~~

If the repository is already cloned, open Git Bash in its root folder and run just:

~~~bash
bash run-a07.sh
~~~

Every run performs the following in order:

1. **Pull latest Git commits** using \`git pull --ff-only\` (never overwrites local edits; dirty repositories are rejected).
2. Configure and compile the **isolated** raylib A-07 program in \`build-a07/\`.
3. Run a deterministic WAV generation **self-test**.
4. Run an audio-device **smoke test** including Play / Pause / Seek / Resume.
5. Launch the **interactive diagnostic window**.
6. Ask a few optional observation questions after the window closes.
7. Pack console/build output, diagnostic CSV, and your feedback into an archive.

**This launches the A-07 probe, not the game.** The production Windows application has not been implemented.

### What to do in the diagnostic window

The program generates a **16-second, 120 BPM metronome** and logs its reported playback position.

| Key | Test action |
| --- | --- |
| **A / D** | Tap on audible clicks to record Left/Right input capture timestamps |
| **Space / P** | Pause and resume (time should stop advancing while paused) |
| **J / L** | Seek 2 seconds backward / forward |
| **R** | Restart the 16-second metronome; use this to keep testing |
| **Esc** | Close the window; return to Git Bash for feedback and log archive |

Listen for regular clicks, tap A/D around each tick, pause for a few seconds, resume, seek, and restart. Note any discontinuity, noise, freezing, delayed reaction, or uneven timing.

**Timing interpretation:** \`GetMusicTimePlayed()\` is not a hardware sound-output timestamp, and raylib's \`GetKeyPressed()\` is polled during frames. The CSV can reveal API/clock behavior but **cannot by itself prove physical input-to-audio latency**. Report subjective impressions separately; they are valuable but are not a calibrated latency measurement.

### Send me the logs

At the end, the script prints an exact path resembling:

~~~text
.locus-runs/a07-YYYYMMDDTHHMMSSZ-1234/LOCUS-A07-logs.tar.gz
~~~

**Attach that \`LOCUS-A07-logs.tar.gz\` file to the ChatGPT conversation.** It includes:

- Git commit, compiler/generator and EXE size.
- CMake configure/build output and WAV self-test.
- Audio lifecycle test output.
- \`raylib_probe_log.csv\` (song-time samples and A/D key captures), when the app launched.
- Your optional listening/playback feedback.

The WAV and compiled binaries are deliberately **not** included in the archive. All generated data is in ignored local folders, never committed. The script doesn't transmit or upload any logs. **Review before sharing:** compiler logs can contain local Windows paths or user folder names. It is fine to send the archive even if building or audio initialization failed; the script collects partial logs.

### If it fails

- **CMake or Git not found:** ensure Git for Windows / CMake were installed and restart Git Bash.
- **Visual Studio generator not found:** install Visual Studio 2022 Build Tools with **Desktop development with C++**; ensure you selected the Windows SDK.
- **Cannot fetch raylib:** check Internet access on first configure.
- **Generator mismatch from earlier attempts:** after verifying no important files were placed there, delete the generated \`build-a07/\` folder and rerun.
- **Audio device unavailable:** select a working Windows output device, unplug/replug if needed, and rerun. The generated archive will contain an \`AUDIO_SMOKE_NO_DEVICE\` diagnostic.
- **Pull blocked by local edits:** review and commit/stash your changes manually; the script intentionally does not modify them.
- **Other error:** attach the archive and include the visible error line.

For advanced environments, \`LOCUS_CMAKE_GENERATOR\` can override the default \`Visual Studio 17 2022\`; ensure that generator and its compiler are correctly installed.

## Engineering priorities

1. **Accurate rhythm timing and minimal input/audio latency**, measured where possible.
2. **Reliable and performant rendering/audio behavior** across frame rates.
3. **Maintainable, bounded scope and dependency complexity**; avoid unnecessary frameworks and uncontrolled code growth.
4. Reasonable package size, startup, and memory footprint are **secondary metrics**, **not hard size limits**. A build of tens or hundreds of megabytes is acceptable if justified by clear benefits to timing, stability, or maintainability.

Project content, source, comments, and documentation use **English**.

## HUMAN GATE REQUIRED

When a task cannot be verified without the owner (audio-device tests, usability checks, key design decisions, licenses, etc.), the assistant must call it out **immediately** in its answer using the prominent uppercase heading **HUMAN GATE REQUIRED**, not wait silently for the scheduled end-of-stage review. See the policy in [GDD §10](mvp/GDD.md) and [ROADMAP](mvp/ROADMAP.md).

The first such check is the current **A-07 audio-device test**. A-07 remains open until its missing human observations have been reviewed.
