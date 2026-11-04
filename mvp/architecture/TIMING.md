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

Rendering a Wrap step must use seam-aware exit/re-entry geometry, not interpolation through the grid interior. The proposed screen-space treatment and worked examples are specified in [WRAP.md](./WRAP.md) (A-04).

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

Key-down arbitration during play, pause, and editor seeking is specified in **Section 8.1 (A-03)**. The clock adapter's ability to keep actual audio and virtual song time aligned is a test requirement for **A-07/T-06–T-08**, not assumed accomplished by this document.


## 8.1 Input arbitration policy (A-03)

This section defines deterministic **scoring behavior**, not the numerical widths of the timing windows or a specific keyboard API. The authored route always executes independently, including on a Miss. This policy is applied to an active **Play/test-play session** only: editor timeline scrubbing is never a scoring operation.

### 8.1.1 Key-down definition and timestamp source

- A **candidate press** is a new physical **key-down edge** on a currently mapped Left or Right action, occurring when that key transitions from **released to down**. One held key generates **one** candidate press, not a continuous stream.
- OS-generated key-repeat events and repeated "down" notifications before a matching key-up are discarded. A later release and fresh down is a **new** candidate press.
- Non-turn keys and key-up events never award, reduce, or consume a judgement.
- Use the **capture timestamp of the edge** in the coherent monotonic clock domain, converted to **song time** against the same SongClock reference as the turn target. Do **not** substitute the time at which a render/update loop happens to process the event.
- If the chosen platform cannot provide reliable per-event timestamps, the input adapter must record monotonic time immediately when polling/capturing the edge; quantify that uncertainty in the audio/input spike. The remaining policy must not change with FPS.
- User input calibration, if eventually offered, adjusts **judgement-time alignment only**. It never changes chart timestamps, route geometry, or when the marker turns. Calibration direction and units will be specified when that setting is implemented.

### 8.1.2 Active window and grade determination

For a turn event `e` at chart beat `n`, define its **target song time** `T = O + n * 60 / B`. Let its configured nonnegative window thresholds, in seconds, satisfy:

```text
0 <= P <= G <= W
```

Here `P` is the Perfect half-width, `G` the Good half-width, and `W` the Bad (outermost accepted) half-width. Numerical values remain **Open** until T-11 and playtesting.

- The **eligible interval** is the **closed** interval `[T-W, T+W]`. A key-down at exactly either outer endpoint is eligible.
- The **first eligible candidate press** (or same-timestamp press group; see below) is the only input that can resolve `e`.
- If that press action is **wrong**, resolve `e` immediately as **Miss**, regardless of its timing; no subsequent correct press can repair it.
- If that press action is **correct**, with error `delta = press_song_time - T`, assign:
  - `abs(delta) <= P`: **Perfect**;
  - otherwise `abs(delta) <= G`: **Good**;
  - otherwise `abs(delta) <= W`: **Bad**.
- A press earlier than `T-W` is **ignored**, not buffered. Its button remaining held into the window does **not** count as a new press.
- A press later than `T+W` is **ignored** for this event. The event has already become **Miss** once the window expires without an eligible press.
- Each event resolves **once and irreversibly**. Further presses do not produce additional judgements for it.
- Keys pressed **outside every active window** are ignored and incur **no extra penalty**. This does not contradict "wrong input gives Miss": that rule applies to a wrong action **inside an active event window**.

**Boundary rule:** equality is intentional. `abs(delta) = P` is Perfect; `abs(delta) = G` is Good unless it also meets P; `abs(delta) = W` is Bad unless it meets a tighter threshold. Expire an untouched event only after logical song time becomes **strictly greater** than `T+W`, not at the exact endpoint.

### 8.1.3 Simultaneous Left and Right

Both turn actions may be pressed with the **same captured timestamp**. Treat all key-down edges at that **exact timestamp** as one input group before evaluating them; the platform's iteration order must not choose a winner.

- Both Left and Right present in the same eligible timestamp group → **Miss** for the active event, even if one direction was correct.
- Only the same action repeated within a timestamp group → treat as a **single** candidate action.
- Different timestamps are processed chronologically. The earlier eligible candidate wins, even if another action arrives extremely soon afterward. No speculative tolerance window is introduced for "almost simultaneous" input in the MVP.
- A simultaneous group outside all windows is ignored, without additional Misses.
- An input group cannot resolve two events. See the non-overlap rule below.

This choice favors deterministic outcomes, without relying on keyboard event ordering. True simultaneous presses are not silently treated as the correct key.

### 8.1.4 Event ownership and overlapping windows

- A single active turn event owns the input window surrounding its timestamp.
- For MVP, **scorable windows must not overlap or even share an endpoint**. With ordered turn targets `T[i]`, the chosen window configuration is admissible only if `T[i] + W < T[i+1] - W` for every adjacent event pair.
- Chart event uniqueness by beat is a separate chart validation rule. **Window separation depends on the selected judgement configuration**, so confirm it when opening a Play/test-play session (and warn in the editor). Do not permanently reject a structurally valid chart just because a later difficulty profile might use wider windows.
- If the selected timing profile causes overlap, display a clear validation/configuration error and **do not start scoring**; do not arbitrarily award the press to the earlier or nearer event.
- Consequently, an eligible input timestamp belongs to **at most one** turn event. A wrong key inside that event's window yields its Miss, and no other event is affected.

This restriction keeps the first implementation small; it may be reconsidered only after demonstrated need for denser charts and an approved deterministic multi-window policy.

### 8.1.5 Processing order, expiry, and song completion

An input adapter may deliver multiple captured events during one frame. For any processing batch:

