# LOCUS — Musical Time and Turn Semantics

**Task:** A-02 — Freeze beat/event semantics  
**Status:** Architecture contract for MVP v0.1; subject to Stage 1 owner review  
**Scope:** Beat zero, signed chart offset, quarter-note steps, turn/Wrap ordering, song boundaries  
**Related documents:** [GDD](../GDD.md), [Architectural Boundaries](./BOUNDARIES.md), [Roadmap](../ROADMAP.md)

> This document defines **behavior**, not the eventual C++ interface or serialized chart-file schema. The next task (A-03) will specify how competing key-downs are arbitrated; later timing tasks will measure audio clocks and select numeric judgement windows.

## 1. Invariants

- One chart beat means **one quarter note**; movement advances by one grid edge per beat. Grid cells are not beats.
- The song's musical position, not frame count or accumulated per-frame delta, determines the marker's location.
- A charted turn is an instantaneous **relative** change in heading at a beat boundary, applied regardless of the player's score.
- Every movement step lasts one complete quarter-note beat, including a Wrap step.
- The initial grid size is **4×4**. Wrap is the sole boundary rule and preserves heading.
- Only Left and Right chart actions are permitted in the MVP. A straight step is implicit when no event exists.
- The renderer may use continuous interpolation; route positions and turn directions remain deterministic and independent of player inputs.

## 2. Time domains and notation

| Symbol | Definition | Unit |
| --- | --- | --- |
| `s` | Song time, with `s = 0` at the **beginning of the audio file** | Seconds |
| `b` | Chart position measured in quarter-note beats, beginning at beat zero | Beats (real number) |
| `n` | Nonnegative, integer beat boundary | Beats (integer) |
| `B` | Positive, constant BPM for MVP | Quarter notes per minute |
| `O` | Signed chart audio offset: song time when beat zero occurs | Seconds |
| `D` | Audio file duration measured from `s = 0` | Seconds |
| `P[n]` | Logical grid cell at the start of beat `n` | 4×4 coordinates |
| `H[n]` | Heading for the outgoing step from `P[n]`, **after** any turn on beat `n` | Up/Right/Down/Left |

Time conversion in the MVP is:

```text
seconds_per_beat = 60.0 / B
song_time_at_beat(n) = O + n * seconds_per_beat
beat_at_song_time(s) = (s - O) / seconds_per_beat
```

Use sufficient precision (prefer double precision for domain time). Do **not** round `b` before route interpolation or timing judgement. An exact event's beat `n` maps to the single target song time `O + n * 60/B`.

**Offset sign, unambiguously:**

- **Positive `O`** means chart beat zero is **after** audio file start (audio has an intro before beat zero).
- **Zero `O`** means chart beat zero coincides with the beginning of the audio file.
- **Negative `O`** means chart beat zero is **before** the first audio sample; a playable negative-time lead-in is necessary for any early actions. Negative offset does not reverse the meaning of time.

Chart offset `O` is part of the authored chart and does not equal a player's separate hardware/input calibration offset. Calibration may shift **judgement alignment** when added later, but it must not rewrite chart beats or route positions.

## 3. Beat zero and initial state

- The start cell `P[0]` and initial heading are explicitly authored.
- **Beat zero (`b=0`) is the instant the first movement step begins.** It is not an extra stationary beat, and the first point-to-point step occupies `0 <= b < 1`.
- A turn event **at beat zero is valid**. It rotates the initial heading **before the first step**. An empty chart starts straight from the authored heading.
- For `b < 0`, show the marker at `P[0]` without advancing route steps. Events are not scheduled at negative beat indices.
- Count-in/preroll is a **presentation and transport preparation phase**, not additional chart beats. It must provide enough time before the earliest scorable event for the eventual early timing window; it never shifts beat-zero timestamps.
- The clock/transport must be capable of representing **negative virtual song time** before the first audio sample when a count-in or negative offset requires it. The audio file begins playback when virtual song time reaches `s=0`. How this is achieved in the chosen backend is determined during the timing spike and implementation stages, not by an extra rule in the renderer.
- At beat zero, the marker occupies the starting point exactly. It advances *out of* that point as fractional beat progress increases.

**MVP restriction:** Turn events are placed only on nonnegative **integer quarter-note beat boundaries**. No simultaneous/multiple events on the same beat. Subdivisions, tempo changes, and fractional event beats are future extensions.

## 4. Exact turn → step → Wrap order

At every integer beat boundary `n >= 0`:

1. The route arrives at cell `P[n]`, with its incoming heading (or the authored initial heading for `n=0`).
2. If the chart has a Left/Right event on beat `n`, rotate that heading by exactly 90° **relative to the incoming direction**. Otherwise, keep it.
3. The resulting direction is the outgoing heading `H[n]`.
4. Start movement **from `P[n]` along `H[n]`** over the interval `[n, n+1]`.
5. The destination `P[n+1]` is the neighboring grid cell, using modulo-4 Wrap if the step exits any edge.
6. A Wrap never adds a turn or a scoring event. Its duration is exactly one beat and its direction is unchanged.

Coordinates use `x=0..3` left→right and `y=0..3` top→bottom. Heading vectors are:

| Heading | dx | dy |
| --- | ---: | ---: |
| Up | 0 | -1 |
| Right | +1 | 0 |
| Down | 0 | +1 |
| Left | -1 | 0 |

For any step:

```text
P[n+1].x = (P[n].x + dx(H[n]) + 4) % 4
P[n+1].y = (P[n].y + dy(H[n]) + 4) % 4
```

On the interval `n < b < n+1`, the logical step is `P[n] -> P[n+1]`, with fractional progress `alpha = b-n` in `(0,1)`. At an **exact** integer beat, report the cell `P[n]`, not a rounded interpolation from the previous segment, and use the outgoing heading `H[n]`.

**Crucial:** A turn at beat `n` changes the **outgoing step** `P[n] -> P[n+1]`; it does **not** retroactively curve the incoming movement `P[n-1] -> P[n]`.

Rendering a Wrap step must use seam-aware exit/re-entry geometry, not interpolation through the grid interior. The exact visual treatment is deferred to A-04.

## 5. Worked example: BPM 120, positive offset

For `B=120` and `O=+0.250 s`:

```text
seconds_per_beat = 0.500 s
beat 0 -> song time 0.250 s
beat 1 -> song time 0.750 s
beat 4 -> song time 2.250 s
```

Start: `P[0]=(1,3)`, initial heading **Up**. The chart contains `beat 1: Right` and `beat 4: Left` with no other events.

| Beat boundary | Song time | Arrival point `P[n]` | Action at boundary | Outgoing heading | Next point `P[n+1]` |
| --- | ---: | --- | --- | --- | --- |
| 0 | 0.250 s | (1,3) | None | Up | (1,2) |
| 1 | 0.750 s | (1,2) | Right | Right | (2,2) |
| 2 | 1.250 s | (2,2) | None | Right | (3,2) |
| 3 | 1.750 s | (3,2) | None | Right | (0,2) — **Wrap** |
| 4 | 2.250 s | (0,2) | Left | Up | (0,1) |

Interpretation:

- At **beat 0**, the marker starts on `(1,3)` and travels upward; it does not arrive at `(1,2)` until beat 1.
- At **beat 1**, the Right event means rotate from Up to Right *at* `(1,2)`; the marker reaches `(2,2)` at beat 2. The input grade is evaluated around song time `0.750 s`, independently of whether the rendered frame displays the corner.
- At **beat 4**, the marker has already wrapped during beat 3's step and is now at `(0,2)`. The Left event changes Right to Up, so it moves toward `(0,1)`.
- Missing either turn has **no effect** on any cells/headings listed here.

## 6. Worked example: a turn on the outermost grid point

At `B=120`, `O=0`, start cell `(3,0)`, initial heading **Right**, and a **Left event on beat 0**:

1. Beat 0 occurs at song time `0.000 s`, with the marker at `(3,0)`.
2. The Left event rotates **Right → Up** before the outgoing step.
3. Up from `(3,0)` leaves the top edge, so that step **Wraps to `(3,3)`**.
4. During the interval `0.000–0.500 s`, the movement is the seam-aware step `(3,0) -> (3,3)`.
5. At beat 1 / `0.500 s`, the marker is at `(3,3)`. No event means it continues Up, reaching `(3,2)` at beat 2 / `1.000 s`.

This example fixes a possible ambiguity: movement must not first wrap rightward to `(0,0)` and then turn left. The turn precedes the step and its boundary test.

**Opposite case for regression:** `P[0]=(3,1)`, initial Right, and **no event at beat 0** gives `P[1]=(0,1)` heading Right. Adding a Left turn on beat 0 changes the outgoing step to Up `P[1]=(3,0)`, so that step does **not** wrap.

## 7. Negative offset example and preroll

For `B=120`, `O=-0.250 s`:

