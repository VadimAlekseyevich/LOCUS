# Chart difficulty — V4

The overall rating is a chart-only, uncapped estimate of physical execution
difficulty. A directional event is a simultaneous mask of one or two of the
four physical keys. The rating does not depend on route geometry, crossings,
turn angles, skin, BPM notation, snap, or keyboard layout. The base rating
uses 100% speed; HT/DT recompute the same model at the selected rate.
EZ/HR change judgement windows (150% / 70%), not physical press strain.
HD fades near the player and FL caps future visibility at 350 real milliseconds.
Neither visual nor timing-window modifiers change the physical strain rating.
It is calculated on demand, not stored in chart JSON.

## Design

The algorithm uses independent key histories and exponentially decaying
strain, inspired by [osu!mania's Strain skill](https://github.com/ppy/osu/blob/master/osu.Game.Rulesets.Mania/Difficulty/Skills/Strain.cs)
and [osu!'s section aggregation](https://github.com/ppy/osu/blob/master/osu.Game/Rulesets/Difficulty/Skills/StrainSkill.cs).
Separate jack/chord/technical and endurance skills in
[Etterna MinaCalc](https://github.com/etternagame/etterna/tree/master/src/Etterna/MinaCalc)
inform the decomposition. LOCUS uses its own formulas and constants, not
either game's rating scale. All parameters are provisional and uncalibrated.

## Input strain

All times are real elapsed seconds at the selected rate: original song
seconds divided by the clock rate, without rounding the event times. For every event let dt be the gap from
the preceding event, k the number of physical keys (1 or 2), and df the gap
from the previous press of finger f. Every state decays by exp(-dt/tau).

- SPEED: decay with tau=0.40; add k/0.40 on each event.
- FINGERS: decay each of four independent states with tau=0.48; for every
  pressed finger add (1 + 0.65*(0.125/(df+0.025))^1.2)/0.48, or 1/0.48
  on its first press. Combine as 0.75*max(F) + 0.25*sqrt(sum(F^2)).
  A jack, trill and roll have different retrigger costs at the same NPS.
- COORDINATION: decay with tau=0.42; add impulse/0.42. Impulse is 0.85 for
  a chord plus 0.08 per changed key bit from the previous event, plus
  (0.40+0.32*sharedKeys)*0.12/(dt+0.025) if either event is a chord and
  they share a key. This models chord synchronization and anchor reuse.
- RHYTHM: decay with tau=0.45; add (0.70*surprise*k + 0.30*fingerRhythm)/0.45.
  Surprise combines 20% adjacent gap log2-ratio distance and 80% minimum
  matching error against repeated periods of 2–4 gaps, with two repetitions
  required. Ratio distances are bounded at 3. FingerRhythm sums log2-ratio
  distances (bounded at 2) of successive intervals on each pressed finger,
  where both intervals are <=1s. Gaps >2s reset global rhythm history.
- STAMINA: decay a separate press-rate state with tau=6.0; add k/6.0 and
  report max(0,state-2). This models slow recovery independently of speed.

## Section aggregation and overall

Each occupied 400ms section, aligned to the first event, stores the maximum
of each skill at actual event times. Sort the peaks of a skill descending.
Empty sections do not add load; elapsed time still decays every state.

```text
skill = sqrt(0.10 * sum(0.90^j * peak[j]^2))
overall = sqrt(0.35*SPEED^2 + 0.30*FINGERS^2 + 0.20*COORDINATION^2
             + 0.10*RHYTHM^2 + 0.05*STAMINA^2)
```

The normalization is fixed (not divided by section count), so short sections
matter, sustained high strain contributes more, and easy padding cannot
dilute already-recorded peaks. There is no extra note-count confidence
penalty. The weights are coefficients of a quadratic norm, not literal
percentages of the final score.

READ remains a diagnostic from the previous angle/clutter model, displayed
as Read* in PLAY, with zero weight in overall. The 1.8s peak NPS/KPS and
PEAK density are telemetry, not independent rating bonuses.
activeSeconds still sums note gaps capped at 1.5s for telemetry only.

## Properties and limitations

- Blank charts return zero. Exact event timestamps dominate tick metadata.
- Shifting all times, rotating/mirroring headings or changing the tempo grid
  without changing input leaves the rating unchanged.
- One-to-one key permutations that preserve valid input masks leave physical
  rating unchanged even when the route geometry changes.
- No assumed mapping of keys to hands or player-specific dexterity.
- O(n + m log m) input analysis for n events and m occupied sections;
  bounded four-finger state and eight-gap history. Reading diagnostics use
  bounded local geometry comparisons and linear-time percentile selection.
- Parameters need real-player calibration using observed accuracy and
  matched-density pattern families; this is not a measured human skill model.

The editor uses a route-revision cache; the library stores derived metadata for each verified chart at native rate. Both use the same core calculator. Difficulty V4 is the estimator version, independent of judgement rules version 1. Ratings are not persisted in charts or Results.

For modded PLAY, a separate selection cache retains only the selected chart payload and its most recent modified rating. The browser tree keeps base ratings, so toggling a modifier never re-rates the whole collection. Unavailable/stale payloads show no modified rating until the library refreshes.

The editor's song overview shows a max-pooled 256-bin timeline from the same occupied 400 ms section peaks, using the physical weighted norm before whole-chart aggregation. Silence is empty; height is relative to the highest local strain of the focused chart, not a star rating. The cache is invalidated by route revisions (including duration changes), not cursor motion. The overview still seeks on click.
