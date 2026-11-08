# LOCUS — Architecture audit

Reviewed: 2026-11-08. Scope: current Windows/raylib rhythm game, not prospective new features.

## Module boundaries

| Area | Ownership and contract |
| --- | --- |
| `src/core` | Timing, eight-heading route geometry, turn judgement, chart parsing and the editor model. No raylib window or Windows file dialogs. |
| `src/app/route_renderer.*`, `path_mesh.*`, `gameplay_ui.*`, `chart_editor_ui.*` | Convert one continuous model-space route into gameplay/editor visuals. PLAY/TEST uses an 800 ms default song-time preview; its path, turn dots and 180° reverse caps share proximity-based fades and scaling. Authored geometry/judgement are unchanged. |
| `src/app/app_input.*`, `menu_controller.*`, `screen_flow.*` | Collect and route native input. The exit modal and editor BACK hitbox are handled before screen commands, with shared viewport-aware geometry and a headless pointer test. |
| `src/app/audio_device.*`, `chart_playback.*`, `game_session.*`, `local_music.*`, `demo_transport.*` | Explicit shared audio-device lifetime, playback clocks, scored sessions and non-owning streams. Chart audio is not stopped at scoring completion: uncharted tails stream through Results until real audio EOF. |
| `src/app/menu_music_preview.*`, `audio_volume_fade.h`, `transition_easing.h` | Dedicated Song Select audio decoders and headless-testable nonlinear fade envelopes. A shared C1-smooth curve drives music transitions, screen fades and final-route movement. Sources fade sequentially; menu preview never seeks the scored decoder. |
| `src/app/play_outro_transition.h`, `screen_fade_transition.h` | Headless-tested presentation transitions. The last-note outro retains heading/camera and provides a longer eased Results blackout. Regular screen changes complete a 65+95 ms visual fade while input/audio switch immediately; entering PLAY reveals its live frame directly in 100 ms. No judgement ownership. |
| `src/app/chart_file.*`, `chart_save.cpp`, `song_library.*`, `song_set_storage.*`, `song_archive.*` | Validate chart-relative paths, serialize per-library/per-Set writers, persist audio and optional artwork, back up overwrites, and manage recoverable Trash. Optional per-Set `previewStartMs` and `backgroundFile` are manifest metadata, independent of note timing. ZIP archives carry the safely validated image along with charts and audio. |
| `src/app/editor_session.*`, `editor_document.*`, `editor_file_actions.*`, `editor_entry_policy.h` | Editor preview/TEST, transactional file handling and injected native dialogs. Home creates a blank draft; Song Select EDIT opens the selected difficulty; Results EDIT reopens the played chart; Editor TEST retains its unsaved document. |
| `src/app/listen_session.*`, `skin_profile_actions.*`, `library_playback.*`, `library_browser.*` | Cohesive user-facing session workflows with testable boundaries. |
| `src/app/app_runtime_policy.*` | Pure screen navigation and judgement-completion policy, independent of rendering and file dialogs. |
| `src/app/play_screen_actions.*`, `play_adjustments.h` | Play-only pause/retry/practice input, transition start at final note judgement, playback snapshots, and pure speed/preview hotkey repeat/step rules. Held adjustments use the editor's deterministic delay and acceleration; frequent global-skin writes are coalesced. |
| `src/app/editor_screen_actions.*` | Editor-only New/Open/Attach/Save/TEST command routing and SET tab events, with deterministic precedence and no screen navigation ownership. |
| `src/app/app_smoke_checks.*` | Headless integration checks behind the existing `--loop-check`, `--screen-check`, and `--session-check` entry points. |
| `src/app/app_frame_renderer.*`, `app_quit_dialog_layout.h`, `image_asset_texture.*`, `image_cover_geometry.h` | Rendering and OS font selection. Reusable GPU textures load from native-path bytes (Unicode-safe), keep aspect ratio using a headless-tested cover crop and unload before the window closes. The quit confirmation shares hitboxes between renderer and input. |
| `src/app/library_file_actions.*`, `image_asset_file.h` | Song library file commands (import/export/copy/Trash/restore/background) and injected native pickers. Reusable file guards reject unsafe image paths and signatures, ready for future skin/UI images without coupling them to song data. |
| `src/app/library_launch_actions.*` | SongSelect launch decisions and effects: pure path-based intent selection and a focused Edit/Play/NewDifficulty coordinator. |
| `src/app/main.cpp` | Composition root, explicit resource lifetime, cross-screen ordering and frame rendering call. Keeps the necessary high-level coordination rather than hiding it in an all-knowing Application object. |

