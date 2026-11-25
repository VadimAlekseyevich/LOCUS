# Completed Results — format v2

Each completed LOCUS chart attempt creates **one small JSON record**. New attempts additionally save a separate binary scoring replay when capture succeeds. The JSON summary contains no event payload.

## When records are created

- After the final judged note and end-of-chart transition, **once** per completed session.
- Retrying, leaving early, revisiting Results, or a partial TEST from the editor never creates a completed record.
- Normal PLAY sessions at lower practice rates include the selected speed as `practiceRate`. The built-in demo uses a separate identity.
- The newly completed result opens in Results. Selecting a prior result from the PLAY screen's left history panel opens its statistics **read-only**; no replay playback is available.
- One chart can have as many completed results as the user creates. PLAY and RESULTS use scrollable virtualized history lists sized to the viewport.

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
  "practiceRate": 1.0,
  "variablePracticeRate": false
}
```

The current `judgementVersion` is 1, independent of Difficulty V4. The `judgementVersion` is authored once next to the actual scoring windows and grade rules in `core/judgement.h`, not independently in the UI or results writer. Bumping that constant marks old records as earned under previous judging rules.

The score uses the game's existing weighted accuracy formula: `(100*Perfect + 75*Good + 50*Bad) / notesJudged`. Timestamps are stored as UTC Unix milliseconds and rendered as UTC.

Difficulty history is keyed by **Song Set ID + permanent 128-bit difficultyId**, not its filename or display title. Authoring-only metadata (difficulty title, editor snap, preview lead) does not change the gameplay revision. Each attempt also records a stable gameplay-content fingerprint and the judgement rules version so old and updated authored revisions remain distinguishable without losing history. The Library results list and historical Results page label attempts from **OLDER CHART REVISION** or **OLDER JUDGEMENT RULES**; practice speed remains explicit. Different practice rates remain separate values inside attempts. `variablePracticeRate` is a sticky flag when playback speed changed during the run; `practiceRate` then means the *last* speed, not a uniform speed. Both Results screens label such runs **VARIABLE RATE**, even if playback returned to 100%. Restart resets the flag. This adds an explicit field to pre-release v2 records; earlier v2 records without it are treated as fixed-rate. Deleted files do not automatically delete result records.

**Pre-release breaking change:** v1 result records, keyed by the former filename scheme, are not loaded by v2 and stay on disk. There is no automatic migration, since filename-keyed history has no reliable way to associate renamed files. Result JSON `version` is now 2.

Writes use exclusive temporary files, flushed before atomic no-clobber publication. History reads on the Song Select screen run on a background worker with cancellation; rapid difficulty switching cannot publish stale results into a newly selected chart. UI shows `LOADING RESULTS...` until the selected key is loaded. Loading validates ranges and identifiers, ignores incomplete `.pending` files and malformed JSON files, and sorts the remaining attempts newest-first. Disk reads happen on library-selection changes, writes happen only on completion, never on the judgement hot path.

## Timing telemetry

New completed PLAY attempts additionally store an optional `statistics` object in the existing v2 JSON: `maxCombo`, `timedHits`, `meanErrorMs`, `unstableRate`. The last two are in original song-time units: signed mean error in milliseconds (negative = early), and 10 times the population standard deviation of hit errors. Only Perfect/Good/Bad physical arrows contribute; misses and wrong-direction presses never contribute timing samples. Both diagonal components are independent samples. The judge accumulates telemetry in constant memory and resets it on retry or TEST origin. The summary is persisted with the existing atomic result transaction, not per frame. Results shows maximum combo and mean hit error, with UR shown only for at least two hits. Older v2 records without telemetry remain readable and show unavailable fields as "--"; no migration invents missing values. At reduced or variable practice rate these are song-time statistics, not real-time device latency measurements. This summary is not a replay format. Judgement rules, chart and skin formats are unchanged.

## Scoring replays (format 1)

New completed PLAY attempts record scoring operations in actual dispatch order, using calibrated song seconds. Each fresh cardinal press reaching TurnJudge (including ignored presses) is captured until completion. An Advance operation is stored only when it expires notes; an explicit Finish operation is stored if it resolves remaining notes. Idle render frames and key releases are not needed for the discrete scoring model. Editor TEST and abandoned attempts do not publish a replay. Retry resets capture. Intro-skip presses consumed outside TurnJudge, transport changes, pauses, and camera state are not part of this format, so it is a scoring trace, not yet an audiovisual replay.

The optional v2 JSON flag `hasReplay` identifies a sidecar named `<attempt-id>.locus-replay` in the same directory. Its little-endian binary header is 24 bytes: 8-byte ASCII LOCREP01 magic, uint32 judgement version, binary64 chart offset in seconds, uint32 event count. Each event is 9 bytes (uint8 operation: Up/Right/Down/Left/Advance/Finish = 0..5, binary64 song seconds; Finish has zero timestamp). Up to 2,000,000 events; exact byte count, finite values and operation codes are checked. No rounding or sorting of event timestamps. The same core TurnJudge rejudges the trace; verification requires the matching gameplay revision and compares counts, max combo and timing metrics. This is not cryptographic anti-cheat.

The sidecar is exclusively published before the small result JSON, which is the commit marker. If replay creation fails, the score still saves without a replay. A failed summary publication removes only the writer's own sidecar; a crash may leave an unreferenced sidecar, ignored by history. Old JSON results remain readable; history never parses replay payloads. A missing/corrupt replay does not remove the saved score. Results indicates recording availability, but interactive playback and transport recording remain future work.
