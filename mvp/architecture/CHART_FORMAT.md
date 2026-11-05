# LOCUS — Chart File Format v1

**Task:** A-05 — Define the first chart schema  
**Status:** Implemented chart v1; M-03 prefers millisecond offsets while keeping legacy charts readable  
**Scope:** Unbounded route plane, 2..16 editor grid viewport (default 4×4), quarter-note steps, eight-way headings and relative angle turns  
**Related:** [GDD](../GDD.md), [TIMING](./TIMING.md), [BOUNDARIES](./BOUNDARIES.md), [ROADMAP](../ROADMAP.md)

> This file specifies data and validation rules, not executable parser/editor code. A chart is self-contained gameplay data with a local reference to a separate music file. There is no online service or embedded music.

## 1. Design decisions

- Use **UTF-8 JSON**, one chart per file, with an explicit **formatVersion: 1**. The format version is independent of the game's v0.1 release number.
- Store **starting state plus ordered turn events**, not a redundant list of every route coordinate.
- Represent music time as **signed integer quarter-note beat indices**; negative beat indices can occur before positive audio offsets. The marker moves once per beat; a turn at beat n affects the outgoing step beginning on beat n.
- Store **one constant BPM**, a **signed chart-to-audio offset in milliseconds**, and an explicit preview lead in beats.
- Support **2..16 columns and 2..16 rows** as an **editor-only view density**, with a **4×4 default**. They are neither a musical time signature nor boundaries. The world is infinite: there is **no Wrap or boundary mode**. Legacy JSON is still valid and retains its previously saved grid/start metadata.
- Encode **relative left/right turns of 45°, 90° and 135°**. Legacy `left/right` means 90°. Straight movement requires no event; direct 180° reversals are disallowed.
- Require a chart-relative audio path. Files can be played without accounts or network access.
- Reject incompatible versions and unknown fields instead of silently ignoring data.

## 2. Valid minimal example

Suggested file name: **example.locus.json**. This is illustrative; the referenced audio has not been included in the repository.

~~~json
{
  "formatVersion": 1,
  "title": "Example",
  "audioPath": "audio/example.wav",
  "bpm": 120,
  "offsetMs": 250,
  "grid": {
    "width": 4,
    "height": 4
  },
  "start": {
    "x": 3,
    "y": 0,
    "heading": "right"
  },
  "previewLeadBeats": 4,
  "turns": [
    { "beat": 0, "turn": "left" },
    { "beat": 4, "turn": "right" }
  ]
}
~~~

**Interpretation:** Beat 0 occurs at audio time 0.250 s, and beat 4 at 2.250 s. The Right heading changes to Up at beat 0; each step continues into unbounded world coordinates, even when y becomes negative. At beat 4, Right changes the direction to the right without teleporting. The player stays screen-centred while the visible red path translates. Missing an input never alters this authored movement.

An otherwise valid chart with **"turns": []** is also playable: the marker moves automatically and receives no turn judgements.

## 3. Exact schema, required fields

Every field below is **required** for v1. Do not invent default values during parsing.

| JSON location | Type | Allowed value |
| --- | --- | --- |
| formatVersion | Integer | Exactly **1** |
| title | String | Non-empty title after trimming whitespace |
| audioPath | String | Non-empty chart-relative local path; see Section 6 |
| bpm | Number | Finite and **greater than 0**; quarter notes per minute |
| offsetMs | Number | Preferred: finite signed **milliseconds**; audio time at beat zero |
| offsetSeconds (legacy) | Number | Legacy seconds-based alternative; **exactly one** of `offsetMs` or `offsetSeconds` is required |
| grid | Object | Exactly width and height |
| grid.width | Integer | **2 through 16**, columns |
| grid.height | Integer | **2 through 16**, rows |
| start | Object | Exactly x, y, and heading |
| start.x | Integer | **0 through grid.width−1**, from left to right |
| start.y | Integer | **0 through grid.height−1**, from top to bottom |
| start.heading | String | `up`, `right`, `down`, `left`, `up-right`, `down-right`, `down-left`, `up-left` |
| previewLeadBeats | Number | Finite and **greater than 0**; fractional values allowed |
| turns | Array | Zero or more turn objects, in **strictly increasing beat order** |
| turns[i].beat | Integer | Whole-number beat index **−65536 through 65535**; negative beats are valid before a positive offset if the event time is within the song |
| turns[i].turn | String | `left` or `right` (90°), `left45`, `right45`, `left135`, `right135` |

**JSON type and naming rules**

