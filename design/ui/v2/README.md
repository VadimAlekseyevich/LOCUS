# LOCUS · UI prototype v2

Open [index.html](index.html) locally in a browser. This is the current interactive design reference for the application, not a replacement for the native C++ interface.

## Screens

- **Home** — Play, Editor, Skins, Settings, Exit.
- **Song library** — song tree, difficulty selection, chart rating, **Results history at the left of the selected chart**.
- **Settings** — Game, Display, Key bindings, Audio & offset, calibration.
- **Skins** — Path, Player, Colors, My Skins with a shared preview.
- **Editor** — SET, ROUTE, AUDIO, FIELD, comparison panes, note editing tools and two timeline scales.
- **PLAY sample** — a static playfield used solely to demonstrate completion and the result flow.
- **Results detail** — opens only after a chart has been completed or a saved result is selected from that chart's history. **No standalone Results navigation.**
- **Components** — internal design gallery, reached through the studio toolbar.

**Listen is out of scope for the present design pass.** No Listen or standalone Results page appears in the main navigation.

## Per-chart Results flow

1. Select a difficulty in the Song Library; its **Results** panel appears directly below the song tree on the left.
2. Click an existing result to open its accuracy and judgment breakdown.
3. The static PLAY sample includes **Finish chart** to simulate the real game's automatic completion. Each completion appends another result for the current difficulty and immediately opens its detail.
4. Return to the library to view all results for that difficulty. Switching to another difficulty shows **only its own history**. An empty history reads *No results*.

The example results exist **in JavaScript memory only**, and a page reload resets them. No native audio, file access, persistent saving, ranking, scoring, or gameplay code has been changed. Do not present simulated runs as actual recorded attempts.

## Implementation requirement — future native work

When implemented in the game, completion of the **final judged note** must:

- Finalize exactly one attempt, avoiding duplicates from re-rendering, pause, retry or results navigation.
- Store the attempt under a stable **Song Set + difficulty identity** (not a path or display title), with completion timestamp, all judgment counts, notes judged and computed accuracy. Save atomically and avoid disk I/O on the scoring hot path.
- Open the existing Results UI automatically with the newly completed attempt.
- Read **multiple attempts per chart** in the Library and allow opening any historical Results view.
- Keep histories isolated between difficulties; define explicit behavior for difficulty deletion and recovery.
- Preserve the integrity of chart files and Song Set formats. Results are a separate player-history concern.
- Work for the built-in demo as well as imported songs without accidentally attaching two charts to one history.

This is a **product behavior requirement**, not an implementation claim. The storage location, persistence format, profile ownership, retention policy and score/version consistency must be decided during the native implementation.

## Review

The prototype's design prioritizes actionable labels and values instead of instructional microcopy. All major navigation, control states, unsaved-work warnings and modal examples remain usable. Keyboard: Tab, Enter, Escape, Ctrl+K; editor prototype Ctrl+S.

The source brand remains in [v1](../v1/). No changes were made to chart, Song Set, SKIN or settings file formats.