## Architecture-first refactoring contract

The objective is not an arbitrary `main.cpp` line count. A newly extracted
1,000-line `Application` object with references to every subsystem would
still be a God Object. Every extraction must establish **one coherent owner**
and reduce the number of unrelated reasons a file can change.

- **Domain model:** `src/core` owns judgement, time, route geometry, chart
  parsing, and authoring semantics. It must not depend on raylib UI, Windows
  file dialogs, `SongLibrary`, or the rendering subsystem.
- **Presentation:** renderers receive read-only views. They never import
  chart files, mutate editor documents, or dispatch gameplay events. Shared
  click geometry belongs in a small layout contract, not a private renderer
  constant accessed by global input code.
- **Screen workflows:** library, skin, editor, and gameplay actions use their
  own narrowly scoped state and dependencies. Native pickers and time sources
  are injected for reproducible tests. A screen controller should not hold
  all application services by default.
- **Lifetime:** the composition root creates and destroys resources in a
  fixed order; the audio device outlives streams; a library refresh invalidates
  pointers to prior cached chart/song records.
- **Persistence:** on-disk writes remain transactional and serialized;
  refactoring must not introduce duplicate save paths, change chart semantics,
  or remove recovery safeguards unintentionally.
- **Regression gate:** small direct-`main` commits, Windows Release, CTest,
  CLI diagnostics, and the BAT launcher. Verify visual/audio behavior on a
  desktop separately; a headless pass cannot prove it.

### Remaining coupling to remove

1. `main.cpp` retains window/audio lifetime, settings/skin changes,
   cross-screen navigation, render-view assembly and the frame ordering.
   Play controls/scoring, editor file commands, library launch/file commands
   and native quit-input mapping now have separate owners. This is an
   intentional boundary: only extract more if a new feature proves the
   coupling harmful, rather than moving 400 lines into a God Object.
2. `chart_editor_actions.cpp` combines editor keyboard/mouse interaction,
   timeline movement, route edits and selection state. Split these by
   input operation and preserve its pure action tests.
3. `skin_menu.cpp` combines palette math, personal profile UI, live preview,
   drawing and text-input interaction; distinguish color-editor interaction
   from rendering and profile persistence.
4. `song_library.cpp` combines scanning, set identity, import/installation,
   mutation and removal; move storage operations behind focused interfaces
   and preserve lock ordering and data-durability tests.
5. `app_frame_renderer.cpp` still has a broad frame signature. Make
   per-screen view types as screens gain features; resist a mutable
   giant frame-state DTO.

## Completed stabilization

- **2026-11-08:** Enabled VSync by default and removed the fixed 60-FPS presentation cap. Phone frames showed a horizontal screen-tearing boundary crossing moving path segments, especially vertical segments during horizontal camera movement. Rendering now requests display-synchronized swaps and uses 240 FPS only as a fallback ceiling. Timing and chart geometry remain clock-based; verify live on the target monitor because a GPU driver may override the swap interval.


- **2026-11-08:** The scored preview draws a tapered `path_mesh` triangle ribbon, with shared joins and interpolated vertex opacity rather than independently rasterized `DrawLineEx` fragments. Turns up to 135° use bounded miters; 180° reversals split runs at an identical junction. Mesh tests cover fractional and dense ticks, continuity, fading and reverse overlays. The editor authoring preview is unchanged.