```text
beat 0 -> virtual song time -0.250 s
beat 1 -> virtual song time  0.250 s
beat 4 -> virtual song time  1.750 s
audio first sample -> song time 0.000 s, half a beat after beat zero
```

Before audio starts, the virtual SongClock can already count through a preroll and the route may already be advancing. A beat-0 turn is playable only if the count-in provides a sufficient early input window. Do not silently clamp negative offset to zero or retime the chart events.

**Product consideration, not a v0.1 scope increase:** negative offsets may feel unintuitive to authors; the editor should show a clear sign convention. It is valid for the file timing domain even though it can place gameplay in silence before the audio starts.

## 8. Song duration, ending, and stopping

Assume the chosen backend reports a trustworthy audio duration `D > 0` seconds.

- The intended playable song domain is virtual song time from preroll through `0 <= s < D`; the negative-time preroll is silent where audio samples do not yet exist.
- A turn event at beat `n` targets `s(n) = O + n*60/B`. **No chart turn may target `s >= D`** (the end of the audio), and events at negative song times are allowed only under the explicit negative-offset/preroll policy above.
- Song completion occurs when the audio reaches **`s=D`** (or the backend signals authoritative end-of-track). The marker stops at the route sample corresponding to that exact song time; a partially completed last step is not forced to its next cell.
- On completion, settle every unresolved *already scheduled* turn as **Miss exactly once**, even if its later input window would have extended beyond `D`. Do not wait for playback of nonexistent audio or invent later movement/turns.
- If an event's full Bad window would extend beyond the audio end, warn during validation/editor authoring when the final window sizes and duration are known. The event must not become a late hit after results have started.
- A chart with no turns is valid and ends naturally with the audio; it has zero turn judgements.
- A too-short or missing audio file is an actionable load/validation issue; do not synthesize an arbitrary song end from the last turn or preview length.
- After results transition, no additional movement or judgement occurs until the session is restarted.

**Pause:** The clock's song position is frozen and no expiry, movement, or scoring progression occurs. **Resume:** Continue from the same song position, maintaining audio alignment. **Retry:** Reinitialize at the preroll/start and reset all grades. **Editor seek:** Jump to any allowed song position and recompute the preview from chart data without awarding/expiring judgements.

The exact arbitration of key presses during pause, seek and event windows is deferred to **A-03**. The clock adapter's ability to keep actual audio and virtual song time aligned is a test requirement for **A-07/T-06–T-08**, not assumed accomplished by this document.

## 9. Preview sampling boundaries

- The preview path is calculated using the **same** Chart and Route logic as gameplay.
- At chart beat `b`, a configured lead `L` shows the path from current progress through future progress `b+L`, clipped to the song end `b_end = (D-O)B/60` if audio duration is available.
- At `b < 0`, the marker remains on the start point, while preview can display the path that begins at beat zero; it must not invent movement before beat zero.
- A preview crossing an integer beat obeys the same turn-at-boundary order. Any Wrap in its horizon is represented as a seam, not a line connecting opposite grid points across the interior.
- The default and range for `L` remain open for playtesting and editor UX. They are not part of A-02.

## 10. Explicitly deferred decisions

| Topic | Next task |
| --- | --- |
| Handling two keys at once, repeat presses, and which event wins a press | A-03 |
| How wide Perfect/Good/Bad/Miss windows are | T-11 and playtesting |
| Visual shape/duration of exit and re-entry at the seam | A-04 |
| JSON serialization, names of timing fields, chart audio-reference encoding | A-05 |
| Real backend transport, pause/seek, input clock correlation and timing drift | A-07, T-06–T-09 |
| Editor snapping UI and count-in visuals | E-10, E-13, P-10 |

This contract deliberately introduces **no** new event types, tempo changes, alternate boundary rules, or variable grid sizes.

## 11. Acceptance examples for A-02

- [x] Beat zero equals the start of step 0 and permits a charted turn.
- [x] Positive and negative audio-offset signs are defined without ambiguity.
- [x] Quarter-note motion is tied to BPM, not frame rate.
- [x] Turn then step then Wrap is explicit, including on an outermost grid point.
- [x] At BPM 120, worked examples cover beat 0, beat 1, and beat 4.
- [x] Audio duration and end-of-song outcomes are specified.
- [x] Unresolved input arbitration and backend-dependent timing remain attributed to later tasks.

**A-02 output:** A behavioral contract for implementation and unit-test fixtures; not a claim that audio, movement, or scoring code already exists.
