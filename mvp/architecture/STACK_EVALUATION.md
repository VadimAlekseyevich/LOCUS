# LOCUS — A-07 raylib Audio/Packaging Evaluation

**Roadmap task:** A-07 — Spike raylib audio time and packaging  
**Current status:** Windows x64 Release **build and headless WAV self-test passed**. The optional Windows audio API smoke test returned **AUDIO_SMOKE_NO_DEVICE** because the hosted runner had no audio output device. Interactive audible playback, physical input latency, and a clean end-user Windows machine launch remain unverified. This is a **test-environment blocker**, not evidence of a raylib audio failure. Do not confuse the passing build with proof of rhythm-grade timing.  
**Pinned candidate:** raylib **6.0** (via CMake FetchContent)  
**Probe:** [raylib_audio](../spikes/raylib_audio/README.md)  
**Scope:** Technical experiment only, not a production engine choice. **Primary evaluation: timing stability, audio/input latency, and frame behavior.** Package size is secondary; there is no kilobyte or megabyte limit.

## 1. What the experiment measures

| Question | Mechanism | Current evidence |
| --- | --- | --- |
| Can a small C++/raylib program play a local metronome? | Generated 16 s 120 BPM PCM WAV, played via LoadMusicStream / UpdateMusicStream | **Interactive Windows playback still unverified** |
| Can it show the reported music position? | GetMusicTimePlayed() sampled in GUI and CSV | **Instrumented; interactive playback still unverified** |
| How do captured key-downs compare with reported song position? | A/D actions with std::chrono::steady_clock capture time and GetKeyPressed() polling | **Instrumented; manual results pending** |
| Are pause and seek coherent? | PauseMusicStream, ResumeMusicStream, SeekMusicStream and recorded actions | **Audio API smoke could not run:** hosted runner reported AUDIO_SMOKE_NO_DEVICE; manual device test needed |
| Is reported song time stable relative to a monotonic reference? | CSV samples every ~100 ms, reported minus wall-anchored time | **No acoustic or interactive measurement yet** |
| How big is a Windows Release executable? | CMake Release x64 + PowerShell byte count | **979,968 bytes** (latest source, Windows 2022 Release; run 41886414912) |
| What additional files or runtime are required? | Inspect output DLLs and run on a clean Windows machine | **0 adjacent DLLs** in built Release folder; **clean-machine test pending** |

**Important:** GetMusicTimePlayed() is a raylib stream-position value, not a direct DAC presentation timestamp. GetKeyPressed() is polled each frame and cannot guarantee OS event-capture precision. If these prevent defensible low-latency judgement, the project needs a separate input timestamp/audio-clock design (or a conditional SDL3 comparison); do not claim raylib alone has passed rhythm-game requirements.

## 2. Version and reproducible build

- raylib Git tag: **6.0** (not latest-master).
- CMake: minimum 3.24.
- C++: C++17.
- Target: Windows x64, Visual Studio 2022, Release.
- Compiler/linker options: see the probe's isolated CMakeLists.txt.
- The original WAV is created at runtime and is not committed as a binary.
- A Windows GitHub Actions workflow is intended to capture a headless build result; it cannot hear the audio.

### Owner's one-command local runner

**HUMAN GATE REQUIRED (EARLY):** The GitHub-hosted Windows runner has no audio output endpoint; the owner must verify real playback and input feel on an audio-capable Windows machine **before marking A-07 complete**.

Use **Git Bash** in the cloned repository root:

~~~bash
bash run-a07.sh
~~~

