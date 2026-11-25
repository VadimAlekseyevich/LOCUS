# LOCUS — Current architecture

This is a map of the *current* code and its remaining boundaries, not a
chronicle of earlier refactors. The chart, song-set and skin formats are
pre-release; incompatible changes are allowed but must be called out.

## Owners and data flow

| Owner | Responsible for | Must not own |
| --- | --- | --- |
| `src/core` | Chart timestamps, musical-grid editing, exact-time `TimedRoute` index, eight-way headings, judgement, difficulty and chart JSON | GPU, native dialogs, filesystem library policies |
| `ChartEditor` | Mutable authoring document, selection, revisioned undo/redo | Saved-file selection, playback device, render UI |
| `EditorComparisonCache` | Revision-cached routes for one focused draft and up to two parked/library difficulties; frame-local read-only render snapshot | Audio transport, chart mutations, persistence |
| `SongLibrary` / `song_set_storage` | Installed Song Sets, authoritative shared tempo map, file transactions, recovery, cache/index | Gameplay scoring or editor keyboard state |
| `ChartPlayback` / `GameSession` | Audio-time transport and per-arrow judgement | Screen rendering, camera, skin settings |
| `PlayPresentation` | Presentation-only camera lag, outro and adjustment notice | Authoritative song time or note results |
| `path_mesh` / `route_renderer` / `gameplay_ui` | Project sampled route into a clipped square, draw ribbons, turn targets and marker | Loading files or changing gameplay state |
| Screen controllers | Play, editor, library, skin and modal commands | Other screens' storage or render internals |
| `main.cpp` | Stable resource lifetime and cross-screen composition | Another all-purpose service layer |

Play: physical input → `GameSession`/`TurnJudge` → read-only
`GameplayView` prepared by `PlayPresentation` → route/marker rendering.
Editor: authoring input → `ChartEditor` revision → one compiled editable route;
other open Song Set documents retain their own editor histories under `EditorFileActions`, while `EditorComparisonCache` compiles their read-only render previews.
All one-to-three panes consume one shared audio-time cursor, and their
editor-only view speed and lookahead from GameSettings; neither opening a reference nor navigating
its picker edits notes. The upper editor note ruler uses BPM/snap ticks and
its own zoom. The bottom whole-song overview is an independent audio-seconds
projection over `ChartEditor::SongDurationSeconds()`, supporting fast
seek without note mutation. All BPM markers read the canonical Song Set
tempo map (fallback: the current draft), rather than individual chart notes.
The tempo map in `core/tempo_map.h` anchors a new local downbeat at each BPM marker; the upper ruler scrolls in fixed song seconds and changes subdivision density locally. Notes retain their exact `timeMs` during reprojection. Song Setup selects a difficulty on the first click and opens it on the second click (or Enter), retaining unsaved-change confirmation.
TEST borrows a prepared draft without writing it. Library: disk → validated
Song Set index → explicit chart/audio activation. Audio streams are unloaded
before the shared audio device; GPU textures before the window.

## Invariants worth keeping

- Scoring reads the unsmoothed authoritative transport cursor, never rendered frame count. Its actual backend (monotonic rate clock or supported consumed cursor) is specified in [Timing contract](TIMING_CONTRACT.md).
  Smoothing and the camera are **presentation only**.
- A diagonal is an authored two-arrow event; each physical arrow receives
  its own timestamp and judgement. Renderer and difficulty calculation must
  agree on the number of physical inputs.
- An authored chart remains unchanged on miss or wrong input. No artificial
  coordinate offsets for reversals; the route is infinite and eight-way.
- Editor draft, active gameplay chart and library cache have different
  lifetimes. Library refresh may invalidate indexed song/chart pointers;
  comparison selection stores filesystem identities, never raw pointers
  across frames. Editor revisions invalidate only the active compiled route.
- Write failures must never discard an unsaved draft or silently overwrite
  another difficulty's audio. A Song Set manifest owns shared timing.
- `SKIN` controls appearance, including fixed visual speed and optional
  camera lag. Chart files do not store skin choices.

## Architectural work, ordered by impact

### 1–2. Exact-time gameplay geometry and indexed route (implemented)

`core::TimedRoute` compiles exact `TurnEvent.timeMs` timestamps into
cumulative run positions and explicit turn events. No gameplay corner is
projected onto the nearest editor tick. `PositionAt` is logarithmic in
the number of events and `Window` visits only visible runs. `PlayPresentation`
compiles once per chart activation and lends the immutable route to camera,
path mesh and junction renderer. The outro freezes the same song timestamp.
Difficulty V4 uses exact timestamps and key masks for physical strain; its independent READ diagnostic approximates local geometry and never contributes to overall.

The world-space scale still uses ticks-per-beat as a *unit conversion*:
the camera applies its inverse, so screen motion is based on real elapsed
song seconds and is independent of snap resolution or BPM. Authoring
`core::RoutePositionAt` and editor grid navigation intentionally remain
tick-based; they are not a second scoring clock. Saved chart version 4 stores exact timestamps plus an explicit tempo map;
Song Set manifest v2 is authoritative for every difficulty.

Distinct authored timestamps can occupy one projected editor tick. Exact-time selection, placement, copy/paste and playback preserve both events; duplicate absolute timestamps are not allowed.

### 3. One read-only route snapshot per frame (implemented)

`RoutePreviewFrame` prepares the marker, exact-time spans and the upcoming
turn window once per PLAY frame and once per comparison pane. The route
ribbon, junctions and camera consume the same source geometry. Marker order
is far-to-near, even when two authored notes occupy the same coordinate.
`TimedRoute::Window` supports caller-owned reusable buffers to avoid repeated
per-frame allocations in gameplay. Standalone/SKIN rendering retains its
convenience overloads.

