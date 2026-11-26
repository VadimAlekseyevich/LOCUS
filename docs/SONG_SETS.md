# LOCUS — Song Sets and data recovery

Song Set `title`, `artist`, `mapper` and `tags[]`, difficulty titles,
source audio filenames and skin-profile names use **UTF-8**. Tags and mapper
belong to the **whole Song Set**; individual difficulty JSON files do not
contain them. Windows filesystem paths use Unicode APIs;
authored-name limits are in Unicode characters rather than arbitrary bytes.

## Installed layout

Permanent songs live in **%LOCALAPPDATA%\LOCUS\Songs**. Each managed Song Set
has one directory containing a shared WAV/MP3, a **set.locus.json** manifest
and any number of independently editable difficulty JSON files. Each set may
also store one optional **shared background image** (PNG/JPEG).

    %LOCALAPPDATA%/LOCUS/
      Songs/
        song-<technical-id>/
          .locus-lock                   OS writer-lock placeholder
          set.locus.json                setId/title/artist/mapper/tags/audioFile/tempoMap/previewStartMs/backgroundFile
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

The Song Set manifest stores optional `mapper` (UTF-8, up to 100 characters)
and a single `tags` array (up to 16 nonempty unique tags, maximum 32 UTF-8
characters each; commas separate tags only in the editor). New manifest writes
include both fields; existing manifest v2 files without them load as empty
metadata. Library indexing, portable `.locuspack` export/import and direct
Song Set metadata updates preserve these fields without rewriting difficulty
files or recalculating chart ratings. A title edit changes the display title,
not the stable Song Set ID or chart filenames.

Each managed Song Set owns **one shared ordered tempo map**, saved in manifest
version 2. Song Setup has a BPM/OFFSET (MS) table; each row changes tempo at
that absolute song time. The first marker defines beat zero, later markers
start a new local downbeat at their exact timestamp; their tempo lasts until the next row
(or the end of the song). Earlier time uses the first BPM. All difficulties
share the map, and all authored note timestamps stay in song milliseconds.
A BPM change may project multiple distinct millisecond events into one editor tick; all their original timestamps are preserved and they remain separately editable.
Only manifest v2 and chart v4 are supported.

Each Song Set also stores **previewStartMs** (default 0), independent of
BPM/offset. In **Song Setup → AUDIO & TIMING**, enter an exact whole-millisecond position
(0..3,600,000), or Ctrl+V to paste a value copied by clicking the TIME display on ROUTE. Enter saves,
Esc cancels, and no increment/decrement buttons are used. PLAY only reads it.
A separate menu audio decoder fades the old song out before the newly
selected song fades in. Song Set previews start from their own saved position.

In EDIT, open a chart belonging to a **managed Song Set**, return to Song Setup
and choose **ADD BACKGROUND** (or replace the existing image) to attach a PNG or JPEG.
Use **REMOVE BG** to clear it. Image data must successfully decode before
LOCUS installs a copy; invalid files leave the previous background untouched.
The original is untouched and all difficulties share the same background. It appears in Song Select, EDIT, PLAY and Results, with its aspect
ratio preserved: the image fills the physical window and any excess is
center-cropped at the edges, never stretched. A dark overlay keeps routes and
labels legible; artwork never changes BPM, note timestamps or scores.

The `backgroundFile` manifest field stores a safe flat filename or an empty string.
Portable `.locuspack` archives
include referenced backgrounds and validate their type, extension, size
(maximum 24 MiB) and path before publishing. Bad/missing artwork must not
make an otherwise valid chart disappear. Folders without a current Song Set manifest are ignored by the song library.

The files on disk are authoritative. The library index is rebuilt at startup
or on explicit rescan (also after imports/deletion). Unchanged chart files,
identified by last-write time, size and content fingerprint, reuse compact metadata and difficulty results. Full note vectors are materialized on demand and verified against the indexed fingerprint. Asynchronous scans publish complete revisions. Changing the Song Set's BPM or offset refreshes
musical-grid projections but does not change difficulty for unchanged note timestamps; timestamp
changes in an individual chart invalidate only that chart. A shared timing
edit refuses to publish if difficulty files were externally changed or added
after the last scan: refresh first. Export is different: it enumerates and validates the saved files under the writer locks and does not require a fresh catalog. Within each Song Set, difficulties are displayed in
ascending calculated V4 rating (easiest first), using title and filename only
to break ties. A Song Set's technical ID stays independent of its
visible title.

