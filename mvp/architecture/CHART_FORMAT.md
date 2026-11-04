# LOCUS — Chart File Format v1

**Task:** A-05 — Define the first chart schema  
**Status:** Proposed architecture contract, pending Stage 1 human gate  
**Scope:** MVP v0.1, fixed 4×4 grid, Wrap, quarter-note steps, relative Left/Right  
**Related:** [GDD](../GDD.md), [TIMING](./TIMING.md), [BOUNDARIES](./BOUNDARIES.md), [ROADMAP](../ROADMAP.md)

> This file specifies data and validation rules, not executable parser/editor code. A chart is self-contained gameplay data with a local reference to a separate music file. There is no online service or embedded music.

## 1. Design decisions

- Use **UTF-8 JSON**, one chart per file, with an explicit **formatVersion: 1**. The format version is independent of the game's v0.1 release number.
- Store **starting state plus ordered turn events**, not a redundant list of every route coordinate.
- Represent music time as **nonnegative integer quarter-note beat indices**. The marker moves once per beat; a turn at beat n affects the outgoing step beginning on beat n.
- Store **one constant BPM**, a **signed chart-to-audio offset in seconds**, and an explicit preview lead in beats.
- Keep the grid fixed at **4×4**. Wrap is the only boundary behavior; **there is no boundary-mode field**.
- Encode only **relative Left and Right** turns. Straight movement requires no event.
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
  "offsetSeconds": 0.25,
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

**Interpretation:** Beat 0 occurs at audio time 0.250 s, and beat 4 occurs at 2.250 s. The initial heading Right turns Left into Up at beat 0, causing a top-to-bottom Wrap from (3,0) to (3,3). Continued Up movement returns the marker to (3,0) at beat 4, where a Right turn produces a right-to-left Wrap on the next step. The marker follows this path even when both player inputs are missed.

An otherwise valid chart with **"turns": []** is also playable: the marker moves automatically and receives no turn judgements.

## 3. Exact schema, required fields

Every field below is **required** for v1. Do not invent default values during parsing.

| JSON location | Type | Allowed value |
| --- | --- | --- |
| formatVersion | Integer | Exactly **1** |
| title | String | Non-empty title after trimming whitespace |
| audioPath | String | Non-empty chart-relative local path; see Section 6 |
| bpm | Number | Finite and **greater than 0**; quarter notes per minute |
| offsetSeconds | Number | Finite signed seconds; audio time at which beat zero occurs |
| grid | Object | Exactly width and height |
| grid.width | Integer | Exactly **4** |
| grid.height | Integer | Exactly **4** |
| start | Object | Exactly x, y, and heading |
| start.x | Integer | 0 through 3, from left to right |
| start.y | Integer | 0 through 3, from top to bottom |
| start.heading | String | One of **"up", "right", "down", "left"** |
| previewLeadBeats | Number | Finite and **greater than 0**; fractional values allowed |
| turns | Array | Zero or more turn objects, in **strictly increasing beat order** |
| turns[i].beat | Integer | Nonnegative whole-number quarter-note index (0 allowed) |
| turns[i].turn | String | Exactly **"left"** or **"right"** |

**JSON type and naming rules**

- Names and enum values are **case-sensitive**. JSON booleans, nulls, or numeric strings cannot substitute for numbers.
- Integer fields must use integer JSON tokens (for example, 4, **not** 4.0, 4e0, or "4") and must fit the supported integer range.
- Decimal numbers are accepted for bpm, offsetSeconds, and previewLeadBeats. Reject overflow/non-finite numeric results instead of clamping.
- All defined object fields are required. **Duplicate JSON member names** and unknown properties at any level are errors, even if a JSON library would normally keep the last duplicate.
- Use exactly one event at most per beat. A charted turn is a discrete action, not a note with duration.
- There are **no optional gameplay fields** in version 1. A future format update must explicitly migrate/interpret additional behavior.

## 4. Time and route meaning

Use the definitions and ordering specified in [TIMING.md](./TIMING.md):

~~~text
secondsPerBeat = 60.0 / bpm
targetSongSeconds(beat) = offsetSeconds + beat * secondsPerBeat
chartBeatAtSongSeconds(s) = (s - offsetSeconds) / secondsPerBeat
~~~

- **Positive offsetSeconds**: beat 0 happens **after** the audio file starts.
- **Negative offsetSeconds**: beat 0 happens **before** the first audio sample; the transport must provide a sufficient silent preroll.
- Beat 0 is the beginning of the first step. A turn event **at beat 0 is legal**.
- At each beat: first apply the scheduled Left/Right change relative to the **incoming heading**, then travel one cell in the outgoing heading. Wrap across any grid edge without additional input or judgement.
- On a beat with no event, keep the current heading. Store neither straight events nor calculated marker positions.
- Missing or incorrect player input changes **judgement only**, never the route.
- Timing-window widths, key bindings, player calibration, results, and scores belong to **session/user settings**, not the chart file.
- Constant BPM and integer beat events are MVP limitations. No fractional event beats, tempo map, diagonal action, hold, Bounce, or dynamic grid change is defined by v1.

## 5. Validation responsibilities

### 5.1 Pure parsing and chart validation (no filesystem/audio)

Reject malformed/non-UTF-8 JSON; missing/duplicate/unknown fields; wrong types; unsupported formatVersion; empty title; invalid audio path; non-positive bpm or previewLeadBeats; non-finite numeric values; grid sizes other than 4×4; start outside grid; unknown headings/actions; invalid, unsorted, repeated, fractional, or negative event beats.

Validators must report a **specific field path and reason**, for example **turns[1].beat: duplicate beat 4**. Never silently sort, deduplicate, clamp, coerce a value, choose an alternative heading, or accept unknown future semantics.

The mutable **Editor ChartDraft** may be incomplete/invalid while editing; it must be validated before producing a read-only Chart snapshot for Play or saving a valid chart.

### 5.2 Local audio validation (filesystem and backend)

- Locate the audio file relative to the **directory containing the chart file**, not the process working directory.
- Check existence/readability and whether the chosen audio backend decodes the format. Parsing JSON alone does not access an audio device.
- When file duration **D** is known, each turn target must occur at audio time **strictly less than D**. Negative-time events are permitted only under the silent-preroll rules of TIMING.md.
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
  "offsetSeconds": 0,
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
- [x] The MVP is fixed 4×4, Wrap-only, one quarter-note step per beat.
- [x] No Bounce field, extra grid dimensions, tempo/event plugins, or speculative features are introduced.
- [x] Pure validation and file/audio integration are separate.

**A-05 delivers a specification only.** No parser, editor, game binary, or automated chart test is created by this task.
