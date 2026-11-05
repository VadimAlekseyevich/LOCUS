# LOCUS — First Playable Prototype Roadmap

**Priority:** Make a game you can actually play on Windows, not an engineering showcase.  
**Revised:** 2026-11-05 at the owner's request.  
**Status:** **First playable prototype accepted by the owner (2026-11-05).** The next goal is creating and playing custom music charts.  
**Design reference:** [GDD](./GDD.md) · [Timing rules](./architecture/TIMING.md) · [Wrap behavior](./architecture/WRAP.md)

> This short roadmap **replaces the former 124-task checklist**. That overly granular plan is available in Git history if ever needed; it is **not** an active queue, gate, or obligation. Existing working code is retained. Finished setup work will not be repeated or counted as future tasks.

## What we want to play first

**First playable = one complete, original demo rhythm chart, launched by double-clicking `build_and_run.bat`.**

- A **4×4 grid** and one marker advancing automatically **one point per quarter-note beat**.
- A short **original/generated 120 BPM metronome-backed demo**, with a fixed series of authored Left/Right turns. No external songs or editor setup required to start.
- **Wrap** at the edges, shown as an exit/re-entry rather than a line cutting across the grid.
- A readable **look-ahead route** showing upcoming motion.
- Two keys (**F = Left** and **J = Right**) graded **Perfect / Good / Bad / Miss**, with immediate visual feedback.
- **No Fail:** missed or incorrect actions never alter the correct route or stop playback.
- A basic start/play/pause/retry/finish loop with results. Close with Esc or X.
- Input judgements and movement use the **same music-time domain**, not frame count. Keep this rule simple and correct; investigate actual latency only if playtesting exposes a real problem.

The first playable may use **one built-in chart** and one generated test track. That is intentional. Custom songs, JSON import and an editor are valuable, but **they must not delay the first game loop**.

## What is already done — do not redo it

- C++17 + raylib 6.0 production window, separate input/update/render phases, and the Home/Play/Editor/Results placeholder states.
- Windows `build_and_run.bat` (double-click to build/run); the original audio diagnostic remains under `audio` mode.
- Reproducible CMake build, basic core test, pinned dependencies and Windows CI. CI compiles and tests without needing an interactive desktop.
- A raylib audio/input prototype that the owner ran successfully; Windows placeholder screen switching also confirmed by the owner.
- More readable Windows system-font rendering (still subject to ordinary visual feedback).

**No further architecture document, dependency comparison, benchmark, deployment pipeline or test framework is required before implementing gameplay.** Reuse the existing foundation.

## Next: seven meaningful gameplay slices

Each item should produce visible or audible progress. Add focused automated tests **as part of the feature** where they catch real bugs. Do not split it into a dozen sub-tasks merely to increase the count.

- [x] **G-01 — Make a real demo chart and deterministic route.**  
  Add the first built-in 4×4 example with a start cell, heading, constant BPM and several Left/Right events. Implement the pure route calculation (turn before step; Wrap at edges) so the same beat always yields the same cell and heading. Verify a few turns and all edge directions. **Result:** a known correct route, independent of player keys.
  **Verified (2026-11-05):** Implemented a 32-beat, 120 BPM built-in chart with nine authored turns, pure integer-beat route computation in `src/core/route.h` and `route.cpp`, and focused `tests/route_tests.cpp` tests for turn-before-step, all four Wrap directions, beat-zero turning and deterministic samples. [Windows CI run 41913241034](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41913241034) passed **2/2 CTest tests** including `locus_demo_route`, and the production app still builds. The route exists in the core but is **not yet drawn on screen**; that is G-02.

- [x] **G-02 — Draw the game field and marker.**  
  Show 16 clearly spaced dots and the marker on the Play screen, using the existing raylib window and legible type. Keep Home/Editor/Results placeholders as needed; do not build menus now. **Result:** the game field is recognizably LOCUS, not a screen name.
  **Verified (2026-11-05):** The Play screen now shows 16 evenly spaced grid points, subtle guide lines, and a bright stationary marker using the built-in demo chart's start cell `(1,1)`. The existing Windows-readable UI font and F2 debug navigation are retained. [Windows CI run 41914371229](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41914371229) passed the Release build, **2/2 CTest** cases, and headless screen-transition checks. **CI does not visually inspect the desktop.** Audio and musical-time movement deliberately remain G-03/G-04.

