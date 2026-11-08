# LOCUS — Chart Format

LOCUS saves UTF-8 JSON with **formatVersion 2**. Note times are absolute whole milliseconds from the start of the referenced audio file, and do not change when BPM or editor snap density changes.

## Example

~~~json
{
  "formatVersion": 2,
  "title": "My Chart",
  "audioPath": "song.mp3",
  "bpm": 120,
  "offsetMs": 0,
  "ticksPerBeat": 1,
  "snapDivision": 1,
  "grid": {"width": 8, "height": 8},
  "start": {"x": 3, "y": 3, "heading": "right"},
  "previewLeadBeats": 4,
  "turns": [
    {"timeMs": 1000, "turn": "right"},
    {"timeMs": 2500, "turn": "left"}
  ]
}
~~~

The [working sample](../examples/first-song.locus.json) contains nine turns; supply your own WAV/MP3 as `song.mp3` beside it.

## Fields

| Field | Rules |
| --- | --- |
| `formatVersion` | **2** for new files |
| `title` | Nonempty UTF-8 text, at most 1024 bytes |
| `audioPath` | Nonempty local path **relative to the chart folder**, with forward slashes only |
| `bpm` | Finite positive musical beats per minute |
| `offsetMs` | Signed **integer**, from -3,600,000 to 3,600,000; audio time of beat zero |
| `ticksPerBeat` | Optional integer 1–48, default 1 |
| `snapDivision` | Optional 1, 2, 3, 4, 6, 8, 12, 16 or 24, default 1; must divide `ticksPerBeat` |
| `grid.width`, `grid.height` | Integers 2–16. The editor exposes **one square side length**, default 8, and writes equal values |
| `start.x`, `start.y` | Zero-based internal initial position, within grid dimensions; automatically chosen by the editor |
| `start.heading` | `up`, `right`, `down`, `left`, `up-right`, `down-right`, `down-left` or `up-left` |
| `previewLeadBeats` | Finite positive forward preview length; default editor/skin presentation is four beats |
| `turns[]` | Ordered array of at most 65,536 events with `timeMs` and `turn` |

The seven turn names are `left`, `right` (90°), `left45`, `right45`, `left135`, `right135` and `reverse` (180°). A straight step needs no turn event. A reversal travels directly back along the existing trajectory, without offsets or a separate lane. Only the on-screen turn dot gains a semicircular end cap, perpendicular to the path, on the arriving side; no extra field is saved. Movement is continuous in an infinite world; the NxN size controls **visible density**, not world boundaries.

Each `timeMs` is a signed whole number between **-3,600,000 and 86,400,000**. Times must be strictly increasing, and two events must not project to the same musical tick. When music is loaded, notes outside playable audio time (except permitted negative-offset preroll) are rejected. Song duration determines the timeline length; there is **no editable length field**.

BPM, offset and snapping affect the musical ruler or visual projection, not saved note timestamps. For example, at 250 BPM a beat lasts 240 ms; `snapDivision: 3` provides 80-ms placements on the ruler. Changing it later must not rewrite earlier note times.

## File handling

The audio reference must stay inside the chart folder or a subfolder after canonical path resolution. Absolute paths, drive/URL prefixes, backslashes, empty segments, `.`, `..` and unsafe path characters are rejected. Saving charts to a folder that does not contain their referenced audio is rejected. File writes use a temporary file and an on-volume replacement.

Unknown fields, duplicate keys, unsupported versions, invalid timestamps and malformed colors/data are not silently ignored. LOCUS also **reads version 1 charts** and converts them to v2 on save, so existing user creations remain loadable; new authored examples should always use v2.

For gameplay rules and scoring see [GDD](GDD.md).
