# Chart difficulty — v1

LOCUS estimates the intrinsic difficulty of a chart on a **0–10** scale.
This is a heuristic for comparing charts inside LOCUS, **not** a calibrated
performance rating, a player skill estimate, an accuracy prediction, or a
guarantee that a first-time player can read the map. The UI explicitly labels
the number **V1**.

The calculator runs in `core/chart_difficulty.{h,cpp}`. It needs only the
absolute note directions, timestamps, song BPM and chart geometry; it does not
open music, allocate graphics, depend on the current skin, or touch save files.
Results are recomputed after each authored editor revision, and once per
difficulty on library rescan. The value is **derived**, never added to JSON.

## Inputs

- **Absolute note timestamps** (`timeMs`), rather than BPM alone or the
  number of timeline subdivisions. BPM/offset are used only to identify
  musical placement for a small rhythmic accent and for legacy-free computed
  timestamps in internal, unsaved charts.
- **Eight-way direction**, represented internally by a compass heading and
  its physical 1–2 directional key presses. A diagonal is one note but two
  simultaneous keys.
- **Turn geometry**, simulated on the same normalized, infinite route as the
  game. Nearby older path segments are compared against new segments.
- **Timing gaps** between events, active duration, and local note/key rates.

## Six displayed measurements

All are individually scaled to **0–10** with saturating curves (high values
become progressively harder to earn):

1. **SPEED** — 1.8-second rolling note/key rates. The 85th and 95th
   percentiles carry more weight than the single fastest burst; diagonal
   notes consume two keys.
2. **RHYTHM** — surprises in consecutive intervals, measured by the
   logarithm of their duration ratio. Small additional weight for placements
   off the ordinary quarter-beat grid and extremely short intervals. A steady
   triplet stream does not receive the same penalty as irregular timing.
3. **KEYS** — diagonal two-key chords, rapid changes in held key masks,
   full opposite-chord switches and repeated-note jacks. The physical
   bindings are configurable, so this is **binding-neutral**: it does not
   assume a particular finger layout or player handedness.
4. **READ** — relative angle between consecutive *absolute* headings,
   time pressure on sharp bends, direct 180° reversals and local geometric
   ambiguity. Nearby crossing or overlapping line segments within the
   previous 4 seconds contribute a bounded clutter penalty.
   This approximates visual reading; it does **not** run the actual renderer,
   recognize images or model a particular player's vision.
5. **STAMINA** — active note duration multiplied by sustained local key
   rate. Empty gaps longer than 1.5 seconds contribute at most 1.5 seconds
   each; a long song with only a few notes cannot inflate this rating.
6. **PEAK** — an additional indication of very dense sections using the
   99th-percentile and maximum short-window key rate. It contributes a
   *small* amount to the final rating, without dominating it.

### Total

The primary total is a weighted root-mean-square of five dimensions:

```text
base = sqrt(
    0.29 * SPEED^2 +
    0.19 * RHYTHM^2 +
    0.22 * KEYS^2 +
    0.23 * READ^2 +
    0.07 * STAMINA^2
)
total = clamp(0, 10,
    (0.96 * base + 0.08 * PEAK)
    * (0.70 + 0.30 * min(noteCount / 24, 1))
)
```

The final factor downweights extremely short excerpts whose isolated bursts
are unreliable representatives of a full chart. Ratings are intentionally
subjective: coefficients are **starting hypotheses**, not fitted to a
dataset of successful player clears.

## Important guarantees

- Rotating or horizontally/vertically reflecting an *entire* chart should
  not change its rating: the geometrical challenges are the same.
- Subdivision/tick resolution changes without changing the actual music
  timestamps or trajectory do not change it.
- Shifting the whole timeline and song offset together does not change it.
- No calculation changes a note's direction, time, score, or difficulty JSON.
- Blank charts report zero; invalid chart parameters report no rating.
- The rating does not depend on currently selected note, scroll position,
  practice speed, chosen skin or latest player performance.

## What v1 cannot know

Actual difficulty depends on display size, visual effects/skin, monitor
latency, audio, player experience, background art, practice history and
readability of lines *as rendered*. The first version cannot reliably
predict memorization, individual finger techniques, or player-specific
challenges. A route's apparent overlap is estimated geometrically, not
from pixels. A real calibrated ranking will require a collection of
charts and player judgements/clear data, plus side-by-side comparisons.

Compare **relative ratings and component breakdowns** rather than treating
4.8 versus 5.0 as a definitive statement of skill.
