# LOCUS — Active Music Time and Judgement Rules

**Status:** Current behavioral reference (2026-11-06). The original beat-indexed, Wrap-only A-02 specification is historical and available in Git history.  
**See also:** [GDD](../GDD.md), [Chart v2](CHART_FORMAT.md), [Roadmap](../ROADMAP.md).

## 1. Time domains

- The physical audio file begins at **song time 0 seconds**. Rendering and judgement read one coherent song-time domain, adjusted for pause, seeking, editor TEST and practice speed.
- `bpm` is the musical tempo in quarter notes/minute. `offsetMs` is the **signed, integer audio timestamp of musical beat zero**.
- Beat at audio time `s` is `(s - offsetMs / 1000) * bpm / 60`. A positive offset puts beat zero after the start of the file; a negative offset permits silent preroll.
- A chart's saved turn `timeMs` is an **absolute, integer millisecond timestamp relative to the audio file**, not a beat index. This timestamp is authoritative for scoring even after BPM/offset/snap changes.
- `ticksPerBeat` and `snapDivision` are editor projection and placement tools. Changing density may alter where a note appears relative to musical grid lines, but **must not silently change its actual `timeMs`**.
- Game and editor visuals use **fractional song position**. Do not round to whole beats or use accumulated render frame delta to decide a judgement.

## 2. Authored route versus input

- The chart stores an initial heading and ordered turn events for relative 45°, 90° and 135° angles, with eight possible headings. A turn affects the outgoing movement from its time. **Straight** requires no note; 180° reversals are not supported.
- Route coordinates are unbounded. **No Wrap, teleport, physical grid edge, wall or collision** exists when the camera-visible square edge is reached.
- A chart is played regardless of whether input is successful. **No Fail** means the marker always follows the authoritative route.
- Input is an **absolute target heading** using the four remappable directions (default D/F/J/K) and adjacent two-key diagonals. The editor uses the same binds.
- The chord assembly window defaults to 85 ms and can be configured between 40 and 160 ms. A diagonal is a **single** scored event, not two cardinal notes.

## 3. Grading

| Grade | Correct target direction, absolute timing error |
| --- | --- |
| **Perfect** | At most 50 ms |
| **Good** | More than 50 ms, at most 110 ms |
| **Bad** | More than 110 ms, at most 180 ms |
| **Miss** | Wrong outgoing direction in an active window, or no valid input after the late limit |

- Judge each chart event **exactly once**. Presses outside the active window are ignored rather than generating extra misses.
- The late Miss threshold is **strictly after** the note's timestamp plus 180 ms; a key at the exact 180-ms edge is still eligible.
- Key-down timing uses song-time conversion of captured input times; rendering is not the source of score.
- Results do not change route geometry or undo/redo state in the editor.

## 4. Accuracy

~~~text
Accuracy (%) = (100 × Perfect + 75 × Good + 50 × Bad)
               / (Perfect + Good + Bad + Miss)
~~~

Miss counts in the denominator. Before the first judgement the display is **100.00%**. A one-each sample is **56.25%**. Round only for UI display, using two decimals. Editor TEST skips earlier notes and excludes them from both numerator and denominator.

## 5. Chart completion and audio tail

- **Last scorable turn judged => stop music and open Results**. This applies to a final Perfect, Good, Bad or wrong-direction Miss. Do not wait for an unrelated long WAV/MP3 outro.
- A last unpressed note remains eligible through the complete late 180-ms window. After that it is assigned Miss, then Results may open.
- If audio itself ends before the last note's Miss window is complete, the scoring clock continues briefly **without sound**. Do not force an early Miss on the audio-end frame.
- If there are no scorable events (empty chart, or editor TEST starting beyond all notes), fallback completion is **audio end**, so those sessions do not auto-end immediately.
- Pause freezes the song-time judgement progression. Retry resets the judge and starts the current session from its chosen origin. On TEST, earlier skipped turns do not become misses.

## 6. Verification boundaries

Headless tests can verify conversion rules, boundary timing, pauses, chart serialization and scoring counts. They **cannot** establish acoustic speaker output timing, exact polling delay on the owner's keyboard or subjective visual clarity. Physical audio/input latency measurements and calibration are still not claimed as completed.
