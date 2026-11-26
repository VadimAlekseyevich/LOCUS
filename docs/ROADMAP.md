# LOCUS — Roadmap

Current gameplay, formats and ownership are described in [GDD](GDD.md),
[architecture](ARCHITECTURE.md), [chart format](CHART_FORMAT.md) and
[Difficulty V4](CHART_DIFFICULTY.md). The [engineering acceptance checklist](REFACTORING_PLAN.md)
tracks implementation and remaining validation separately. The game is pre-release.

## 1. Stabilize the existing foundation

The local game, editor, native menus, compact Song Set catalog, recovery,
portable packages, exact-time route and per-key judgement are implemented.
Complete acceptance instead of starting another broad refactor:

- [ ] Real Windows input/audio timing at 100/50/125% PLAY and 10% editor preview, calibration, seek, pause,
  end-of-song and first slow-playback startup; see [timing contract](TIMING_CONTRACT.md).
- [ ] Disk-full, permissions, interrupted publication/import and recovery on Windows.
- [ ] Native screen usability, Unicode text, keyboard focus, resize/DPI and
  editor comparison on physical displays; verify legacy fallback parity.
- [ ] Profile full frame, dense editor operations, large catalog and PCM
  memory on the same hardware/build. Change only measured bottlenecks.
- [ ] Clean-machine portable ZIP acceptance without build tools, dependency
  notices, version and a reproducible tagged build.

## 2. Gameplay presentation

Keep exact input timestamps, authoritative route and grading independent of rendering.

- [ ] **G01. Route rendering:** shared antialiased tapered ribbon and outline
  are implemented. Validate physical GPU output, overlaps, reversals,
  repeated straight notes and outer joins at multiple scales.
- [ ] **G02. Player and junction materials:** tune readability and restrained
  feedback; use the same materials in gameplay and skin preview.
- [ ] **G03. Playfield and HUD:** current combo/accuracy and right-gutter
  grade counts are implemented. Validate narrow windows and contrast.
- [ ] **G04. Judgement feedback:** evaluate distinct effects, timing feedback,
  and optional hitsounds without changing judgement windows prematurely.
  The optional hit error bar, persistent timing histogram, global background dim and custom skin WAV/MP3/OGG hitsounds are implemented;
  validate their readability and timing with physical devices.
- [ ] **G05. Sight-reading:** test dense patterns with players. Route reading
  should communicate input, not dominate physical execution difficulty.

## 3. Mapping and difficulty

- [ ] Calibrate [Difficulty V4](CHART_DIFFICULTY.md) against matched-density
  streams, jacks, alternating keys, diagonal transitions and real attempts.
  Reading remains a separate diagnostic, not a ranking factor.
- [x] Display cached physical-strain timeline in the ROUTE chart editor overview only.
- [x] Toggle the cached waveform in the upper note ruler independently of difficulty.
- [ ] Validate the waveform and variable-tempo metronome on real recordings;
  improve authoring tools based on mapper feedback. Shared tempo maps, exact unsnapped
  note placement, pattern transforms and pitch-preserving practice already exist.
- [ ] Build a representative local chart collection and validate progression
  from approachable to demanding physical input patterns.

## 4. Release and community

- [ ] First public Windows release: complete the acceptance checklist,
  package and onboarding, known limitations and licensing notices.
- [x] Record deterministic scoring replays and provide local spectator playback
  for matching revisions in native Results.
- [x] Seekable replay and Autoplay playback with scoring reconstruction.
- [ ] Validate spectator playback on physical Windows devices, including seek while paused, rate mods and end-of-song.
- [ ] Establish shared portable score contracts, published chart identity and a versioned performance model before implementing profiles and leaderboard UI.
- [ ] Prepare the local/client-server boundaries now; build a private vertical slice for accounts, published maps, score submission and leaderboards before public hosting. See [online architecture](ONLINE_ARCHITECTURE.md). Before hosting,
  implement the actual rights/reporting workflow described in the [README](../README.md#community-chart-packs-with-music--future-hosting-policy).

No pre-release compatibility guarantee: report every incompatible format
change, update examples and the corresponding contract in the same commit.
Prioritize performance, scalable ownership and non-hacky solutions.