When editing an installed difficulty, changing its title and pressing Ctrl+S
also renames its .locus.json file within that Song Set. A conflicting filename
receives a numeric suffix rather than overwriting another difficulty. Save
failure attempts to restore the original filename; successful saves keep the
editor and library selection attached to the new path. The chart format is
unchanged. Song Setup -> OPEN FOLDER opens that Song Set in Windows Explorer.
SKIN -> SKINS -> OPEN FOLDER opens the personal Profiles directory.

## Everyday workflow

1. **Create a song:** Home -> EDIT -> ATTACH AUDIO (WAV/MP3), select and open the draft. This installs the initial Song Set if it is not already managed. Return to Song Setup to edit shared metadata, artwork or timing. Author the chart and Ctrl+S to save the difficulty.
2. **Create difficulty:** PLAY -> select an installed song -> Song Setup ->
   NEW, then Ctrl+S. No duplicate external authoring copy is created.
3. **Edit difficulty:** select it -> EDIT -> rename/edit notes and Ctrl+S.
   BPM/offset belong to the entire song, not a single chart.
   Ctrl+Shift+S creates a separate difficulty without overwriting the original.
   TEST is in-memory; Esc/H returns to the same unsaved draft.
4. **Copy difficulty:** Song Setup -> select difficulty -> DUPLICATE.
5. **Share a complete set:** Song Setup -> EXPORT creates a
   `.locuspack` archive. This is a standard store-only ZIP, with metadata,
   shared audio, optional background art and all valid saved chart JSONs. Export reads live disk files, checks set identity, unique difficulty IDs, shared audio and timing; unsaved edits are not included. Song Setup -> IMPORT installs `.locuspack` archives as whole
   Song Sets. An existing `setId` merges into its installed folder:
   matching `difficultyId` values update in place, new IDs add difficulties,
   and missing IDs in the package do not delete installed difficulties.
   Identical charts are not rewritten. Existing set metadata, shared tempo
   and audio remain authoritative; different audio bytes are rejected.
   A current chart without an installed ID may be repaired at the exact
   matching package filename and title inside the same set.
6. **Import charts:** Song Setup -> IMPORT creates/finds a managed Song Set
   or adds a difficulty to the current Set if its audio bytes match.
   A persistent difficulty ID resolves its installed chart even after
   renaming or moving the source; an ID-less draft uses its original source
   path. Reimport updates rather than duplicating; explicit COPY still
   creates a new identity. If an ID matches multiple sets, select the target.
   Changed files are backed up; batches are validated before writing and
   attempt rollback on a publication failure.
   PLAY and Song Setup import dialogs accept multiple packs/JSON files via Ctrl/Shift. Each file is processed independently; a failed file does not undo other successful imports, and one final library refresh publishes the batch. The summary distinguishes failed imports from saved-but-unindexed files. The first failure is shown in the notice; individual failures are logged.
7. **Delete:** Song Setup has separate DELETE CHART (selected difficulty) and
   DELETE SET (entire song) buttons. Y confirms, N/Esc cancels. You cannot
   delete the final difficulty alone: delete the whole Set instead.
   ROUTE uses Backspace/Delete or the center DEL button for selected notes;
   Ctrl+Z restores a removed note.
8. **Restore:** Song Setup -> RESTORE restores the newest item from the LOCUS
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
  packages are rejected. Only uncompressed ZIP entries are accepted for now. Package filenames must be valid UTF-8, flat and portable across Windows and Linux; normalized name collisions, reserved device names and unsafe characters are rejected. The manifest is bounded to 64 KiB, chart JSON to 8 MiB; duplicate keys and excessive JSON depth are rejected. The portable structural validator is shared by desktop import and headless builds. Full media decoding and server publication are separate concerns.
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
Current formats: chart v4, Song Set manifest v2, skin v2, settings v3, Results v2 and replay LOCREP04. Unsupported versions are rejected.

## Remaining limitations

There is no built-in previous-chart-version backup browser, archive compression, support for generic
Deflate-compressed ZIP imports, or automatic staging-file garbage collection yet. Physical Windows testing
for device timing, denial of filesystem writes and sudden power loss is still
required.
