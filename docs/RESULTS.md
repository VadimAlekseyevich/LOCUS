# Completed Results (native v2)

Each completed LOCUS chart attempt creates **one small JSON record**. A Results file is a judgement summary, **not a replay**: it contains no key timings, button sequence, trajectory recording, video or executable replay.

## When records are created

- After the final judged note and end-of-chart transition, **once** per completed session.
- Retrying, leaving early, revisiting Results, or a partial TEST from the editor never creates a completed record.
- Normal PLAY sessions at lower practice rates include the selected speed as `practiceRate`. The built-in demo uses a separate identity.
- The newly completed result opens in Results. Selecting a prior result from the PLAY screen's left history panel opens its statistics **read-only**; no replay playback is available.
- One chart can have as many completed results as the user creates. The library shows 4 at a time with scroll/up/down controls.

## Disk format and identity

Stored under `%LOCALAPPDATA%/LOCUS/Results/` on Windows, **outside the Song Set**. Each chart has an opaque hashed subdirectory and one `.json` file per attempt. Other Song Set files, archived `.locuspack` files and difficulty files are not modified by the Results writer.

v2 record fields:

```json
{
  "version": 2,
  "id": "1760000000000-0",
  "songSetId": "stable-song-set-id",
  "difficultyId": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "gameplayRevision": "11111111111111111111111111111111",
  "judgementVersion": 1,
  "songTitle": "Song title at completion",
  "chartTitle": "Difficulty title at completion",
  "completedAtMs": 1760000000000,
  "perfect": 123,
  "good": 12,
  "bad": 3,
  "miss": 2,
  "notesJudged": 140,
  "accuracyPercent": 95.35714285714286,
  "practiceRate": 1.0
}
```

The score uses the game's existing weighted accuracy formula: `(100*Perfect + 75*Good + 50*Bad) / notesJudged`. Timestamps are stored as UTC Unix milliseconds and rendered as UTC.

Difficulty history is keyed by **Song Set ID + permanent 128-bit difficultyId**, not its filename or display title. Authoring-only metadata (difficulty title, editor snap, preview lead) does not change the gameplay revision. Each attempt also records a stable gameplay-content fingerprint and the judgement rules version so old and updated authored revisions remain distinguishable without losing history. The Library results list and historical Results page label attempts from **OLDER CHART REVISION** or **OLDER JUDGEMENT RULES**; practice speed remains explicit. Different practice rates remain separate values inside attempts. Deleted files do not automatically delete result records.

**Pre-release breaking change:** v1 result records, keyed by the former filename scheme, are not loaded by v2 and stay on disk. There is no automatic migration, since filename-keyed history has no reliable way to associate renamed files. Result JSON `version` is now 2.

Writes use exclusive temporary files, flushed before atomic no-clobber publication. Loading validates ranges and identifiers, ignores incomplete `.pending` files and malformed JSON files, and sorts the remaining attempts newest-first. Disk reads happen on library-selection changes, writes happen only on completion, never on the judgement hot path.

## Native SET metadata

The first native SET panel also supports Song Set **title, artist, mapper and common tags**. Click a field and type UTF-8; **Enter** saves atomically, **Escape** cancels. A click on another field saves the current value first. Tags are comma-separated, but are persisted as an array once per Song Set. These edits do not touch individual difficulty JSON or gameplay note timing.

New unsaved Song Set drafts must first be saved/installed before their shared metadata can be edited; creating an unsaved pack metadata draft before installation is a separate workflow.

Only the SET panel is migrated here. ROUTE/AUDIO/Skins continue to use the previous native layouts until their dedicated UI port.
