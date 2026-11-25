# Chart difficulty — v2

LOCUS estimates intrinsic chart difficulty on an **uncapped, nonnegative scale**.
There is no maximum score of 10 (or any other fixed ceiling), and none of the
six component ratings saturates at a fixed maximum. Ratings are derived on demand and are not stored in chart files.

This is still a heuristic, **not** a calibrated player skill prediction or a
guarantee that a pattern is sight-readable. The displayed **V2** label makes
the current formula explicit.

The calculator in `core/chart_difficulty.{h,cpp}` uses absolute note
timestamps, directions, and route geometry measured in time-based reference units.
It neither loads audio nor depends on BPM/tempo maps, editor grid positions,
the current skin, practice speed, key bindings or player performance. The editor recalculates after chart changes;
the song library computes ratings during a rescan.

## What contributes

1. **SPEED** — rolling 1.8-second note and physical-key rates, plus **active
   chart-wide physical-key density**. Long or consistently dense streams
   contribute to total difficulty. Diagonal notes count as one note but
   two key presses.
2. **RHYTHM** — irregular adjacent intervals, varying real-time spacing and very
   short gaps. Its density multiplier has no upper ceiling; a steady triplet
   pattern is not as irregular as unpredictable alternating intervals.
3. **KEYS** — simultaneous diagonal input, rapid changes in key masks,
   opposite-chord switches and repeated-note jacks. No hard rating cap.
4. **READ** — sharp turns, 180-degree reversals and nearby route segments.
   Visual ambiguity is approximated geometrically, not from actual rendered
   pixels. Dense turns intensify this contribution without a hard cap.
5. **STAMINA** — active duration and chart-wide physical key rate. Longer
   dense passages increase difficulty even if their fastest 1.8 seconds
   stay identical; long rests cannot inflate exertion by themselves.
6. **PEAK** — a secondary indication of especially fast sections, using
   the 99th-percentile and maximum short-window key rates.

For the chart-wide density and stamina measurements, `activeSeconds` sums
consecutive note gaps with each gap limited to 1.5 seconds. That discounts
long silent intervals but does not erase their effect on the average rate.
One isolated note contributes zero active duration. The calculation uses
integer-millisecond differences, preserving invariance under shifting all
note timestamps together with the song offset.

## Current formula

```text
activeKps = totalPhysicalKeyPresses / activeSeconds  (0 if no active duration)
q85Nps   = 85th percentile of rolling notes/second
q85Kps   = 85th percentile of rolling keys/second

SPEED = 1.35 × (0.55 × q85Nps + 0.20 × q85Kps + 0.25 × activeKps)

base = sqrt(
    0.52 × SPEED² +
    0.14 × RHYTHM² +
    0.14 × KEYS² +
    0.15 × READ² +
    0.05 × STAMINA²
)
total = (base + 0.08 × PEAK) × (0.70 + 0.30 × min(noteCount / 24, 1))
```

All displayed components are nonnegative and **not capped**. RHYTHM, KEYS,
READ, STAMINA and PEAK retain their own formulas in the calculator source.
The confidence factor reduces the influence of extremely short samples;
it does **not** impose an upper limit. Stamina's duration contribution grows
approximately with the square root of active seconds rather than approaching
an artificial asymptote.

The weights are hypotheses. They have not been calibrated using a dataset of
real players, and should be judged by comparing chart examples in-game.

## Guarantees and limitations

- Whole-chart rotation or mirroring leaves every component unchanged.
- Changing BPM, tempo map, or tick subdivision without changing absolute note
  times and directions leaves the entire rating unchanged.
- Shifting note times together with the offset leaves the rating unchanged.
- The calculation never changes chart timing, headings, scoring or JSON.
- Blank charts report zero; invalid chart parameters return no useful rating.
- Audio latency, actual rendered overlap, monitor size, chosen skin, practice
  speed, human memory and finger technique are not modelled.

Compare charts by relative rating and breakdown, not by interpreting a fixed
number as an objective skill level. A score of 12 or 30 is legitimate.
