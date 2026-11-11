# LOCUS — Current architecture

This is a map of the *current* code and its remaining boundaries, not a
chronicle of earlier refactors. The chart, song-set and skin formats are
pre-release; incompatible changes are allowed but must be called out.

## Owners and data flow

| Owner | Responsible for | Must not own |
| --- | --- | --- |
| `src/core` | Chart timestamps, musical-grid editing, exact-time `TimedRoute` index, eight-way headings, judgement, difficulty and chart JSON | GPU, native dialogs, filesystem library policies |
| `ChartEditor` | Mutable authoring document, selection, revisioned undo/redo | Saved-file selection, playback device, render UI |
| `EditorComparisonCache` | One revision-cached editable `TimedRoute`, up to three file-identity-cached reference routes; read-only render frame | Audio transport, modifying other charts, Song Set ownership |
| `SongLibrary` / `song_set_storage` | Installed Song Sets, authoritative shared tempo map, file transactions, recovery, cache/index | Gameplay scoring or editor keyboard state |
| `ChartPlayback` / `GameSession` | Audio-time transport and per-arrow judgement | Screen rendering, camera, skin settings |
| `PlayPresentation` | Presentation-only camera lag, outro and adjustment notice | Authoritative song time or note results |
| `path_mesh` / `route_renderer` / `gameplay_ui` | Project sampled route into a clipped square, draw ribbons, turn targets and marker | Loading files or changing gameplay state |
| Screen controllers | Play, editor, library, skin and modal commands | Other screens' storage or render internals |
| `main.cpp` | Stable resource lifetime and cross-screen composition | Another all-purpose service layer |

Play: physical input → `GameSession`/`TurnJudge` → read-only
`GameplayView` prepared by `PlayPresentation` → route/marker rendering.
Editor: authoring input → `ChartEditor` revision → one compiled editable route;
optional Song Set references are compiled read-only by `EditorComparisonCache`.
All one-to-four panes consume the editor's single audio-time cursor, SKIN
base speed and PREVIEW TIME (MS); neither opening a reference nor navigating
its picker edits notes. The upper editor note ruler uses BPM/snap ticks and
its own zoom. The bottom whole-song overview is an independent audio-seconds
projection over `ChartEditor::SongDurationSeconds()`, supporting fast
seek without note mutation. All BPM markers read the canonical Song Set
tempo map (fallback: the current draft), rather than individual chart notes.
The tempo map is integrated into a continuous beat phase in
`core/tempo_map.h`; notes keep their exact song `timeMs` when a segment changes. SET opens a difficulty on a single click, retaining
the unsaved-change confirmation when the active draft would be discarded.
TEST borrows a prepared draft without writing it. Library: disk → validated
Song Set index → explicit chart/audio activation. Audio streams are unloaded
before the shared audio device; GPU textures before the window.

## Invariants worth keeping

- Scoring reads the consumed-song/audio clock, never rendered frame count.
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
The chart difficulty READ geometry also uses these exact positions.

The world-space scale still uses ticks-per-beat as a *unit conversion*:
the camera applies its inverse, so screen motion is based on real elapsed
song seconds and is independent of snap resolution or BPM. Authoring
`core::RoutePositionAt` and editor grid navigation intentionally remain
tick-based; they are not a second scoring clock. Saved chart version 4 stores exact timestamps plus an explicit tempo map;
Song Set manifest v2 is authoritative for every difficulty.

Known limitation: the editor currently rejects two distinct timestamps
whose snap-grid projections collide. Relaxing that requires a separate
authoring model migration; it must not regress fast keyboard/snap placement.

### 3. One read-only route snapshot per frame (implemented)

`RoutePreviewFrame` prepares the marker, exact-time spans and the upcoming
turn window once per PLAY frame and once per comparison pane. The route
ribbon, junctions and camera consume the same source geometry. Marker order
is far-to-near, even when two authored notes occupy the same coordinate.
`TimedRoute::Window` supports caller-owned reusable buffers to avoid repeated
per-frame allocations in gameplay. Standalone/SKIN rendering retains its
convenience overloads.

Future work: on top of this frame implement density-aware marker sizing,
crossing visibility, compact reverse symbols and a next-action view. The
snapshot must not change scoring or silently drop dense events.

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
Reading chart bytes at refresh has an I/O cost; the JSON parse and
difficulty calculation still remain cached. Verify refresh performance
on large libraries rather than assuming this optimization is free.

Continue physical Windows fault testing for permissions, interrupted
replacement and disk-full recovery. A file fingerprint is not a substitute
for a transactional filesystem or a cryptographic authenticity check.

### 7. Keep build/QA proportional (medium)

The Windows CI and headless domain tests are valuable, but cannot establish
actual motion readability, acoustic sync or screen-tearing behavior.
The optional headless `locus_route_preview_bench` executable profiles
1,024/60,000 event charts at 4/20 NPS with one/four simultaneous preview
panes and reports wall-clock compile time plus per-frame snapshot time. Windows
CI runs all four cases and uploads the output. This gives repeatable CPU-side
numbers, **not GPU frame rate or input latency**. Verify the camera and glyph
legibility on a real Windows display before introducing visual crossings
masks or a supplementary action queue. Add regression
tests only for contracts or failures they can actually detect.

## Implementation order

1. **Done:** compile exact-time gameplay geometry and indexed runs.
2. **Done:** consolidate per-frame route and transport snapshots.
3. Add visibility policy and density-aware glyphs on top of the snapshot.
4. Consider editor time-first authoring while preserving BPM/Snap ergonomics.
5. Profile dense cases and verify real-device input, audio and clipping.

The exact-time route is now the structural foundation for a trustworthy
high-difficulty readability system. Do not introduce a new plugin framework,
screen hierarchy or premature general-purpose rendering engine.
