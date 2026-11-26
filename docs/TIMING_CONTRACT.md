# Input and audio-clock contract

This is the runtime contract for PLAY and Editor TEST. It describes **current**
sources of truth and the checks required before making timing changes. Neither
FPS nor visual easing may influence scored time.

## Time domains

| Domain | Authoritative source | Consumers |
| --- | --- | --- |
| OS keyboard event | Windows `GetMessageTime()` (queue timestamp, uptime milliseconds) | `native_key_capture.cpp` |
| Application monotonic | `steady_clock`, same origin as `MonotonicSeconds()` | `SongClock`, `GameSession` |
| Authoritative song time | `DemoTransport::SongSeconds` or `ChartPlayback::AudioSeconds` | Raw input to calibration, `ChartClockFrame::judgeSeconds` |
| Rendered song time | `ChartClockFrame::visualSeconds` | Avatar, camera, route presentation only |
| Musical beat | Song milliseconds through `tempoMap` and `ticksPerBeat` | Route and editor grid |
| Judgement time | `CalibratedSongSeconds(rawSongTime, globalOffsetMs, practiceRate)` | `TurnJudge::PressHeading` / `Advance` |

The audio backend may report only its current consumed position, even when
queried with a historical OS timestamp. `GameSession` therefore captures a
single `SampledSongTime` (monotonic time, audio cursor, practice rate) per
frame and projects each individual key press back to its original monotonic
timestamp. At 50% speed, two physical presses 7 ms apart become 3.5 ms apart
in chart time; they do **not** collapse into one decoder-buffer cursor. The
original judgement windows are unchanged. During the silent tail the
end-of-audio anchor is used instead.

The Windows backend receives each keydown/keyup in the **same WndProc** used
to guard window close. It converts the 32-bit message uptime to the application
monotonic origin with `GetTickCount64()` and a sampled steady-clock anchor.
An event is sorted by its original timestamp and capture-order sequence ID.
`GameSession` converts each timestamp, not the later render-frame boundary,
to calibrated song time. At a physical press/release/re-press burst all edges
remain separate. Native Windows event timestamps have a nominal **1 ms unit**
but **no demonstrated 1 ms physical-key latency accuracy**. Windows timer
granularity, queued-message scheduling, USB keyboard scanning and the
audio-output delay still require measurement.

On platforms without the Windows event adapter, raylib's frame-level
`IsKeyPressed` is explicitly a less-precise fallback; it does not preserve
every transition between two frames.

## Playback source selection

- **Custom chart during silent preroll:** `ChartPlayback` advances a
  `SongClock` only before physical audio starts. Negative song time is
  impossible to obtain from a decoder cursor.
- **Custom chart at 100% after audio starts:** `ChartPlayback` delegates to
  `LocalMusic::SongSeconds()`, which uses its anchored monotonic rate clock
  at normal speed. Switching from a delayed slow consumed cursor back to
  normal rate preserves the song position rather than jumping to the
  independent preroll clock. This is not a measured hardware cursor.
- **Custom chart at a non-native rate:** after the audio stream begins,
  `ChartPlayback::AudioSeconds()` requests `LocalMusic::SongSeconds()`.
  `LocalMusic` prefers the raylib consumed cursor when using the ordinary
  pitch-shifted stream and the backend supports that cursor. SoundTouch's
  pitch-preserving branch currently uses a synthetic rate clock, **not**
  verified sample-accurate hardware presentation time. If no usable consumed
  cursor is present, it also falls back to the rate clock.
- **Built-in demo:** `DemoTransport::SongSeconds()` supplies the authoritative
  audio-time domain.
- **Visuals:** `ChartPlayback::Snapshot()` samples the judge cursor first,
  then may interpolate only `visualSeconds` with
  `PlaybackCursorSmoother`. NEVER feed the interpolated visual position to
  `TurnJudge`.
- **Global latency compensation:** positive `globalOffsetMs` subtracts
  `offsetMs / 1000 * practiceRate` from raw song time; it must never mutate
  chart event timestamps or tempo markers.

## Transport transitions

| Transition | Rule |
| --- | --- |
| Start with a note at song 0 | `ScoredChartStartSeconds` chooses a negative start at least one second ahead of the note, rounded to musical measures |
| Silent preroll | Same continuous, rate-scaled `SongClock` and authored route. No audio stream or scoring-time shift |
| Crossing song time 0 | Start the physical audio stream once; preserve the overshoot and existing clock anchor |
| Pause | Freeze song time; do not judge newly pressed keys or build up pending input |
| Resume | Exclude paused wall time; no new origin for chart timestamps |
| Change editor preview rate | Preserve the exact song position while changing slope; a rate change while paused must remain paused. PLAY and replay use a fixed rate from launch. |
| Seek / retry | Explicit new origin and clear the presentation cursor. Replay/AT seek with Left/Right or the bottom timeline, preserving pause and reconstructing judge state without skipped hitsounds. Normal scored PLAY cannot seek except its pre-note intro skip. |
| Final audio frame | `GameSession` retains a short **silent judgement tail** so late inputs can finish the last note's standard window |
| Results | Save only completed eligible scored attempts, never Editor TEST, replay, Autoplay, failed or abandoned attempts. Spectators wait until audio end. |

## Acceptance and known evidence gaps

`locus_precise_direction_input_events` replays identical recorded physical
events under frame batches at 30, 60, 144 and 240 FPS and after an extended
frame, comparing scores and signed errors. `locus_win32_message_time_and_key_mapping`
uses the production Win32 translator and tests key repeat, focus loss, and
numpad vs navigation identity. `locus_editor_to_play_to_results_journey`
exercises the real controllers, completed-outro transition, persistence and
a 7 ms inter-key diagonal. `locus_chart_clock_transport_contract` checks
preroll, tempo changes, 100/50/10% editor/transport rates, pause, rate change while paused,
seek, completion and missing audio output with a deterministic output backend.

The SoundTouch decode retains raylib's converted 32-bit float `Wave` as its full-song PCM buffer, without a second full copy. `LOCAL_MUSIC_STRETCH_READY` reports decode wall milliseconds and
retained PCM bytes at the first slowdown; these are not a process-wide
peak-memory measurement. A bounded streaming decoder remains necessary to
avoid memory scaling linearly with song duration.

**These are logical checks, not a sound-card latency benchmark.** T04 cannot be
fully accepted without Windows loopback or an external audio recording that
correlates the audible click to hardware key events at 100/50/125% PLAY and 10% editor preview, including
startup, pause, seek and the first slowdown's SoundTouch decode latency /
peak resident memory. Record the device model, sample rate, buffer size,
OS timer resolution, playback rate, number of samples, median, P95 and worst
absolute error. Do not declare the DSP cursor to be consumed-sample based
until those measurements confirm it.
