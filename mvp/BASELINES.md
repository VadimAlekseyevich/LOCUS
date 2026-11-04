# LOCUS — Foundation v0.1 Windows Baselines (F-09)

**Status:** IN PROGRESS — the headless CI measures are automated, but a real interactive Windows desktop is required for the GUI idle CPU/RAM samples. Do not mark F-09 complete before these are actually observed and recorded.

**Code:** [Windows sampler](../tools/measure-baseline.ps1) · [one-click launcher](../build_and_run.bat) · [production CI](../.github/workflows/windows_ci.yml) · [Roadmap](./ROADMAP.md)

## 1. Measurement philosophy

The early priority is **timing correctness, stable frame pacing and low input/audio latency**, not a tiny executable. Package size may be tens or hundreds of megabytes when justified. These are **observation-only baselines**, not release acceptance thresholds, performance guarantees or measured acoustic/input latencies.

Release EXE size excludes Microsoft Windows system libraries, Windows SDK, separately installed fonts, audio tracks, diagnostic archives and build intermediates. A single file size is **not** a full distribution size.

## 2. CI: first-launch and release-executable measurement

- **Platform:** GitHub Actions `windows-2022`, Visual Studio 2022 MSVC, x64 Release, pinned raylib 6.0. GitHub-hosted runner; actual CPU model, logical processors, Windows version/build and physical RAM are recorded by the script.
- **Code identity:** SHA-256 of the measured `locus.exe` and the repository commit are included in the output.
- **Measurement command:** After clean checkout, configuration and Release compilation, the CI job runs:

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/measure-baseline.ps1 -Executable build/windows-foundation/Release/locus.exe -OutputDirectory build/windows-foundation/baseline-ci -Mode Headless
~~~

- **Executable bytes:** `Get-Item` length; not archive or installed footprint.
- **First launch-to-exit:** `Stopwatch` around the *first* `locus.exe --foundation-check` invocation. Measures process start, core-linkage check and process termination **without GUI initialization**. This is not a cold operating-system boot, forced disk-cache purge or visible startup latency; the first run may already benefit from OS cache.
- **Warm repeats:** Five additional launch-to-exit samples, with the median reported.
- **CI idle GUI RAM/CPU:** **NOT MEASURED**, because the hosted Windows runner has no interactive desktop. We do **not** substitute short-lived console test process memory for a real 60 FPS blank window.
- **CI machine evidence:** `build/windows-foundation/baseline-ci/baseline-summary.txt`, `baseline-ci-console.txt`, and `baseline-foundation-output.txt` in the downloadable build artifact.

| CI baseline metric | Measured value | Caveat |
| --- | --- | --- |
| Production Release EXE size | **Pending CI measurement** | Binary only |
| First headless process launch-to-exit | **Pending CI measurement** | No graphics, audio, or cache flushing |
| Median of five warm headless runs | **Pending CI measurement** | Process-launch proxy, not a GUI metric |
| Windows/CPU/RAM of runner | **Pending CI measurement** | GitHub hosted, not owner's machine |
| GUI idle working set | **Not measurable on hosted CI** | Owner machine needed |
| GUI idle CPU | **Not measurable on hosted CI** | Owner machine needed |
| Physical audio/input latency | **Not measured** | Separate T/Q-stage work |

## 3. Real Windows GUI baseline collection

The owner has already confirmed that the window opens/closes and F1–F4 change the placeholder text. For F-09, use **one new measurement** on the owner's Windows machine, with no ZIP upload required when it works.

From Windows Command Prompt in the repository root:

~~~bat
build_and_run.bat baseline
~~~

The BAT fast-forward pulls, compiles **LOCUS Release**, and calls:

~~~powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/measure-baseline.ps1 -Executable build-a07/foundation/Release/locus.exe -OutputDirectory .locus-runs/manual-example -Mode Interactive -SampleSeconds 12
~~~

**Procedure:** Leave the LOCUS window **uninteracted with for 12 seconds** while one sample is collected per second. Then close it using Esc or X when prompted. The launcher prints a human-readable `baseline-summary.txt` and packages it with `baseline-idle-samples.csv` into `LOCUS-game-logs.zip`, saved under the local Git-ignored `.locus-runs/` folder.

- **Time to a detected GUI window:** Stopwatch from launching the actual GUI to the first nonzero Windows `MainWindowHandle`, with a 10-second timeout. This measures existence of an interactive top-level window; it **does not** measure the first fully drawn frame or keyboard-to-pixel latency.
- **Idle RAM:** Mean and maximum `WorkingSet64` (resident process working set) and mean `PrivateMemorySize64` over the quiet window interval, in MiB.
- **Idle CPU:** Process `TotalProcessorTime` deltas / elapsed wall time × 100. **100% denotes the equivalent of one logical processor** rather than the sum across all processors; the number is not an end-to-end latency metric.
- **Environment:** Windows OS/build, CPU model, logical processor count, physical RAM and process SHA256 are recorded automatically. Background apps, display refresh, power scheme and scheduling can affect measurements.
- **Fonts:** The GUI uses Windows-installed Segoe UI where available with Arial and raylib fallback. The owner should also check text readability during the scheduled Stage 2 acceptance.
- **Privacy:** The report includes a local executable path and the user's hardware details. It stays on the local computer and is **not uploaded automatically**. If sharing a log, review paths first. A text-only report of the key numeric metrics is sufficient.

### GUI results (owner's machine)

| Owner observation | Value |
| --- | --- |
| Windows machine/OS build | **Pending owner sampling** |
| Window-handle ready, ms | **Pending owner sampling** |
| Mean GUI idle working set, MiB | **Pending owner sampling** |
| Peak GUI idle working set, MiB | **Pending owner sampling** |
| Mean GUI idle CPU, % of one logical processor | **Pending owner sampling** |
| Clean close | **Pending owner sampling** |
| Updated text legibility | **Pending owner confirmation** |

## 4. Follow-up and interpretation

When the results are available, record **specific values, the observation environment, sample duration, command and exact commit**, and assess whether the program appears stable. Do not invent a target such as `<5 MiB` or `<1 ms` from this minimal rendering prototype. Its frame cap is 60 FPS only for placeholder UI, not the game clock. Audio output, key-to-judgement timestamps, device buffers, and full-game frame pacing will be measured in later T/Q-stage tasks.

**Stage 2 human gate:** After F-09 numbers are recorded, explicitly ask the owner to approve moving from Foundation to Stage 3 (chart domain). Do not advance to C-01 before that approval.