- [x] **G-03 — Play the demo track and drive one song clock.**  
  Reuse the proven raylib audio approach and a generated rights-clean click track. Start/stop it reliably; expose song time/beat for game logic rather than accumulating frame deltas. **Result:** audible beat-aligned playback on Windows and a coherent time source.
  **Implemented (2026-11-05):** `src/core/demo_audio.*` synthesizes a 16-second original mono WAV in memory for the built-in 120 BPM / 32-beat demo. `src/core/song_clock.*` defines one monotonic song-time domain independent of render frame count, with seconds/beat conversion. `src/app/demo_transport.*` uses raylib 6.0 `Music` streaming to play on entering Play (F2) and stop when leaving, unloading the decoder before its backing WAV data and the audio device. The Play screen displays song seconds and beat; the marker remains stationary until G-04. [Windows CI run 41915821845](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41915821845) passed **3/3 CTest** cases including generated PCM/clock tests and the full x64 Release build. **Limitation:** hosted CI has no real audio output; acoustic playback on the owner's Windows speakers and latency are not physically verified. The transport clock is anchored to the PlayMusicStream call, not a measured audio DAC presentation timestamp. This limitation does not block working toward a playable demo or justify mandatory benchmarking.

- [x] **G-04 — Move the marker with music and show the route ahead.**  
  Render fractional-beat motion from the deterministic route, a short look-ahead path, and correct seam-split visuals for Wrap. Use the same song time for marker and preview. **Result:** a moving, readable rhythm path even if the player never presses a key.
  **Verified (2026-11-05):** `src/core/route_view.*` defines pure fractional-beat interpolation and seam-split segments for all four Wrap edges. The Play renderer samples the single `DemoTransport::Beat(now)` clock for both marker and a four-step preview: bright cyan current step, fading blue next three beats, and no false cross-grid line on Wrap. The marker reaches the final chart cell at beat 32. [Windows CI run 41916912504](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41916912504) passed the x64 Release build and **4/4 CTest** cases, including `locus_route_view` checks for all Wrap directions, half-beat positions, end-of-song and clamping. Rendering on the owner's actual monitor and physical audio/display alignment are not certified by hosted CI. No A/D judgements have been implemented yet; that is G-05.

- [x] **G-05 — Judge Left/Right presses without steering the route.**  
  Capture fresh A/D key-downs, associate them with song time, and issue Perfect/Good/Bad/Miss with provisional reasonable thresholds. Test early/late/wrong/missing presses and verify the route stays identical. **Result:** player actions have meaningful, consistent feedback and **No Fail** works.
  **Verified (2026-11-05):** Added pure `src/core/judgement.*` with one final grade per authored turn and provisional symmetric timing windows: Perfect ±50 ms, Good ±110 ms, Bad ±180 ms; incorrect input inside an event window and unplayed events become **Miss**, while out-of-window stray keys are ignored. Only fresh A/D key-downs are captured with the transport's monotonic song-time domain. Play now shows upcoming Left/Right turns, live counts and latest judgement. Hits never mutate the const chart or the deterministic G-01 route; returning to Play resets results. [Windows CI run 41931760389](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41931760389) passed x64 Release and **5/5 CTest** checks including `locus_turn_judgement` (early/late/wrong/missing hits, double-hit prevention, restarts, No Fail). Physical keyboard/audio latency and perceived difficulty are **not** measured; refine thresholds during actual playtesting, without mandatory benchmarks.

- [x] **G-06 — Make a complete short play session.**  
  Provide an obvious way to start the demo from Home, show basic judgement counts, handle pause/resume and restart, and display Results at the end. **Result:** a small game that can be played from start to finish, repeatedly.
  **Verified in CI (2026-11-05):** Home now starts the built-in 16-second chart with **Enter**. Play has **Space pause/resume**, **R retry**, **H Home**; the raylib audio stream and pure `SongClock` pause together so marker movement and judgement time stay frozen. At song end every scheduled turn resolves exactly once and the app moves automatically to Results with Perfect/Good/Bad/Miss counts, where **Enter/R** replays and **H** returns Home. Esc/X still closes the window; F1–F4 stay optional debug shortcuts. [Windows x64 CI run 41932910700](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/41932910700) passed **5/5 CTest** checks and `SESSION_FLOW_PASS turns=9 retry=true results=true`; existing No Fail route and BAT game/audio modes remain intact. **Real-device audio, input feel and visual usability are not certified by hosted CI**; these belong to G-07's owner playtest.

- [x] **G-07 — Deliver and playtest the first working prototype.**  
  Keep double-click `build_and_run.bat` launching the game, with no extra setup beyond the existing toolchain. Run the Windows build/tests, then ask the owner to actually play the demo on their PC. Fix **blocking** playability, timing or crashes discovered there. **Result:** owner confirms the demo is playable. **HUMAN GATE REQUIRED only for this real-device acceptance**, or for a genuine earlier blocker.
  **Owner acceptance (2026-11-05):** The owner played the complete updated Windows prototype and explicitly confirmed it is ready. Their Results screen showed **7 Perfect, 2 Good, 0 Bad, 0 Miss** across nine turns. The accepted build includes **F = Left, J = Right**, F11 fullscreen, resizable window, smooth line reveal and proper Wrap. No more approval or ZIP is required for this milestone. [Green Windows CI](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/42025585935): all five CTest tests and headless session checks passed.

