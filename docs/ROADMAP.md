# LOCUS — Foundation roadmap

Prerequisite work before adding larger mechanics or expanding the chart editor.
See [GDD](GDD.md), [architecture audit](ARCHITECTURE.md) and
[chart format](CHART_FORMAT.md).

## Stabilization stages

- [x] **1. Architecture boundaries (2026-11-08).** EditorSession,
  EditorFileActions, ListenSession, SkinProfileActions, song library and
  session/navigation lifetime are separated. Headless tests cover failure
  paths and native-dialog cancellation through injected pickers. The entry
  point owns application composition, global input, rendering and PLAY glue.
- [ ] **2. User-data durability.** Guard concurrent imports; define safe
  cleanup of unused immutable audio revisions; test denied writes, interrupted
  saves, missing files and recovery. Never delete an audio revision referenced
  by any difficulty. Preserve v1/v2 chart and existing skin/settings files.
- [ ] **3. Full scenario regression tests.** Chart create/edit/undo/save/
  reopen/TEST/PLAY/Results, BPM/snap timing invariance, repeated navigation,
  canceled dialogs, and safe shutdown.
- [ ] **4. Real-device audio and input QA.** Measure synchronization,
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

Stages 1–6 are the engineering gate for adding major new mechanics. Stage 7
is required before public distribution, but need not block internal prototyping.
Every code milestone must pass Windows Release, CTest, production diagnostics,
clean-worktree checks and the actual BAT launcher.

## Possible later features

Tempo maps, hold notes, waveform editing, automatic BPM detection,
pitch-preserving practice speed and online services are not part of
foundation stabilization. Scope each separately after the stable baseline.
