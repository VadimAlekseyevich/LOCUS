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
- **2026-11-07:** `AudioDevice` is the sole raylib audio owner, constructed before borrowed `DemoTransport`/`LocalMusic` streams and closed after they unload. A demo decode failure cannot shut down the local song's output; headless lifecycle checks cover absence of an audio device.
- **2026-11-06:** Editor document loading/saving is isolated in `src/app/editor_document.*`. Failed loads validate their replacement chart, audio decoder and timeline before swapping; failed saves do not discard the active editor draft. Headless tests verify rejection paths.
- **2026-11-06:** Import stages each audio version into an immutable content-fingerprinted file before publishing chart JSON atomically. A crashed or failed import can leave an unreferenced version, but cannot silently change audio bytes used by previously installed difficulties. Each difficulty owns its selected audio path, while the song browser groups versions under one logical song.
- Windows CI builds Release, runs CTests and headless checks, checks worktree cleanliness, exercises the actual BAT launcher, and uploads diagnostics. GUI behavior and acoustic latency are not proven by CI.

## Outstanding work, in priority order

### P1 — Safer import and storage lifecycle

The chart JSON is the sole publication point. Imported audio is immutable and chart references remain valid through interrupted updates; existing v1/v2 JSON and installed audio files stay supported. New imports store audio under `.locus-versions/<16-hex fingerprint>/<original audio filename>`. Remaining limitations: an interrupted import may leave unused staged/revision files; do not garbage-collect versions without cross-process safety. Test permission and power-loss paths on a real Windows machine.

### P1 — Reduce `main.cpp` session orchestration

`main.cpp` still orchestrates editor commands, song selection, scored-session transitions and quit modal handling. Editor document load/save has been extracted, but most screen/session coordination remains. Extract **one cohesive editor/session coordinator at a time**, retaining stable chart references and the existing atomic load behavior. Do not split functions merely by length or introduce a general-purpose event framework. Add transition tests for Editor → Test → Results → Editor, failed reloads, and quit confirmation during playback.

### P2 — Validate real-device audio lifecycle

`AudioDevice` now has explicit ownership and is shut down after both transports. Device-absent headless tests cover failure and idempotent shutdown. Physical device hot-plug, initialization failure recovery and real audio-stream output require interactive Windows testing; CI cannot validate acoustic playback. Keep the song clock independent of rendered frames.

### P2 — Profile long-chart frame work

The visible renderer now samples a range in O(events before view + visible ticks), rather than O(events before view × visible ticks). It still reconstructs history for each frame. Benchmark dense 65k-turn charts on the actual target Windows hardware; only if this is measurable, introduce a compiled immutable route index with explicit invalidation on editor chart revisions. Do not cache by chart pointer alone.

### P2 — File and desktop QA

Check Unicode paths, permission errors, full disks, canceled native dialogs, and clean shutdown after failed audio decode or window/audio initialization. Validate font fallback and 800×450 / portrait / ultrawide presentation on a real desktop. Existing headless CI cannot verify frame presentation or device-level timing.

## Non-goals

No new chart semantics, speculative plugin architecture, per-beat heap cache, dependence on a network service, or changes to gameplay judgement to pursue architectural tidiness. Favor small, independently testable commits and preserve existing local songs, chart v1/v2 support, skins and keybinds.