Any future changes to marker sizing, route symbols, directional hints or
other visible gameplay elements require explicit design approval. SKIN remains
the owner of marker sizes. Rendering must not change scoring or drop events.

### 4. One raw/visual transport snapshot (implemented foundation)

`ChartPlayback::Snapshot` samples the authoritative audio cursor once
and exposes `judgeSeconds`, smoothed `visualSeconds`, practice rate and
transport flags. PLAY and editor rendering consume that frame; per-key
judgement still uses its own captured physical event time. The same visual
timestamp feeds camera, ribbon and turn markers.

Further work: document and measure a bounded visual/judgement drift contract
on real Windows audio hardware and make seek/restart generation explicit
if the existing smoother resets prove insufficient.

### 5. Consolidate state by lifecycle, not file length (medium)

`main.cpp` still coordinates screen transitions, volume fades, backgrounds,
active vs draft skin, audio resources and file paths. Most per-screen actions
already have owners. Avoid replacing it with a huge mutable
`Application` object. Extract a coordinator only when a new feature needs
a narrower lifetime or makes two screen policies disagree.

Camera lag has one runtime control: SKIN camera lag = 0 restores fixed
centering. No build-time experimental switch is needed.

### 6. Persistence/cache correctness at the boundaries (partly implemented)

`SongLibrary::Refresh` now reuses parses only when size, write timestamp
**and a full-file noncryptographic content fingerprint** match. Song Set
tempo publication verifies the same bytes under its writer lock, so an
external same-size/same-timestamp edit cannot silently reuse stale notes.
Known-new or modified chart files are parsed and fingerprinted from the same bytes; unchanged candidates use a streaming fingerprint to avoid parsing and rating again. The published catalog retains compact metadata, ratings and fingerprints rather than note vectors, materializing verified payloads on selection. AsyncLibraryRefresh publishes complete revisions and cancels stale scans; Unicode search and tree projections are cached, and result history uses a separate cancellable worker. CI exercises 8,193 charts and a dense 786,432-note catalog.

Continue physical Windows fault testing for permissions, interrupted
replacement and disk-full recovery. A file fingerprint is not a substitute
for a transactional filesystem or a cryptographic authenticity check.

### 7. Build boundaries and QA

The storage targets in `cmake/storage_targets.cmake` share production code with tests:
`locus_file_io` (byte reads) → `locus_chart_storage` (chart and manifest transactions, archives) → `locus_song_library` (catalog/metadata/materialization) → `locus_library_selection` (headless tree/search model). All depend on the independently linkable `locus_core`; none require raylib or RmlUi. Low-level byte IO is also used by audio without linking the catalog. The application owns orchestration, while tests link only the modules they exercise.

`cmake/runtime_targets.cmake` defines `locus_transport` (device/decoder/clock and pitch-preserving playback), `locus_game_session` (scored session consuming transport), `locus_result_store` (persistent and async result history, no graphics), and separate `locus_path_mesh` / `locus_route_renderer` targets (CPU geometry vs GPU submission). Transport tests inject the existing MusicOutputBackend rather than compiling an alternative implementation. The CPU ribbon profiler links the mesh target without the GPU renderer.

Editor targets in `cmake/editor_targets.cmake` separate authoring input/commands, comparison preparation, canvas rendering, document IO, multi-document workspace, and TEST/preview session. Platform picker/folder adapters, settings and skin model are also shared with tests. Native editor, library/results and skin presenters are optional RmlUi targets; no RmlUi dependency flows into document editing, transport or core. The desktop executable retains screen composition. Shared screen flow, session actions, play presentation, library activation/browser and menu/brand rendering targets are defined in `cmake/application_targets.cmake`. A configure-time ownership check rejects duplicate first-party implementation source assignments and emits `locus-source-ownership.csv` for both desktop and portable builds.

The portable `LOCUS_BUILD_APP=OFF` configuration fetches only JSON and utf8proc, builds `locus_core` and optionally the same 12 portable tests/editor benchmark as the desktop build. `cmake/core_tests.cmake` is the single registration source for these tests. Linux CI exercises this configuration; it is not a Linux desktop port.

The Windows CI and headless domain tests are valuable, but cannot establish
actual motion readability, acoustic sync or screen-tearing behavior.
The optional headless `locus_route_preview_bench` executable profiles
1,024/60,000 event charts at 4/20 NPS with one/four simultaneous preview
panes and reports compile time, per-frame snapshot time, CPU ribbon p50/p95/p99, scratch capacity growth and a geometry checksum. Windows
CI runs all four cases and uploads the output. This gives repeatable CPU-side
numbers, **not GPU frame rate or input latency**. Verify the camera and glyph
legibility on a real Windows display before introducing visual crossings
masks or a supplementary action queue. Add regression
tests only for contracts or failures they can actually detect.

## Difficulty and remaining architecture work

V4 maintains independent histories of the four keys, short/slow decay states,
rhythm history and sorted section peaks. The editor caches ratings by route
revision; the catalog caches verified summaries. See [formulas](CHART_DIFFICULTY.md).
READ is purely diagnostic. The renderer reuses PathRibbonWorkspace across
PLAY and comparison panes; TempoProjection indexes musical-time conversions;
cached direction counts, SET views and virtualized lists avoid repeated
full-document work in stable frames.

Prioritize measured issues: audio cursor accuracy/PCM memory, filesystem
fault recovery, full-frame profiling, keyboard focus and real-window usability.
Keep changes feature-driven rather than refactoring by file length.
See [engineering acceptance](REFACTORING_PLAN.md).
