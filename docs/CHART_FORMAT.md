# LOCUS — Chart Format

LOCUS saves UTF-8 JSON with **formatVersion 4** for tap-only charts and **formatVersion 5** for charts containing holds. Both are readable by the current game; v4-only older builds cannot read v5. Note times are absolute whole milliseconds from the start of the referenced audio file, and do not change when BPM or editor snap density changes. The supported JSON file size is **8 MiB**, sufficient for the parser's maximum **65,536 events** even with indented serialization.

## Example

~~~json
{
  "formatVersion": 4,
  "difficultyId": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "title": "My Chart",
  "audioPath": "song.mp3",
  "bpm": 120,
  "offsetMs": 0,
  "tempoMap": [
    {"timeMs": 0, "bpm": 120},
    {"timeMs": 30000, "bpm": 180}
  ],
  "ticksPerBeat": 1,
  "snapDivision": 1,
  "grid": {"width": 8, "height": 8},
  "start": {"x": 3, "y": 3, "heading": "r"},
  "previewLeadBeats": 4,
  "turns": [
    {"timeMs": 1000, "heading": "d"},
    {"timeMs": 2500, "heading": "r"}
  ]
}
~~~

The [working sample](../examples/first-song.locus.json) contains nine turns; supply your own WAV/MP3 as `song.mp3` beside it.

## Fields

| Field | Rules |
| --- | --- |
| `formatVersion` | **4** for taps, **5** for charts with holds; versions below 4 are rejected |
| `difficultyId` | Optional for external drafts; installed difficulties receive a permanent 32-character lowercase hex identifier. Rename preserves it; copy/new difficulty generates a new one |
| `title` | Valid UTF-8, no control characters, at most 1024 bytes; the editor allows 100 Unicode characters |
| `audioPath` | Nonempty local path **relative to the chart folder**, with forward slashes only |
| `bpm` | Finite positive musical beats per minute; first tempo marker value, overridden by a managed Song Set manifest |
| `offsetMs` | Signed **integer**, from -3,600,000 to 3,600,000, matching the first tempo marker |
| `tempoMap[]` | Required in v4. Ordered list of 1–256 `{timeMs,bpm}` markers. The first offset equals `offsetMs` and defines beat zero, and its BPM equals `bpm`. Each subsequent marker starts a new local downbeat at its exact timestamp; all following subdivisions use that new BPM and the upper ruler maintains constant pixels per song second |
| `ticksPerBeat` | Optional integer 1–48, default 1 |
| `snapDivision` | Optional 1, 2, 3, 4, 6, 8, 12, 16 or 24, default 1; must divide `ticksPerBeat` |
| `grid.width`, `grid.height` | Integers 2–16; retained as stored chart metadata. ROUTE preview sizing no longer depends on them |
| `start.x`, `start.y` | Zero-based internal initial position, within grid dimensions; automatically chosen by the editor |
| `start.heading` | `u`, `r`, `d`, `l`, `ru`, `rd`, `ld` or `lu` |
| `previewLeadBeats` | Required finite positive chart metadata retained by the current serializer; ROUTE/PLAY actually use global SKIN PREVIEW TIME (MS) |
| `turns[]` | Ordered array of at most 65,536 events with `timeMs`, absolute `heading` and optional positive integer `holdMs` in v5 |

Every event sets the **absolute outgoing direction**, independent of the previous heading: `u` (up), `r` (right), `d` (down), `l` (left), `ru` (right-up), `rd` (right-down), `ld` (left-down), `lu` (left-up). Unchanged travel needs no new event unless a repeated tap or hold is authored. A hold is a single event, e.g. `{"timeMs": 1000, "heading": "ru", "holdMs": 500}`. It keeps that heading until its tail at 1500 ms; no later event may start before the tail. The tail must fit inside the actual audio duration. A zero duration is encoded as an ordinary tap without a `holdMs` field. Copy/paste and shifts preserve duration in milliseconds. An opposite heading is a 180° reversal; its junction gets a semicircular reverse cap without extra JSON fields. **FLIP H** swaps left/right components; **FLIP V** swaps up/down components of notes in the selected pattern without moving their timestamps. Movement is continuous on an unbounded plane; SKIN speed and preview duration determine on-screen spacing and visible future, not the stored grid dimensions.

Each `timeMs` is a signed whole number between **-3,600,000 and 86,400,000**. Times must be strictly increasing; multiple events may project to the same musical tick as long as their absolute `timeMs` values are distinct. When music is loaded, notes outside playable audio time (except permitted negative-offset preroll) are rejected. Song duration determines the timeline length; there is **no editable length field**.

BPM and snapping define **only the editor's musical ruler and selectable snap points**. PLAY/TEST compiles a continuous route directly from the exact `timeMs` boundaries; no rounding of a note onto `ticksPerBeat` can move its on-screen corner or judgement. Distance between corners is proportional to the real time interval, so the user-selected screen speed remains constant. Editor tick positions remain temporary authoring indices. The editor supports multiple exact millisecond events in the same tick; using Ctrl+click on the note ruler stages an unsnapped time, then a direction-pad click authors that event.

A tempo map preserves absolute song milliseconds: before the first marker its BPM is extrapolated backwards; each later marker starts a new local downbeat (even if the prior segment ended mid-beat), and its BPM applies until the next marker. Reserved fractional-beat indices have zero song duration; no note times are moved. Markers are defined in absolute song milliseconds. Changing any marker reprojects the authoring grid across the entire song, **without rewriting note timestamps**. BPM, offset and snapping affect the musical ruler or the authored timeline's coordinate system, not saved note timestamps. For example, at 250 BPM a beat lasts 240 ms; `snapDivision: 3` provides 80-ms placements on the ruler. Changing it later must not rewrite earlier note times.

## Song Set ownership

Installed difficulties in one Song Set share a single audio file beside `set.locus.json`. The metadata file now writes `formatVersion: 2`, with `setId`, `title`, `artist`, `audioFile`, and **canonical song-wide `tempoMap`**; `bpm` and `offsetMs` mirror the first marker. Chart JSON includes BPM/offset for standalone playback, but the Song Set manifest is authoritative for managed charts. All difficulty note timestamps stay in absolute milliseconds. It is **not** a playable chart. Managed difficulties are grouped using this manifest and folder, not guessed filenames. See [Song Set storage](SONG_SETS.md).

## File handling

The audio reference must stay inside the chart folder or a subfolder after canonical path resolution. Absolute paths, drive/URL prefixes, backslashes, empty segments, `.`, `..` and unsafe path characters are rejected. Saving charts to a folder that does not contain their referenced audio is rejected. File writes use a temporary file and an on-volume replacement. Overwrites create a previous-version backup in `.locus-history/`. A managed Set's audio is never silently replaced on import.

Unknown fields, duplicate keys, unsupported versions, invalid timestamps and malformed colors/data are not silently ignored. Only v4 absolute-heading charts with a required tempo map are supported. Older versions and long heading strings are rejected.

For gameplay rules and scoring see [GDD](GDD.md).