1. Collect all available key-down edges with capture timestamps and discard key-repeat/held-key notifications.
2. Convert edges to song time, sort by ascending captured timestamp, and group **equal** timestamps. Within a group, action ordering is irrelevant.
3. For each timestamp group, evaluate eligibility against the configured event windows in **chronological** order; advance expiry for events whose `T+W` is strictly **before** that group's timestamp, then process the group.
4. After draining captured input through the current authoritative song time, expire any still-unresolved events whose window ended **strictly before** that time.
5. At authoritative audio completion `s=D`, process all captured eligible inputs **through D** first, then settle remaining already-scheduled unresolved events as Miss, exactly once, and close the session.

The system must not assign a Miss based on the **render frame's** arrival time before it has processed already-captured earlier key-downs. A keyboard input that was captured inside the window but processed after a dropped/stalled frame still receives the grade for its **capture timestamp**.

A record arriving *after* its event was irrevocably resolved/expired is not permitted to reopen that event; unexpected delayed delivery should be surfaced in diagnostics. An event whose window extends past audio end can only be hit **up to D**; after the result transition, the window no longer exists.

### 8.1.6 Transport states and focus changes

| State / action | Input arbitration outcome |
| --- | --- |
| Count-in / preroll | Candidate presses are scored **only if** their song-time timestamps enter a real turn's window (including a beat-0 event). Other presses are ignored. Count-in must begin early enough to make the first accepted window playable. |
| Normal play | Apply the rules above in timestamp order. Unresolved windows expire when due. |
| Pause requested | First drain captured play-time inputs through the pause boundary, then freeze song time and event expiry. Key-downs during pause are ignored. |
| Resume | Preserve prior resolved grades and remaining windows. **Require a release before a new down** for any turn key held through pause; do not score a synthetic resume key-down. |
| Keyboard focus lost | **Proposed safe MVP behavior:** automatically pause and ignore key edges while unfocused; restore focus before resuming, with the same release-before-new-down rule. |
| Editor preview seek/scrub | No judgement session is running. Show the route at the selected beat, with **zero** awarded grades/Misses. |
| Editor test-play | Start a **new** regular Play judgement session from a validated chart snapshot. It uses the same input arbitration as normal Play. |
| Retry/restart | Discard all old capture events and event-resolved flags; reset grade/combo state; start a new session from preroll. |
| Regular Play seek | Arbitrary seek within an active scoring run is **not supported in MVP**; it cannot silently preserve a partially judged run. |
| Results / audio ended | Ignore all presses for the finished session. No additional route or score changes. |

### 8.1.7 Deterministic arbitration examples

All examples below use a **Right** turn event with target `T`. Assume `0 <= P < G < W`; symbols are thresholds, **not numerical defaults**.

| Captured inputs / timeline | Result for the event | Why |
| --- | --- | --- |
| Right at `T` | Perfect | Correct action at zero error |
| Right at `T-P` | Perfect | Perfect endpoints inclusive |
| Right at `T+G` | Good | Good endpoint inclusive (outside P) |
| Right at `T-W` | Bad | Eligible outer endpoint |
| Right at `T+W` | Bad | Eligible outer endpoint; not expired at equality |
| No input through time `>T+W` | Miss | Window expired unresolved |
| Left at `T`, then Right at `T+P/2` | Miss | First eligible input was wrong; irreversible |
| Right at `T-W-ε`, then Right at `T` after release | Perfect | Early press ignored, new valid key-down scores |
| Right down before `T-W`, held past `T` | Miss, if no new press | No held-key scoring |
| Right at `T`, then Left at `T+G/2` | Perfect | First eligible input resolved event |
| Left and Right at identical `T` | Miss | Simultaneous conflicting actions |
| Left at `T`, Right at `T+ε` | Miss | Distinct timestamps: earliest eligible wins |
| Left or Right entirely outside all windows | No separate judgement | Stray press ignored |
| Right at `T` captured before a render stall, processed later | Perfect | Captured timestamp, not processing frame time |
| Pause while window open; Right key-down during pause | No new grade | Paused edges are discarded |
| Resume while Right is still held | No grade without release/new down | No synthetic key-down at resume |
| Editor seeks across `T` | No grade | Editor preview never scores |
| Audio ends before an untouched window would expire | Miss at end | Completion settles unresolved due events |

For `ε` above, choose any sufficiently small **positive** duration that preserves the stated ordering. Events with mutually overlapping windows are rejected at session setup, so these examples never require an ambiguous choice between two active events.

**Design consequences:** A mistimed or wrong turn **never** changes the correct route. The UI may show a judgement when resolved, but its animation timing is independent of the capture timestamp and cannot retroactively decide the grade.

## 9. Preview sampling boundaries

- The preview path is calculated using the **same** Chart and Route logic as gameplay.
- At chart beat `b`, a configured lead `L` shows the path from current progress through future progress `b+L`, clipped to the song end `b_end = (D-O)B/60` if audio duration is available.
- At `b < 0`, the marker remains on the start point, while preview can display the path that begins at beat zero; it must not invent movement before beat zero.
- A preview crossing an integer beat obeys the same turn-at-boundary order. Any Wrap in its horizon is represented as a seam, not a line connecting opposite grid points across the interior.
- The default and range for `L` remain open for playtesting and editor UX. They are not part of A-02.

## 10. Explicitly deferred decisions

| Topic | Next task |
| --- | --- |
| Handling two keys at once, repeat presses, and which event wins a press | Resolved by Section 8.1 (A-03) |
| How wide Perfect/Good/Bad/Miss windows are | T-11 and playtesting |
| Visual shape/duration of exit and re-entry at the seam | Documented in [WRAP.md](./WRAP.md) (A-04); prototype feel still awaits validation |
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
