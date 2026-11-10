# LOCUS — Current architecture

This is a map of the *current* code and its remaining boundaries, not a
chronicle of earlier refactors. The chart, song-set and skin formats are
pre-release; incompatible changes are allowed but must be called out.

## Owners and data flow

| Owner | Responsible for | Must not own |
| --- | --- | --- |
| `src/core` | Chart timestamps, musical-grid editing, eight-way headings, route mathematics, judgement, difficulty and chart JSON | GPU, native dialogs, filesystem library policies |
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

### 1. Resolve exact timestamps versus snapped route geometry (critical)

`TurnEvent.timeMs` is authoritative for scoring
(`core/judgement.cpp`), but `TickAtTimeMs` maps it to an integer tick
(`core/chart_json.cpp` and `app/song_timing.h`). All route positions and
turn glyphs then use that *rounded tick* (`core/route.cpp` and renderers).
For a 120 BPM chart with one tick per beat, the discrepancy can approach
250 ms. A frame-perfect judgement can therefore disagree with a visually
displayed turn. More particles, colors or camera movement cannot fix this.

Design one immutable **timed route** in `core`:
- canonical note boundaries use exact song timestamps;
- each route run has a start/end timestamp, absolute heading and cumulative
  position, without rounding into authoring ticks;
- editor snapping is a *placement tool*, not the source of playback geometry;
- camera, ribbon, turn markers and results all query the same timed route;
- the core time representation is independent of raylib and practice rate.

Decide whether the new time-to-distance mapping changes route geometry for
off-grid notes; test equal-spaced patterns and BPM/offset edits before
replacing the old sampler. Keep judgement windows unchanged. This may
intentionally change chart appearance; no silent format migration.

### 2. Compile a route index for long charts (high)

`SampleRouteStep` walks previous turns from the beginning. The gameplay
frame currently requests route positions separately for presentation,
camera/marker and the path; `BuildScoredPathRibbon` and
`DrawRouteJunctions` each initialize their own route window from history.
Window sampling reduced the cost per segment, but it is **not** O(visible
events) per frame. Measure worst-case 65k-note play and editor scrubbing.

Use a **revision-owned index** of event times, cumulative positions and
headings. Resolve current playhead by binary search; iterate only events
inside the visible time range. Gameplay owns an immutable compiled index;
editor invalidates/rebuilds it on its existing document revision, not by
`Chart*` address. Keep a direct slow sampler for correctness comparisons.

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

1. Specify and validate exact-time route geometry against current charts.
2. Build the immutable timed route/index with editor revision invalidation.
3. Build one per-frame preview/clock snapshot and migrate render consumers.
4. Add visibility policy and density-aware glyphs on top of that snapshot.
5. Profile dense cases and verify real-device input, audio and clipping.

The first three items are structural prerequisites for a trustworthy
high-difficulty readability system. Do not introduce a new plugin framework,
screen hierarchy or premature general-purpose rendering engine.
