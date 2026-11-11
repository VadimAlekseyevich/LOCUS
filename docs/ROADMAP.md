# LOCUS — Foundation roadmap

Prerequisite work before adding larger mechanics or expanding the chart editor.
See [GDD](GDD.md), [architecture audit](ARCHITECTURE.md) and
[chart format](CHART_FORMAT.md).

## Stabilization stages

- [x] **1. Architecture boundaries (2026-11-08).** EditorSession,
  EditorFileActions, ListenSession, SkinProfileActions, song library and
  session/navigation lifetime are separated. Headless tests cover failure
  paths and native-dialog cancellation through injected pickers. The entry
  point now owns application composition and cross-screen frame ordering;
  dedicated render, editor, Play, library and input modules own the respective
  screen behaviors. Further refactors should be feature-driven, not cosmetic.
- [x] **2a. Managed Song Set storage.** New imports have one shared audio,
  a manifest, independently saved difficulty charts and library-wide writer
  locks. Save no longer creates redundant external authoring copies.
- [x] **2b. Recoverable local deletion.** A confirmed delete moves a difficulty
  or Song Set to LOCUS Trash; U can restore it, including after restart.
  Chart overwrites keep backups in `.locus-history/`.
- [x] **2b. Portable Song Set packages.** Export and import a complete
  managed Song Set as .locuspack (ZIP store-only), with CRC, size bounds and
  path traversal checks.
- [x] **2b. Song Set artwork.** Optional shared PNG/JPEG backgrounds per
  managed song, aspect-preserving cover crop, safe ZIP transport and a reusable
  image-resource layer for later skins and UI sprites.
- [x] **2c. Shared song tempo maps.** Chart v4 and Song Set manifest v2
  store absolute-millisecond BPM markers. The editor has a song-wide table
  and preserves note timestamps when the beat ruler is reprojected.
- [ ] **2d. Power-loss and filesystem validation.** Test denied writes, full
  disks, interrupted imports and recovery on physical Windows; add conservative
  cleanup only when safe.
- [ ] **3. Full scenario regression tests.** Chart create/edit/undo/save/
  reopen/TEST/PLAY/Results, BPM/snap timing invariance, repeated navigation,
  canceled dialogs, and safe shutdown.
- [ ] **4. Real-device audio, input and visual QA.** Measure synchronization,
  end-of-song late-hit windows, pause/resume/seek, practice rates and
  initialization/device loss on actual Windows hardware.
- [ ] **5. Profiling.** Benchmark dense 65k-turn charts, editor timeline
  operations and render cadence. Add caches only if measurements justify them.
- [ ] **6. GUI and usability verification.** Windows desktop tests for
  wide/narrow/portrait aspect ratios, clipping, font fallback, Unicode paths,
  remappable shortcut conflicts and editor workflows.
- [ ] **7. Reproducible release baseline.** Portable package or installer
  that runs without Visual Studio/CMake, clean-machine validation, version,
  license, documentation and tagged stable build.

There is **no pre-release compatibility guarantee** for chart versions or library layout. Changes affecting older files must be communicated to the developer.

Stages 1–6 are the engineering gate for adding major new mechanics. Stage 7
is required before public distribution, but need not block internal prototyping.
Every code milestone must pass Windows Release, CTest, production diagnostics,
clean-worktree checks and the actual BAT launcher.

## Possible later features

Hold notes, waveform editing, automatic BPM detection,
pitch-preserving practice speed and online services are not part of
foundation stabilization. The shared song tempo map is already implemented. Scope each separately after the stable baseline.
