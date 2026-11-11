# LOCUS — Song Sets and data recovery

Song Set `title` and `artist`, difficulty titles, source audio filenames and
skin-profile names use **UTF-8**. Windows filesystem paths use Unicode APIs;
authored-name limits are in Unicode characters rather than arbitrary bytes.

## New installed layout

Permanent songs live in **%LOCALAPPDATA%\LOCUS\Songs**. Each managed Song Set
has one directory containing a shared WAV/MP3, a **set.locus.json** manifest
and any number of independently editable difficulty JSON files. Each set may
also store one optional **shared background image** (PNG/JPEG).

    %LOCALAPPDATA%/LOCUS/
      Songs/
        song-<technical-id>/
          .locus-lock                   OS writer-lock placeholder
          set.locus.json                setId/title/artist/audioFile/tempoMap/previewStartMs/backgroundFile
          song.mp3                      SHARED, never silently replaced
          background-<id>.png           optional, shared by all difficulties
          Easy.locus.json               difficulty name/turns/absolute note timestamps
          Hard.locus.json
          .locus-history/
            Easy.locus.json.tmp-....bak previous chart versions
      Trash/
        item-<timestamp>-.../
          origin.json                   original relative path
          payload                       deleted chart or whole Song Set

Each managed Song Set owns **one shared ordered tempo map**, saved in manifest
version 2. EDIT → AUDIO has a BPM/OFFSET (MS) table; each row changes tempo at
that absolute song time. The first marker defines beat zero, later markers
continue the accumulated beat count and their tempo lasts until the next row
(or the end of the song). Earlier time uses the first BPM. All difficulties
share the map, and all authored note timestamps stay in song milliseconds.
If a new map merges notes on the editor tick grid, the change is rejected.
Only manifest v2 and chart v4 are supported.

Each Song Set also stores **previewStartMs** (default 0), independent of
BPM/offset. In **EDIT → AUDIO**, enter an exact whole-millisecond position
(0..3,600,000), or Ctrl+V to paste a value copied by clicking the TIME display on ROUTE. Enter saves,
Esc cancels, and no increment/decrement buttons are used. PLAY only reads it.
A separate menu audio decoder fades the old song out before the newly
selected song fades in. Song Set previews start from their own saved position.

In EDIT, open a chart belonging to a **managed Song Set**, switch to SET
and choose **ADD BG** (or **REPLACE BG**) to attach a PNG or JPEG.
Use **REMOVE BG** to clear it. Image data must successfully decode before
LOCUS installs a copy; invalid files leave the previous background untouched.
The original is untouched and all difficulties share the same background. It appears in Song Select, EDIT, PLAY and Results, with its aspect
ratio preserved: the image fills the physical window and any excess is
center-cropped at the edges, never stretched. A dark overlay keeps routes and
labels legible; artwork never changes BPM, note timestamps or scores.

The optional `backgroundFile` manifest key stores a safe flat filename.
Portable `.locuspack` archives
include referenced backgrounds and validate their type, extension, size
(maximum 24 MiB) and path before publishing. Bad/missing artwork must not
make an otherwise valid chart disappear. Folders without a current Song Set manifest are ignored by the song library.

The files on disk are authoritative. The library index is rebuilt at startup
or on explicit rescan (also after imports/deletion). Unchanged chart files,
identified by last-write time and file size, reuse their parsed data and
difficulty rating rather than being rescored at every refresh. Changing the
Song Set's shared BPM or offset invalidates those derived ratings; timestamp
changes in an individual chart invalidate only that chart. A shared timing
edit refuses to publish if difficulty files were externally changed or added
after the last scan: refresh first. Within each Song Set, difficulties are displayed in
ascending calculated V2 rating (easiest first), using title and filename only
to break ties. A Song Set's technical ID stays independent of its
visible title.

