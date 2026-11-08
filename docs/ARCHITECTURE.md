# LOCUS — Architecture audit

Reviewed: 2026-11-08. Scope: current Windows/raylib rhythm game, not prospective new features.

## Module boundaries

| Area | Ownership and contract |
| --- | --- |
| `src/core` | Timing, eight-heading route geometry, turn judgement, chart parsing and the editor model. No raylib window or Windows file dialogs. |
| `src/app/route_renderer.*`, `path_mesh.*`, `gameplay_ui.*`, `chart_editor_ui.*` | Convert one continuous model-space route into gameplay/editor visuals. Both views share the same normalized geometry; editor view is grid-free. |
| `src/app/app_input.*`, `menu_controller.*`, `screen_flow.*` | Collect input, handle menu interactions, and resolve screen navigation. |
| `src/app/audio_device.*`, `chart_playback.*`, `game_session.*`, `local_music.*`, `demo_transport.*` | Explicit shared audio-device lifetime, playback clocks, scored sessions and non-owning streams. |
| `src/app/chart_file.*`, `chart_save.cpp`, `song_library.*`, `audio_revision.*` | Validate paths, persist authored charts and manage immutable audio revisions. |
| `src/app/editor_session.*`, `editor_document.*`, `editor_file_actions.*` | Editor preview/TEST, transactional file handling, and injected native dialogs. |
| `src/app/listen_session.*`, `skin_profile_actions.*`, `library_playback.*`, `library_browser.*` | Cohesive user-facing session workflows with testable boundaries. |
| `src/app/main.cpp` | Application composition, UI flow and resource lifetime. This remains the primary coupling point. |

## Completed stabilization

- **2026-11-08:** Enabled VSync by default and removed the fixed 60-FPS presentation cap. Phone frames showed a horizontal screen-tearing boundary crossing moving path segments, especially vertical segments during horizontal camera movement. Rendering now requests display-synchronized swaps and uses 240 FPS only as a fallback ceiling. Timing and chart geometry remain clock-based; verify live on the target monitor because a GPU driver may override the swap interval.


- **2026-11-08:** The scored preview draws a tapered `path_mesh` triangle ribbon, with shared joins and interpolated vertex opacity rather than independently rasterized `DrawLineEx` fragments. Turns up to 135° use bounded miters; 180° reversals split runs at an identical junction. Mesh tests cover fractional and dense ticks, continuity, fading and reverse overlays. The editor authoring preview is unchanged.

- Chart loading prepares and validates replacement state before replacing active game/editor state.
- Screen flow, input, menu actions, viewport mapping, gameplay drawing and route geometry have separate modules.
- Chart v2 timestamps are authoritative in song milliseconds; editor snapping and visualization do not rewrite timing.
- Movement speed is independent of BPM, snap subdivisions and heading; diagonals are Euclidean unit steps.
- **2026-11-06:** Visible-route range sampling replaces repeated full-history scans for each preview segment. Random-access geometry remains available. The dense 240-tick window regression compares batched and standalone samples.
- **2026-11-08:** `SkinProfileActions` isolates Load/Save/Save As/Reset and owns skin draft-vs-active publication. Native file pickers are injected. A missing user-selected skin is a hard error (preserves the working draft), while a missing startup profile intentionally uses defaults; tests cover both cases, canceled dialogs, exporting and invalid drafts.
- **2026-11-08:** `ListenSession` owns the standalone WAV/MP3 picker and listening controls. Canceled/failed selections leave the old audio intact; clock timestamps are refreshed after native dialogs. Headless tests cover the no-device and canceled-dialog paths.
- **2026-11-08:** `EditorFileActions` coordinates New/Open/Attach/Save and library editing with injected native file pickers. Opening or decoding failures preserve the active draft; successful Save still imports an installed library copy. Tests cover cancel, missing chart/audio, rejected save and New Draft cleanup.
- **2026-11-08:** `EditorSession` owns ROUTE input/preview interaction and the atomic preparation of Editor → TEST. Preview follows the existing audio-time clock; headless tests cover invalid song duration, playhead boundaries and unavailable audio without changing the current scored chart.
- **2026-11-08:** Library import and browser refresh are coordinated by `library_browser.*`: refresh keeps a selected difficulty by its installed path (even if library sorting changes), and failed imports retain the previous selection/status. Native file dialogs remain in the UI composition.
- **2026-11-08:** Library difficulty activation lives in `library_playback.*`. It decodes replacement audio and validates a copy of the chart BEFORE committing to the active chart/audio objects, keeping game-session references stable. Headless tests cover invalid duration, out-of-range turns, per-difficulty audio reuse and failed audio loading.
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

### Stage 1 architecture boundaries — complete

Editor document lifecycle, preview and TEST; standalone listening; library play/import;
skin profiles; quit modal; and screen-boundary session lifecycle now have distinct
modules and independent headless tests. The entry point deliberately retains
composition, global input, frame rendering and PLAY-screen glue. This is not a
promise that `main.cpp` will never grow: move code when a cohesive new feature
or a demonstrated regression justifies it, rather than chasing a line-count goal.

### P2 — Validate real-device audio lifecycle

`AudioDevice` now has explicit ownership and is shut down after both transports. Device-absent headless tests cover failure and idempotent shutdown. Physical device hot-plug, initialization failure recovery and real audio-stream output require interactive Windows testing; CI cannot validate acoustic playback. Keep the song clock independent of rendered frames.

### P2 — Profile long-chart frame work

The visible renderer now samples a range in O(events before view + visible ticks), rather than O(events before view × visible ticks). It still reconstructs history for each frame. Benchmark dense 65k-turn charts on the actual target Windows hardware; only if this is measurable, introduce a compiled immutable route index with explicit invalidation on editor chart revisions. Do not cache by chart pointer alone.

### P2 — File and desktop QA

Check Unicode paths, permission errors, full disks, canceled native dialogs, and clean shutdown after failed audio decode or window/audio initialization. Validate font fallback and 800×450 / portrait / ultrawide presentation on a real desktop. Existing headless CI cannot verify frame presentation or device-level timing.

## Non-goals

No new chart semantics, speculative plugin architecture, per-beat heap cache, dependence on a network service, or changes to gameplay judgement to pursue architectural tidiness. Favor small, independently testable commits and preserve existing local songs, chart v1/v2 support, skins and keybinds.
