# LOCUS — Song Sets and data recovery

## New installed layout

Permanent songs live in **%LOCALAPPDATA%\LOCUS\Songs**. Each managed Song Set
has one directory containing a shared WAV/MP3, a **set.locus.json** manifest
and any number of independently editable difficulty JSON files.

    %LOCALAPPDATA%/LOCUS/
      Songs/
        song-<technical-id>/
          .locus-lock                   OS writer-lock placeholder
          set.locus.json                setId/title/artist/audioFile
          song.mp3                      SHARED, never silently replaced
          Easy.locus.json               individual note timestamps/BPM/offset
          Hard.locus.json
          .locus-history/
            Easy.locus.json.tmp-....bak previous chart versions
      Trash/
        item-<timestamp>-.../
          origin.json                   original relative path
          payload                       deleted chart or whole Song Set

The files on disk are authoritative. The library index is rebuilt at startup
or on explicit rescan. A Song Set's technical ID stays independent of its
visible title.

## Everyday workflow

1. **Create a song:** Home -> EDIT -> O (attach WAV/MP3), author the chart
   and Ctrl+S. The new chart and shared audio are installed as one Song Set.
2. **Create difficulty:** PLAY -> select an installed song -> N, then edit and
   Ctrl+S. No duplicate external authoring copy is created.
3. **Edit difficulty:** select it -> E -> Ctrl+S changes that chart only.
   Ctrl+Shift+S creates a separate difficulty without overwriting the original.
4. **Copy difficulty:** select it and press Ctrl+D.
5. **Share a complete set:** select the song and press X to export a
   `.locuspack` archive. This is a standard store-only ZIP, with metadata,
   shared audio and all chart JSONs. C imports `.locuspack` packages as whole
   Song Sets. LOCUS refuses to overwrite an already-installed set with the
   same setId.
6. **Import charts:** C creates/finds a managed Song Set; Shift+C explicitly
   adds a chart to the selected Set, but only if its audio bytes match.
   Reimporting the same source chart updates that difficulty, not a duplicate.
7. **Delete:** Delete confirms deletion of a difficulty; Shift+Delete confirms
   deletion of an entire Song Set. Y confirms, N/Esc cancels. You cannot
   delete the final difficulty alone: delete the whole Set instead.
8. **Restore:** U restores the most recent recoverable item from the LOCUS
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
Some legacy v1/v2 parsing and old audio-version directories may still be
read by the code, but new work targets managed Song Sets. Any deliberate
breaking change must be disclosed to the developer.

## Remaining limitations

There is no built-in history browser, archive compression, support for generic
Deflate-compressed ZIP imports, or automatic staging-file garbage collection yet. Physical Windows testing
for device timing, denial of filesystem writes and sudden power loss is still
required.