The script fast-forward pulls Git, builds the isolated probe, performs WAV and audio-device smoke tests, starts the GUI, asks brief subjective questions, and creates \`.locus-runs/a07-.../LOCUS-A07-logs.tar.gz\`. Upload that archive to the assistant. Setup and error instructions are in [the repository README](../../README.md). The script never uploads logs itself and will not overwrite uncommitted changes.

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
| Windows x64 Release build | **PASS** | [Windows run 41886414912](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41886414912), MSVC 2022, raylib 6.0 |
| Headless WAV self-test | **PASS** | Windows self-test output: SELF_TEST_PASS |
| Release .exe size (bytes) | **979,968 bytes** (957 KiB) | Windows CI windows-build-metrics.txt, latest probe with audio-smoke mode |
| Bundled non-system DLL size (bytes) | **0 bytes (0 adjacent DLLs)** | Build folder inventory; does not exclude Windows/CRT dependencies |
| Generated metronome.wav size | **1,536,044 bytes (measured)** | Windows self-test + CI metrics; 16 s × 48,000 × 2 + 44 bytes |
| Portable/clean Windows machine launch | **Not tested yet** | Manual installation check required |
| Audible WAV playback | **Not tested yet** | Manual interactive run required |
| Input timestamp granularity | **Not measured yet** | CSV + polling observation |
| Pause/seek drift and jumps | **Not measured yet** | CSV + operator observation |

**Measured build provenance:** GitHub Actions [run 41886414912](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41886414912), commit `06d8e530759bbdf83414c3cb56e4f354e799a4be`, Windows 2022 hosted image, Visual Studio 2022 MSVC x64 Release, pinned raylib tag 6.0. Windows built static raylib archive: **5,324,650 bytes** (intermediate library, not an extra distributed file). Earlier initial run on Windows 2025 failed to locate VS 2022; choosing the Windows 2022 image fixed that configuration problem. A headless runner can establish **build feasibility**, but not low-latency audio correctness on a typical player's machine.

### Windows hosted-runner audio limitation

The optional `--audio-smoke` command ran on the Windows 2022 CI host but reported **`AUDIO_SMOKE_NO_DEVICE: audio output unavailable`** at `InitAudioDevice()`. The workflow intentionally treats this optional step as non-blocking, since GitHub-hosted runners are not guaranteed to expose a speaker/audio endpoint. As a result, **Play/Pause/Seek/Resume behavior and audible click timing remain unmeasured**, rather than failing or passing. The actionable next test is the same binary on a real Windows computer with an active audio output device.

## 4. Technical risks discovered in design review

1. **Stream position is not output-device latency.** raylib's music getter may represent decoded/processed frames in a buffered stream, not the instant heard through speakers.
2. **Polling is not an event timestamp.** The probe records the earliest available monotonic timestamp during GetKeyPressed() polling; it can still lag a physical key press by up to a polling interval or scheduling delay.
3. **Float time precision and seek granularity.** The raylib API reports song times and takes seek positions as float seconds; investigate precision and seek behavior on longer tracks before approving an authoritative timing model.
4. **Frame stalls.** Long render frames may also delay UpdateMusicStream() and keyboard polling; log and retest, rather than assuming a smooth audio clock.
5. **Windows redistributables.** Static-linking raylib does not necessarily eliminate the CRT or Windows/system libraries; a clean-machine launch determines the actual package. This is a deployment consideration, **not a goal to make the entire game tiny**.
6. **Original test audio.** The generated metronome is original and safe to include only through source; no copyrighted music files are uploaded.

## 5. Go / no-go decision (A-09, not A-07)

**Do not choose the production stack yet.** A-07 only records measured facts and blockers. A-08 (SDL3 probe) is conditional on an unresolved material failure, and A-09 makes the actual library decision.

| Criterion | A-07 result |
| --- | --- |
| Clean Windows build succeeds | **PASS** on Windows 2022 hosted runner |
| Release size and adjacent DLLs measured | **PARTIAL**: 979,968-byte EXE, 0 adjacent DLLs; system/CRT imports and clean-machine launch not tested |
| Music plays correctly on an actual device | Pending |
| Pause/seek/restart behave predictably | Pending |
| Reported time stable enough to support a suitable clock adapter | Pending |
| Polling timestamps/latency limitations are understood | Pending |

**Roadmap status:** A-07 remains **pending** because the Windows hosted runner has no audio device and therefore cannot exercise actual raylib playback/pause/seek or physical key timing. The Windows build and headless file test are complete, and precise size evidence is available. The remaining step is the owner's real-device test via **`bash run-a07.sh`** and upload of its diagnostic archive. A-08 is **not automatically triggered** by absent CI audio hardware. Do not judge raylib's suitability by its sub-megabyte spike executable; **correct audio/input timing and stability are the priorities.**