**Prototype milestone: complete (7/7).** The seven items above are retained as historical implementation milestones. They are not a new work queue.

## Next milestone: make and play your own levels

**Goal:** Choose a local song, build a chart for it in LOCUS, save it, reopen it, and play it. **Five outcome-based slices, not dozens of microtasks.** Each slice must result in something the owner can actually use. The generated demo stays available as a safe fallback throughout development.

- [x] **M-01 — Play a local music file.** Allow the owner to select a Windows local **WAV or MP3** file, hear playback using the existing game transport, and pause/retry without losing synchronization. Make unsupported-file errors understandable. No online account, uploader, song library, or installation flow. **Done when:** a real song plays in LOCUS while the original demo still works.
  **Implemented (2026-11-05):** Home **O** opens a native Windows WAV/MP3 picker. After selecting a local file, LOCUS enters a separate **Listen** screen and streams the song with **Space pause/resume**, **R replay**, **O change song**, **H Home**, F11 fullscreen and a progress/time display. The built-in F/J chart demo still starts from Home with Enter and retains its original scoring. Win32 Unicode file access supports non-ASCII paths without copying/uploading tracks; the raylib memory decoder retains file data until unload and refuses files over 256 MiB with a readable message. There are **no invented turn judgements** for a selected song without its chart (M-02). [Windows CI run 42029184504](https://github.com/VadimAlekseyevich/LOCUS/actions/runs/42029184504) passed x64 Release, **6/6 CTest**, local WAV/MP3 extension checks, legacy session tests and clean worktree. **Real speaker playback of a user-selected song is not physically verified by hosted CI**; ordinary user feedback may reveal compatibility issues. No benchmark or ZIP is required.

- [ ] **M-02 — Load and play a saved chart.** Implement a minimal **versioned JSON** chart for the existing 4×4 rules: song path, BPM, beat-zero offset, start cell/heading and Left/Right turn events. Use a simple local file-open workflow and clear errors for invalid data; reuse the existing route, visual and judgement engines rather than creating new gameplay logic. **Done when:** a hand-written example chart loads with its song and plays from start to Results.

- [ ] **M-03 — Edit the chart inside LOCUS.** Replace the Editor placeholder with practical controls to set BPM, timing offset, start position/direction, and add/remove/move Left/Right events on a basic beat timeline. Show the familiar grid and route preview while editing. **Done when:** the owner can author a short custom chart without editing JSON manually.

- [ ] **M-04 — Save, reopen and test-play editor creations.** Connect Editor to JSON save/open and Play so changes round-trip correctly. Provide simple errors, avoid losing an unsaved chart accidentally, and reuse the same game clock/route/judgement implementation. **Done when:** create → save → reopen → play → Results works for an original custom level.

- [ ] **M-05 — Make the custom-song version pleasant to use.** Respond to owner playtesting: sensible file selection and feedback, intuitive editor shortcuts, readable UI, and audio/visual offset adjustment **only if the actual songs reveal a need**. Prepare a straightforward Windows build for playing without manually configuring Visual Studio, while preserving `build_and_run.bat` and its optional audio diagnostic. **Done when:** the owner confirms they can play and create a chart with reasonable effort on their Windows PC.

**Next task: M-02 — load a saved JSON chart alongside its audio.** Finish only that feature per "continue development" turn, verify Windows CI, then stop.

## Deliberately deferred

- Cloud storage, online song discovery, accounts, leaderboards, mobile ports, multiplayer, and complex visual skins.
- Variable grid sizes, tempo maps, hold notes and advanced chart gimmicks.
- Full waveform editing, automatic BPM detection, complex settings menus, or mandatory performance/latency benchmarking.
- More project-management gates or a new 100-task checklist.

The [GDD](./GDD.md) describes the longer-term vision. Prefer playable feature delivery and simple owner feedback over prebuilding everything the GDD mentions.

## Working rules

1. **The game comes first.** Make the smallest playable slice, then get feedback and improve it.
2. **Keep the crucial design invariants:** music-driven time, deterministic authored route, Wrap, Left/Right judgements, No Fail. Correctness matters; formal measurements are optional unless a real defect requires investigation.
3. **Tests are practical:** a few headless route/judgement checks and a working Windows build. CI cannot certify audible synchronization or the appearance of a window.
4. **Preserve `build_and_run.bat`.** It should keep launching the latest game with double-click, and retain the audio diagnostic when useful.
5. **Do not silently require a ZIP.** When Windows hands-on testing is genuinely needed, use a prominent **HUMAN GATE REQUIRED** heading and ask for a simple result first.
6. **One useful feature at a time.** Do not prebuild the editor or architecture for an imagined future feature.

**Owner direction (2026-11-05):** Reduce process and task count, keep existing useful changes, **start building the working rhythm game now**.
