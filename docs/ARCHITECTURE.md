# LOCUS — Architecture audit

Reviewed: 2026-11-06. Scope: current Windows/raylib rhythm game, not prospective new features.

## Module boundaries

| Area | Ownership and contract |
| --- | --- |
| `src/core` | Timing, eight-heading route geometry, turn judgement, chart parsing and the editor model. No raylib window or Windows file dialogs. |
| `src/app/route_renderer.*`, `gameplay_ui.*`, `chart_editor_ui.*` | Convert one continuous model-space route into gameplay/editor visuals. Both views share the same normalized geometry; editor view is grid-free. |
| `src/app/app_input.*`, `menu_controller.*`, `screen_flow.*` | Collect input, handle menu interactions, and resolve screen navigation. |
| `src/app/chart_playback.*`, `game_session.*`, `local_music.*`, `demo_transport.*` | Playback clocks, scored sessions, audio streams and the demo audio device. |
| `src/app/chart_file.*`, `chart_save.cpp`, `song_library.*` | Validate paths, persist authored charts and manage installed songs. |
| `src/app/main.cpp` | Application composition, UI flow and resource lifetime. This remains the primary coupling point. |

## Completed stabilization

- Chart loading prepares and validates replacement state before replacing active game/editor state.
- Screen flow, input, menu actions, viewport mapping, gameplay drawing and route geometry have separate modules.
- Chart v2 timestamps are authoritative in song milliseconds; editor snapping and visualization do not rewrite timing.
- Movement speed is independent of BPM, snap subdivisions and heading; diagonals are Euclidean unit steps.
- **2026-11-06:** Visible-route range sampling replaces repeated full-history scans for each preview segment. Random-access geometry remains available. The dense 240-tick window regression compares batched and standalone samples.
- **2026-11-08:** `EditorDiscardGuard` now owns unsaved-draft confirmation state (Home/New/Open), filtering unrelated input while confirmation is pending. The global OS quit modal retains priority; headless tests cover approval, cancellation and close interception.
- **2026-11-08:** Quit-confirmation audio pausing is coordinated by `quit_media.*`. The policy records which active stream the modal paused and restores only that stream after cancellation. Headless tests cover Play, Listen, Editor, pre-paused/stopped audio, and repeated modal frames.
- **2026-11-07:** Screen-boundary lifecycle side effects live in `session_transition.*`, independent of screen navigation decisions. Tests exercise Editor → TEST → Results → retry → Editor, demo fallback, and new library chart loads without a desktop.
- **2026-11-07:** `AudioDevice` is the sole raylib audio owner, constructed before borrowed `DemoTransport`/`LocalMusic` streams and closed after they unload. A demo decode failure cannot shut down the local song's output; headless lifecycle checks cover absence of an audio device.
- **2026-11-06:** Editor document loading/saving is isolated in `src/app/editor_document.*`. Failed loads validate their replacement chart, audio decoder and timeline before swapping; failed saves do not discard the active editor draft. Headless tests verify rejection paths.
- **2026-11-06:** Import stages each audio version into an immutable content-fingerprinted file before publishing chart JSON atomically. A crashed or failed import can leave an unreferenced version, but cannot silently change audio bytes used by previously installed difficulties. Each difficulty owns its selected audio path, while the song browser groups versions under one logical song.
- Windows CI builds Release, runs CTests and headless checks, checks worktree cleanliness, exercises the actual BAT launcher, and uploads diagnostics. GUI behavior and acoustic latency are not proven by CI.

## Outstanding work, in priority order

### P1 — Safer import and storage lifecycle

The chart JSON is the sole publication point. Imported audio is immutable and chart references remain valid through interrupted updates; existing v1/v2 JSON and installed audio files stay supported. New imports store audio under `.locus-versions/<16-hex fingerprint>/<original audio filename>`. Remaining limitations: an interrupted import may leave unused staged/revision files; do not garbage-collect versions without cross-process safety. Test permission and power-loss paths on a real Windows machine.

### P1 — Reduce remaining `main.cpp` session orchestration

Editor persistence, screen transitions and the quit-modal audio policy have been extracted and covered by headless tests. `main.cpp` still handles file dialogs, editor commands, global quit modal button input, song selection and in-play actions. The editor's unsaved-document confirmation state is now isolated; next target is one cohesive file-dialog/selection workflow, preserving atomic chart loading and stable references.

### P2 — Validate real-device audio lifecycle

`AudioDevice` now has explicit ownership and is shut down after both transports. Device-absent headless tests cover failure and idempotent shutdown. Physical device hot-plug, initialization failure recovery and real audio-stream output require interactive Windows testing; CI cannot validate acoustic playback. Keep the song clock independent of rendered frames.

### P2 — Profile long-chart frame work

The visible renderer now samples a range in O(events before view + visible ticks), rather than O(events before view × visible ticks). It still reconstructs history for each frame. Benchmark dense 65k-turn charts on the actual target Windows hardware; only if this is measurable, introduce a compiled immutable route index with explicit invalidation on editor chart revisions. Do not cache by chart pointer alone.

### P2 — File and desktop QA

Check Unicode paths, permission errors, full disks, canceled native dialogs, and clean shutdown after failed audio decode or window/audio initialization. Validate font fallback and 800×450 / portrait / ultrawide presentation on a real desktop. Existing headless CI cannot verify frame presentation or device-level timing.

## Non-goals

No new chart semantics, speculative plugin architecture, per-beat heap cache, dependence on a network service, or changes to gameplay judgement to pursue architectural tidiness. Favor small, independently testable commits and preserve existing local songs, chart v1/v2 support, skins and keybinds.
