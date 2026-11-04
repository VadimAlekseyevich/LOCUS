# LOCUS — A-07 raylib Audio/Packaging Evaluation

**Roadmap task:** A-07 — Spike raylib audio time and packaging  
**Current status:** Probe source and measurement procedure prepared; **Windows build and interactive audio results must be verified before A-07 can be accepted**.  
**Pinned candidate:** raylib **6.0** (via CMake FetchContent)  
**Probe:** [raylib_audio](../spikes/raylib_audio/README.md)  
**Scope:** Technical experiment only, not a production engine choice.

## 1. What the experiment measures

| Question | Mechanism | Current evidence |
| --- | --- | --- |
| Can a small C++/raylib program play a local metronome? | Generated 16 s 120 BPM PCM WAV, played via LoadMusicStream / UpdateMusicStream | **Needs interactive Windows playback** |
| Can it show the reported music position? | GetMusicTimePlayed() sampled in GUI and CSV | **Instrumented; Windows runtime unverified** |
| How do captured key-downs compare with reported song position? | A/D actions with std::chrono::steady_clock capture time and GetKeyPressed() polling | **Instrumented; results pending** |
| Are pause and seek coherent? | PauseMusicStream, ResumeMusicStream, SeekMusicStream and recorded actions | **Instrumented; manual behavior pending** |
| Is reported song time stable relative to a monotonic reference? | CSV samples every ~100 ms, reported minus wall-anchored time | **Pending actual runner/operator data** |
| How big is a Windows Release executable? | CMake Release x64 + PowerShell byte count | **Pending Windows CI run** |
| What additional files or runtime are required? | Inspect output DLLs and run on a clean Windows machine | **Pending Windows CI and clean-machine check** |

**Important:** GetMusicTimePlayed() is a raylib stream-position value, not a direct DAC presentation timestamp. GetKeyPressed() is polled each frame and cannot guarantee OS event-capture precision. If these prevent defensible low-latency judgement, the project needs a separate input timestamp/audio-clock design (or a conditional SDL3 comparison); do not claim raylib alone has passed rhythm-game requirements.

## 2. Version and reproducible build

- raylib Git tag: **6.0** (not latest-master).
- CMake: minimum 3.24.
- C++: C++17.
- Target: Windows x64, Visual Studio 2022, Release.
- Compiler/linker options: see the probe's isolated CMakeLists.txt.
- The original WAV is created at runtime and is not committed as a binary.
- A Windows GitHub Actions workflow is intended to capture a headless build result; it cannot hear the audio.

### Local interactive acceptance procedure

1. Build with the commands in the [probe README](../spikes/raylib_audio/README.md), then run the application on Windows with a real output device.
2. Confirm audible, evenly spaced 120 BPM clicks (one every 500 ms) and no premature termination.
3. Observe GetMusicTimePlayed() increasing during playback; compare the displayed steady-clock estimate.
4. Press A/D on or near ticks. Confirm each accepted key-down prints and writes a CSV row.
5. Pause for at least 2 seconds; song position must remain stable and the displayed expected clock must freeze.
6. Resume, then seek backward and forward multiple times; check the reported position settles near the requested value without freezes/crashes.
7. Restart and confirm playback starts from the first tick.
8. Close and inspect raylib_probe_log.csv for jumps/drift; note device/driver and sample-rate details.
9. Repeat as appropriate under reduced frame rate or deliberate graphics load. Report whether key-down polling delays make a direct judgement implementation unsuitable.

Do not mark **passed** merely because the executable compiles or the WAV header self-test passes.

## 3. Windows build and packaging measurements

| Measurement | Result | Evidence |
| --- | --- | --- |
| Windows x64 Release build | **Not verified yet** | GitHub Actions / Windows toolchain output pending |
| Headless WAV self-test | **Not verified yet** | Windows CI / operator output pending |
| Release .exe size (bytes) | **Not measured yet** | CI windows-build-metrics.txt |
| Bundled non-system DLL size (bytes) | **Not measured yet** | CI artifact inventory |
| Generated metronome.wav size | **Expected 1,536,044 bytes**, by PCM header calculation; **not an observed Windows result** | 16 s × 48,000 samples/s × 2 bytes + 44-byte WAV header |
| Portable/clean Windows machine launch | **Not tested yet** | Manual installation check required |
| Audible WAV playback | **Not tested yet** | Manual interactive run required |
| Input timestamp granularity | **Not measured yet** | CSV + polling observation |
| Pause/seek drift and jumps | **Not measured yet** | CSV + operator observation |

Record the actual Windows runner URL, Visual Studio version, build SHA, sizes, and any dependency DLLs here when results exist. A headless runner can establish **build feasibility**, but not low-latency audio correctness on a typical player's machine.

## 4. Technical risks discovered in design review

1. **Stream position is not output-device latency.** raylib's music getter may represent decoded/processed frames in a buffered stream, not the instant heard through speakers.
2. **Polling is not an event timestamp.** The probe records the earliest available monotonic timestamp during GetKeyPressed() polling; it can still lag a physical key press by up to a polling interval or scheduling delay.
3. **Float time precision and seek granularity.** The raylib API reports song times and takes seek positions as float seconds; investigate precision and seek behavior on longer tracks before approving an authoritative timing model.
4. **Frame stalls.** Long render frames may also delay UpdateMusicStream() and keyboard polling; log and retest, rather than assuming a smooth audio clock.
5. **Windows redistributables.** Static-linking raylib does not necessarily eliminate the CRT or Windows/system libraries; a clean-machine launch determines the actual package.
6. **Original test audio.** The generated metronome is original and safe to include only through source; no copyrighted music files are uploaded.

## 5. Go / no-go decision (A-09, not A-07)

**Do not choose the production stack yet.** A-07 only records measured facts and blockers. A-08 (SDL3 probe) is conditional on an unresolved material failure, and A-09 makes the actual library decision.

| Criterion | A-07 result |
| --- | --- |
| Clean Windows build succeeds | Pending |
| Release size and dependencies measured | Pending |
| Music plays correctly on an actual device | Pending |
| Pause/seek/restart behave predictably | Pending |
| Reported time stable enough to support a suitable clock adapter | Pending |
| Polling timestamps/latency limitations are understood | Pending |

**Roadmap status remains pending until the task's Windows build/run, observations, and concrete size numbers are verified.**
