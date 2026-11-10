# LOCUS — Current architecture

This is a map of the *current* code and its remaining boundaries, not a
chronicle of earlier refactors. The chart, song-set and skin formats are
pre-release; incompatible changes are allowed but must be called out.

## Owners and data flow

| Owner | Responsible for | Must not own |
| --- | --- | --- |
| `src/core` | Chart timestamps, musical-grid editing, exact-time `TimedRoute` index, eight-way headings, judgement, difficulty and chart JSON | GPU, native dialogs, filesystem library policies |
| `ChartEditor` | Mutable authoring document, selection, revisioned undo/redo | Saved-file selection, playback device, render UI |
| `SongLibrary` / `song_set_storage` | Installed Song Sets, authoritative shared BPM/offset, file transactions, recovery, cache/index | Gameplay scoring or editor keyboard state |
| `ChartPlayback` / `GameSession` | Audio-time transport and per-arrow judgement | Screen rendering, camera, skin settings |
| `PlayPresentation` | Presentation-only camera lag, outro and adjustment notice | Authoritative song time or note results |
| `path_mesh` / `route_renderer` / `gameplay_ui` | Project sampled route into a clipped square, draw ribbons, turn targets and marker | Loading files or changing gameplay state |
| Screen controllers | Play, editor, library, skin and modal commands | Other screens' storage or render internals |
| `main.cpp` | Stable resource lifetime and cross-screen composition | Another all-purpose service layer |

Play: physical input → `GameSession`/`TurnJudge` → read-only
`GameplayView` prepared by `PlayPresentation` → route/marker rendering.
Editor: authoring input → `ChartEditor` revision → editor preview; TEST
borrows a prepared draft without writing it. Library: disk → validated
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
  lifetimes. Library refresh may invalidate indexed song/chart pointers.
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
tick-based; they are not a second scoring clock. Saved chart version 3
already stores exact timestamps, so this refactor does not change files.

Known limitation: the editor currently rejects two distinct timestamps
whose snap-grid projections collide. Relaxing that requires a separate
authoring model migration; it must not regress fast keyboard/snap placement.

### 3. Build one read-only preview snapshot per frame (high)

At present the ribbon, turn targets, player marker and follower each do
part of the same sampling work independently. Introduce a pure
`RoutePreviewFrame` assembled from the timed route and one clock snapshot.
It contains the visible segments, ordered authored events, nearest-event
priorities, marker position and clipping/proximity data. `path_mesh` and
`route_renderer` consume that frame without querying chart history or
reimplementing event windows.

That becomes the correct place for density-aware marker sizing, crossings,
reversal symbols and a compact next-action view. It must not change scoring,
authoring data or silently drop dense events.

### 4. Make the clock split explicit (medium-high)

`ChartPlayback::AudioSeconds` follows the actual audio cursor for slow
practice; `VisualAudioSeconds` smooths it. This is intentional, but both
are queried at multiple points during frame update/render. Produce one
`PlayClockFrame` containing **judge time**, **visual time**, practice rate
and pause/seek generation. Define a bounded presentation/judgement drift
contract and reset smoothing on discontinuities. The same visual timestamp
must feed camera, ribbon, turn markers and outgoing outro.

### 5. Consolidate state by lifecycle, not file length (medium)

`main.cpp` still coordinates screen transitions, volume fades, backgrounds,
active vs draft skin, audio resources and file paths. Most per-screen actions
already have owners. Avoid replacing it with a huge mutable
`Application` object. Extract a coordinator only when a new feature needs
a narrower lifetime or makes two screen policies disagree.

The runtime camera has a skin-controlled lag but retains the hardwired
`kExperimentalAdaptiveCamera` flag. Remove the build-time flag after device
validation, leaving one user-visible runtime policy.

### 6. Persistence/cache correctness at the boundaries (medium)

`SongLibrary::Refresh` reuses chart parses by file size + write time.
This is a reasonable optimization, **not** content identity; externally
replaced same-size/same-time files can appear unchanged. Use a stronger
fingerprint or explicitly invalidate the cache on known writes; keep
rechecks under the Song Set write lock. Continue physical Windows fault
testing for permissions, interrupted replacement and disk-full recovery.

### 7. Keep build/QA proportional (medium)

The Windows CI and headless domain tests are valuable, but cannot establish
actual motion readability, acoustic sync or screen-tearing behavior.
Benchmark update/preview time and allocations on representative dense charts,
then verify the camera and visual glyphs on a real display. Add regression
tests only for contracts or failures they can actually detect.

## Implementation order

1. **Done:** compile exact-time gameplay geometry and indexed runs.
2. Consolidate one per-frame preview/clock snapshot around the timed index.
3. Add visibility policy and density-aware glyphs on top of that snapshot.
4. Consider editor time-first authoring while preserving BPM/Snap ergonomics.
5. Profile dense cases and verify real-device input, audio and clipping.

The exact-time route is now the structural foundation for a trustworthy
high-difficulty readability system. Do not introduce a new plugin framework,
screen hierarchy or premature general-purpose rendering engine.
