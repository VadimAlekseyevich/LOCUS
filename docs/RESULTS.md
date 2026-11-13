# Completed Results (native v1)

Each completed LOCUS chart attempt creates **one small JSON record**. A Results file is a judgement summary, **not a replay**: it contains no key timings, button sequence, trajectory recording, video or executable replay.

## When records are created

- After the final judged note and end-of-chart transition, **once** per completed session.
- Retrying, leaving early, revisiting Results, or a partial TEST from the editor never creates a completed record.
- Normal PLAY sessions at lower practice rates include the selected speed as \`practiceRate\`. The built-in demo uses a separate identity.
- The newly completed result opens in Results. Selecting a prior result from the PLAY screen's left history panel opens its statistics **read-only**; no replay playback is available.
- One chart can have as many completed results as the user creates. The library shows 4 at a time with scroll/up/down controls.

## Disk format and identity

Stored under \`%LOCALAPPDATA%/LOCUS/Results/\` on Windows, **outside the Song Set**. Each chart has an opaque hashed subdirectory and one \`.json\` file per attempt. Other Song Set files, archived \`.locuspack\` files and difficulty files are not modified by the Results writer.

v1 record fields:

\`\`\`json
{
  "version": 1,
  "id": "1760000000000-0",
  "songSetId": "stable-song-set-id",
  "difficultyFile": "easy.locus.json",
  "songTitle": "Song title at completion",
  "chartTitle": "Difficulty title at completion",
  "completedAtMs": 1760000000000,
  "perfect": 123,
  "good": 12,
  "bad": 3,
  "miss": 2,
  "notesJudged": 140,
  "accuracyPercent": 96.42857142857143,
  "practiceRate": 1.0
}
\`\`\`

The score uses the game's existing weighted accuracy formula: \`(100*Perfect + 75*Good + 50*Bad) / notesJudged\`. Timestamps are stored as UTC Unix milliseconds and rendered as UTC.

The current pre-release difficulty identity is **Song Set ID + installed difficulty filename**. Editing the display title without renaming the file preserves results; renaming/moving the actual file changes the identity. A future persistent per-difficulty ID can remove that limitation in a pre-release format update. Deleted files do not automatically delete their old result records.

Writes use temporary files and same-directory atomic renames. Loading validates ranges and identifiers, ignores incomplete \`.pending\` files and malformed JSON files, and sorts the remaining attempts newest-first. Disk reads happen on library-selection changes, writes happen only on completion, never on the judgement hot path.

## Native SET metadata

The first native SET panel also supports Song Set **title, artist, mapper and common tags**. Click a field and type UTF-8; **Enter** saves atomically, **Escape** cancels. A click on another field saves the current value first. Tags are comma-separated, but are persisted as an array once per Song Set. These edits do not touch individual difficulty JSON or gameplay note timing.

New unsaved Song Set drafts must first be saved/installed before their shared metadata can be edited; creating an unsaved pack metadata draft before installation is a separate workflow.

Only the SET panel is migrated here. ROUTE/AUDIO/Skins continue to use the previous native layouts until their dedicated UI port.