- Chart loading prepares and validates replacement state before replacing active game/editor state.
- Screen flow, input, menu actions, viewport mapping, gameplay drawing and route geometry have separate modules.
- Chart v2 timestamps are authoritative in song milliseconds; editor snapping and visualization do not rewrite timing.
- Movement speed is independent of BPM, snap subdivisions and heading; diagonals are Euclidean unit steps.
- **2026-11-06:** Visible-route range sampling replaces repeated full-history scans for each preview segment. Random-access geometry remains available. The dense 240-tick window regression compares batched and standalone samples.
- **2026-11-08:** `SkinProfileActions` isolates Load/Save/Save As/Reset, personal profile scanning and activation under `Skins/Profiles`, and owns skin draft-vs-active publication. Native file pickers are injected. A missing user-selected skin is a hard error (preserves the working draft), while a missing startup profile intentionally uses defaults; tests cover both cases, canceled dialogs, exporting and invalid drafts.
- **2026-11-08:** `ListenSession` owns the standalone WAV/MP3 picker and listening controls. Canceled/failed selections leave the old audio intact; clock timestamps are refreshed after native dialogs. Headless tests cover the no-device and canceled-dialog paths.
- **2026-11-08:** `EditorFileActions` coordinates New/Open/Attach/Save and library editing with injected native file pickers. Opening or decoding failures preserve the active draft; successful Save edits the installed chart directly, while Save As creates an independent difficulty. Saving an external chart installs a managed copy, not an extra working file. Tests cover cancel, missing chart/audio, rejected save and New Draft cleanup.
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

### P1 — Song Set durability and recovery

Managed Song Sets have one shared audio file, a manifest and per-difficulty charts. Import and editor Save hold an OS-backed library lock before per-folder write locks. Shared audio is compared byte-for-byte: a conflicting import is refused rather than silently changing older difficulties. Overwrites use a staged, flushed JSON, previous-version backup and on-volume replacement. Deletion moves files or a whole Set to LOCUS Trash with an origin journal; U restores the newest entry. Interrupted processes may leave orphan staging files; cleanup is intentionally conservative. Pending: power-loss, permissions and disk-full fault injection on real Windows; archive import/export and history browsing. There is no pre-release compatibility guarantee.

### Stage 1 architecture boundaries — complete

Editor document lifecycle, preview and TEST; standalone listening; library play/import;
skin profiles; quit modal; and screen-boundary session lifecycle now have distinct
modules and independent headless tests. The 2026-11-08 architecture pass also
isolated headless smoke checks, frame rendering, SongSelect file/launch
commands, Play-session commands and snapshots, Editor-screen file commands,
and modal pointer input. The entry point deliberately retains lifetime
composition, cross-screen orchestration, and the call into the renderer.
Keeping these concerns visible is preferable to creating a new `Application`
God Object. Future splits require a concrete reason, not a line-count goal.

### P2 — Validate real-device audio lifecycle

`AudioDevice` now has explicit ownership and is shut down after both transports. Device-absent headless tests cover failure and idempotent shutdown. Physical device hot-plug, initialization failure recovery and real audio-stream output require interactive Windows testing; CI cannot validate acoustic playback. Keep the song clock independent of rendered frames.

### P2 — Profile long-chart frame work

The visible renderer now samples a range in O(events before view + visible ticks), rather than O(events before view × visible ticks). It still reconstructs history for each frame. Benchmark dense 65k-turn charts on the actual target Windows hardware; only if this is measurable, introduce a compiled immutable route index with explicit invalidation on editor chart revisions. Do not cache by chart pointer alone.

### P2 — File and desktop QA

Check Unicode paths, permission errors, full disks, canceled native dialogs, and clean shutdown after failed audio decode or window/audio initialization. Validate font fallback and 800×450 / portrait / ultrawide presentation on a real desktop. Existing headless CI cannot verify frame presentation or device-level timing.

## Non-goals

No new chart semantics, speculative plugin architecture, per-beat heap cache, dependence on a network service, or changes to gameplay judgement to pursue architectural tidiness. Favor small, independently testable commits and do not silently discard user data. Before the first release, old chart/storage compatibility may be broken explicitly when necessary.
