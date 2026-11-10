# LOCUS — Chart Format

LOCUS saves UTF-8 JSON with **formatVersion 3**. Note times are absolute whole milliseconds from the start of the referenced audio file, and do not change when BPM or editor snap density changes.

## Example

~~~json
{
  "formatVersion": 3,
  "title": "My Chart",
  "audioPath": "song.mp3",
  "bpm": 120,
  "offsetMs": 0,
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
| `formatVersion` | **3** (older chart versions are rejected) |
| `title` | Valid UTF-8, no control characters, at most 1024 bytes; the editor allows 100 Unicode characters |
| `audioPath` | Nonempty local path **relative to the chart folder**, with forward slashes only |
| `bpm` | Finite positive musical beats per minute; standalone compatibility value, overridden by a managed Song Set manifest |
| `offsetMs` | Signed **integer**, from -3,600,000 to 3,600,000; managed Song Set uses one shared value for all charts |
| `ticksPerBeat` | Optional integer 1–48, default 1 |
| `snapDivision` | Optional 1, 2, 3, 4, 6, 8, 12, 16 or 24, default 1; must divide `ticksPerBeat` |
| `grid.width`, `grid.height` | Integers 2–16. The editor exposes **one square side length**, default 8, and writes equal values |
| `start.x`, `start.y` | Zero-based internal initial position, within grid dimensions; automatically chosen by the editor |
| `start.heading` | `u`, `r`, `d`, `l`, `ru`, `rd`, `ld` or `lu` |
| `previewLeadBeats` | Finite positive forward preview length; default editor/skin presentation is four beats |
| `turns[]` | Ordered array of at most 65,536 events with `timeMs` and absolute `heading` |

Every event sets the **absolute outgoing direction**, independent of the previous heading: `u` (up), `r` (right), `d` (down), `l` (left), `ru` (right-up), `rd` (right-down), `ld` (left-down), `lu` (left-up). Unchanged travel needs no new event. An opposite heading is a 180° reversal; its junction gets a semicircular reverse cap without extra JSON fields. **FLIP H** swaps left/right components; **FLIP V** swaps up/down components of notes in the selected pattern without moving their timestamps. Movement is continuous on an unbounded plane; the square size controls visible density, not world boundaries.

Each `timeMs` is a signed whole number between **-3,600,000 and 86,400,000**. Times must be strictly increasing, and two events must not project to the same musical tick. When music is loaded, notes outside playable audio time (except permitted negative-offset preroll) are rejected. Song duration determines the timeline length; there is **no editable length field**.

BPM and snapping define **only the editor's musical ruler and selectable snap points**. PLAY/TEST compiles a continuous route directly from the exact `timeMs` boundaries; no rounding of a note onto `ticksPerBeat` can move its on-screen corner or judgement. Distance between corners is proportional to the real time interval, so the user-selected screen speed remains constant. Editor tick positions remain a temporary authoring index, and the editor still rejects two notes that would project onto the same tick at its available resolution.

BPM, offset and snapping affect the musical ruler or the authored timeline's coordinate system, not saved note timestamps. For example, at 250 BPM a beat lasts 240 ms; `snapDivision: 3` provides 80-ms placements on the ruler. Changing it later must not rewrite earlier note times.

## Song Set ownership

Installed difficulties in one Song Set share a single audio file beside `set.locus.json`. The metadata file has its own `formatVersion: 1`, `setId`, `title`, `artist`, `audioFile`, and **canonical song-wide `bpm` and `offsetMs`**. Chart JSON includes BPM/offset for standalone playback, but the Song Set manifest is authoritative for managed charts. All difficulty note timestamps stay in absolute milliseconds. It is **not** a playable chart. Managed difficulties are grouped using this manifest and folder, not guessed filenames. See [Song Set storage](SONG_SETS.md).

## File handling

The audio reference must stay inside the chart folder or a subfolder after canonical path resolution. Absolute paths, drive/URL prefixes, backslashes, empty segments, `.`, `..` and unsafe path characters are rejected. Saving charts to a folder that does not contain their referenced audio is rejected. File writes use a temporary file and an on-volume replacement. Overwrites create a previous-version backup in `.locus-history/`. A managed Set's audio is never silently replaced on import.

Unknown fields, duplicate keys, unsupported versions, invalid timestamps and malformed colors/data are not silently ignored. Only version 3 with abbreviated absolute headings is supported. Previous chart JSON formats and long heading strings do not parse; no compatibility layer or migrator is provided before release.

For gameplay rules and scoring see [GDD](GDD.md).