- Names and enum values are **case-sensitive**. JSON booleans, nulls, or numeric strings cannot substitute for numbers.
- Integer fields must use integer JSON tokens (for example, 4, **not** 4.0, 4e0, or "4") and must fit the supported integer range.
- Decimal numbers are accepted for bpm, offsetMs (or legacy offsetSeconds), and previewLeadBeats. Reject overflow/non-finite numeric results instead of clamping.
- All defined object fields are required, except that **exactly one** of `offsetMs` and the legacy alias `offsetSeconds` must be present. Providing both is an error. **Duplicate JSON member names** and unknown properties at any level are errors, even if a JSON library would normally keep the last duplicate.
- Use exactly one event at most per beat. A charted turn is a discrete action, not a note with duration.
- There are **no other optional gameplay fields** in version 1. New charts use `offsetMs`; existing `offsetSeconds` charts remain backward-compatible. A future format update must explicitly migrate/interpret additional behavior.

## 4. Time and route meaning

Use the definitions and ordering specified in [TIMING.md](./TIMING.md):

~~~text
secondsPerBeat = 60.0 / bpm
offsetSeconds = offsetMs / 1000.0
targetSongSeconds(beat) = offsetSeconds + beat * secondsPerBeat
chartBeatAtSongSeconds(s) = (s - offsetSeconds) / secondsPerBeat
~~~

- **Positive offsetMs**: beat 0 happens **after** the audio file starts.
- **Negative offsetMs**: beat 0 happens **before** the first audio sample; the transport must provide a sufficient silent preroll.
- **Compatibility:** old JSON charts may still use `offsetSeconds` (e.g., `0.25` instead of `offsetMs: 250`). The editor always displays and adjusts the value in milliseconds.
- Beat 0 is the beginning of the first step. A turn event **at beat 0 is legal**.
- At each beat: first apply the relative 45°/90°/135° turn to the **incoming heading**, then travel one world unit along each active axis in the outgoing heading. Diagonals change both x and y in one beat; there is **no modulo or Wrap**, including at former grid edges. Key presses never change the authored route.
- On a beat with no event, keep the current heading. Store neither straight events nor calculated marker positions.
- Missing or incorrect player input changes **judgement only**, never the route.
- Timing-window widths, key bindings, player calibration, results, and scores belong to **session/user settings**, not the chart file.
- The 4×4 chart in the example is only the editor's default viewport. Setting `grid.width=8` and `grid.height=8` changes the visible editor grid spacing; the infinite trajectory and music timeline are identical.

- Constant BPM and integer beat events remain v1 limitations. No fractional event beats, tempo map, hold, Bounce, or dynamic grid change is defined by v1.

## 5. Validation responsibilities

### 5.1 Pure parsing and chart validation (no filesystem/audio)

Reject malformed/non-UTF-8 JSON; missing/duplicate/unknown fields; wrong types; unsupported formatVersion; empty title; invalid audio path; non-positive bpm or previewLeadBeats; non-finite numeric values; grid dimensions outside 2..16; start outside the authored grid; unknown headings/actions; invalid, unsorted, repeated, fractional, or out-of-range beat indices.

Validators must report a **specific field path and reason**, for example **turns[1].beat: duplicate beat 4**. Never silently sort, deduplicate, clamp, coerce a value, choose an alternative heading, or accept unknown future semantics.

The mutable **Editor ChartDraft** may be incomplete/invalid while editing; it must be validated before producing a read-only Chart snapshot for Play or saving a valid chart.

### 5.2 Local audio validation (filesystem and backend)

- Locate the audio file relative to the **directory containing the chart file**, not the process working directory.
- Check existence/readability and whether the chosen audio backend decodes the format. Parsing JSON alone does not access an audio device.
- When file duration **D** is known, a turn target must satisfy **target song time < D**. Negative-beat events belong to audible time before a positive offset. Existing **nonnegative** beat events in the negative-time silent preroll of a negative offset remain playable for compatibility.
- Warn when a timing window could extend beyond audio end, without silently moving the event. At audio completion any unresolved scheduled events are settled once, as defined in TIMING.md.
- Window overlap is dependent on the selected judgement thresholds and is checked **when starting a scoring session**, not as a permanent JSON-schema error.
- If audio is unavailable, distinguish **valid chart data** from **unplayable chart/audio association** in error messages.

## 6. Local audio path rules

The chart stores **audioPath**, a normalized forward-slash **relative** path, such as **audio/song.wav** or **song.wav**.

For v1, reject paths with a leading slash, Windows drive prefix, UNC path, URL scheme, backslash, NUL character, empty segment, or any **.** / **..** path segment. Thus a chart's audio reference must remain within its own chart-directory tree. Check resolved paths for escapes through symlinks/indirection rather than blindly opening them.

Example folder:

~~~text
MySong/
  my-song.locus.json
  audio/
    song.wav
~~~