When editing an installed difficulty, changing its title and pressing Ctrl+S
also renames its .locus.json file within that Song Set. A conflicting filename
receives a numeric suffix rather than overwriting another difficulty. Save
failure attempts to restore the original filename; successful saves keep the
editor and library selection attached to the new path. The chart format is
unchanged. EDIT -> SET -> OPEN FOLDER opens that Song Set in Windows Explorer.
SKIN -> MY SKINS -> OPEN FOLDER opens the personal Profiles directory.

## Everyday workflow

1. **Create a song:** Home -> EDIT -> O (attach WAV/MP3), author the chart
   and Ctrl+S. The new chart and shared audio are installed as one Song Set.
2. **Create difficulty:** PLAY -> select an installed song -> EDIT -> SET ->
   NEW, then Ctrl+S. No duplicate external authoring copy is created.
3. **Edit difficulty:** select it -> E -> rename/edit notes and Ctrl+S.
   BPM/offset belong to the entire song, not a single chart.
   Ctrl+Shift+S creates a separate difficulty without overwriting the original.
   TEST is in-memory; Esc/H returns to the same unsaved draft.
4. **Copy difficulty:** EDIT -> SET -> select difficulty -> COPY.
5. **Share a complete set:** EDIT -> SET -> EXPORT creates a
   `.locuspack` archive. This is a standard store-only ZIP, with metadata,
   shared audio, optional background art and all chart JSONs. EDIT -> SET -> IMPORT installs `.locuspack` archives as whole
   Song Sets. LOCUS refuses to overwrite an already-installed set with the
   same setId.
6. **Import charts:** EDIT -> SET -> IMPORT creates/finds a managed Song Set
   or adds a difficulty to the current Set if its audio bytes match.
   Reimporting the same source chart updates that difficulty, not a duplicate.
7. **Delete:** EDIT -> SET has separate DELETE (selected difficulty) and
   DELETE SET (entire song) buttons. Y confirms, N/Esc cancels. You cannot
   delete the final difficulty alone: delete the whole Set instead.
   ROUTE uses Backspace/Delete or the center DEL button for selected notes;
   Ctrl+Z restores a removed note.
8. **Restore:** EDIT -> SET -> RESTORE LAST restores the newest item from the LOCUS
   Trash, including after an application restart.

An external chart opened through Ctrl+O is installed into the library when
saved. LOCUS does not silently overwrite the external source.

## Recovery and safety

- **Saving:** JSON is validated and written to a temporary file, flushed,
  then the previous JSON is backed up to .locus-history before an atomic
  on-volume replacement. If validation, backup or replacement fails, the old
  chart remains available.
- **Import:** audio is first copied to a temporary file. Existing Set audio
  is compared byte-for-byte; changing it without explicit remapping is
  rejected to avoid breaking other difficulties.
- **Deletion:** song folders or chart files are atomically renamed into
  LOCUS/Trash with an origin journal. Restoring refuses to overwrite an
  occupied original path.
- **Package integrity:** LOCUS checks ZIP entry names, file counts, total
  size, CRC-32 and central directory consistency; unsafe paths and malformed
  packages are rejected. Only uncompressed ZIP entries are accepted for now.
  A failed extraction stays outside the library and is removed.
- **Concurrency:** imports and managed editor saves hold a library-wide OS
  write lock and then a per-Set lock. Crashes release those locks automatically;
  the harmless .locus-lock files may remain.
- **Backups:** To restore a specific previous chart version manually, close
  LOCUS and copy its corresponding .bak over the original .locus.json.
- **Interruptions:** aborted imports may leave unreferenced staging files.
  LOCUS avoids guessing which files are safe to erase. Do not remove Trash or
  staging files while relying on recovery.

Back up **%LOCALAPPDATA%\LOCUS** before changing chart/storage formats.

## Pre-release compatibility

Before the first public release, **backwards compatibility is not promised**.
Only chart v4, Song Set manifest v2 and skin v2 are supported; older formats are rejected.

## Remaining limitations

There is no built-in history browser, archive compression, support for generic
Deflate-compressed ZIP imports, or automatic staging-file garbage collection yet. Physical Windows testing
for device timing, denial of filesystem writes and sudden power loss is still
required.