The editor must not silently copy or upload a selected local track. If the chosen source file is outside the chart folder, the editor should offer an explicit way to place a permitted local copy within the chart folder, or save the chart in an appropriate folder. This is a later **editor interaction decision**, not extra JSON state.

The schema does **not** restrict audio to WAV by filename. WAV should be the first tested backend format; other formats are enabled only when actually supported and verified. A public chart file does not grant redistribution rights to its referenced music.

## 7. Invalid example and diagnostics

This file is valid **JSON syntax**, but invalid **LOCUS formatVersion 1**:

~~~json
{
  "formatVersion": 1,
  "title": "Broken",
  "audioPath": "../outside.wav",
  "bpm": 120,
  "offsetMs": 0,
  "grid": {
    "width": 5,
    "height": 4
  },
  "start": {
    "x": 3,
    "y": 0,
    "heading": "north"
  },
  "previewLeadBeats": 4,
  "turns": [
    { "beat": 4, "turn": "right" },
    { "beat": 4, "turn": "left" }
  ]
}
~~~

Expected validation feedback:

| Location | Reason |
| --- | --- |
| audioPath | Parent-directory traversal is not allowed |
| grid.width | Unsupported dimension 5; expected 4 |
| start.heading | Unsupported value "north" |
| turns[1].beat | Duplicate event at beat 4 |

The implementation may report all errors or stop after one, but must **not silently repair** this chart.

## 8. File writing and compatibility

- **Suggested suffix:** **.locus.json** (ordinary JSON). A filename extension alone does not establish format validity.
- **Encoding:** UTF-8, LF line endings, two-space indentation, final newline; no comments or trailing commas.
- **Serialization:** Write all required properties and the turn array in strict beat order; preserve event times/actions and the authored BPM/offset/preview precision. Output should be stable for readable Git diffs.
- **Persistence:** The platform storage adapter writes via temporary file + replace, leaving the prior valid file unchanged on write failure. Pure chart serialization does not perform filesystem operations.
- **Round-trip:** Parse → serialize → parse must preserve the complete v1 chart meaning, without time rounding.
- **Versioning:** v1 parsers reject unsupported newer versions instead of guessing semantics. Future changes require a new version/migration plan.
- **No online identifiers:** No player IDs, server fields, likes, usernames, or leaderboard data in v1.

## 9. Implementation handoff and verification

| Roadmap ID | What this contract enables |
| --- | --- |
| C-01 | Define plain Chart, Start, and TurnEvent value types |
| C-02 | Implement the BPM/offset conversion exactly |
| C-03 | Parse v1 JSON using strict types and keys |
| C-04/C-05 | Validate metadata, point bounds, and ordered turn events |
| C-06/C-11 | Produce and verify stable serialized round-trips |
| C-07/C-09 | Read chart files and resolve chart-relative local audio |
| C-08 | Save atomically |
| R-03/R-04 | Derive the same automatic route regardless of input |
| E-04/I-11 | Handle selected audio file and portable saved chart |

**A-05 acceptance checklist**

- [x] A minimal versioned playable JSON chart is shown, including timing, audio, start pose, preview lead, and two timed relative turns.
- [x] A valid empty-turn chart is explicitly allowed.
- [x] A syntactically valid but schema-invalid chart and field-specific diagnostics are shown.
- [x] The live chart format retains 4×4 editor defaults but uses an infinite route plane, advancing one world step per beat.
- [x] No Bounce field, extra grid dimensions, tempo/event plugins, or speculative features are introduced.
- [x] Pure validation and file/audio integration are separate.

**Original A-05 work delivered this specification.** The chart parser and first editor controls have since been implemented as part of the later playable LOCUS milestones.

## November 2026 song-length update

The manual Total Beats field is retired. On load, the app derives a half-open
beat interval directly from audio duration `D`, BPM `B` and offset seconds `O`:

```
firstBeat = min(0, floor(-O * B / 60))
endBeatExclusive = ceil((D-O) * B / 60)
valid route step indices: [firstBeat, endBeatExclusive)
```

The start cell/heading describe the earliest rendered step. A negative offset
retains original beat-zero silent preroll for backward compatibility. A positive offset can place beats −5..−1 inside
the song, including scored relative turns at those negative beat indices.
New JSON never writes `lengthBeats`. Existing JSON with that property is
accepted and its manual length ignored after song-duration calculation.
The stored `turns` remain relative Left/Right with explicit 45/90/135
angle variants. Gameplay scores an absolute outgoing heading. A diagonal
heading uses two adjacent arrow keys (e.g. Up+Right) within an 85 ms chord
window, scored once at the later press. The core trajectory never depends
on input. Earlier v1 charts with only `left/right` and cardinal initial
headings remain unchanged.
