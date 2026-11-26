# LOCUS — native UI acceptance

Checklist for real Windows display/input review. Existing headless tests
provide layout and command evidence, not visual acceptance. Record build,
window size, DPI, data and screenshots/video for defects.

- [ ] HOME: readable approved wordmark, adequate buttons, obvious focus;
  Home EDIT opens a clean draft.
- [ ] Song selection: actual artwork or theme, wide searchable virtual tree,
  metadata and V4 rating/history; second click/Enter starts focused chart, not during text input; EDIT opens selected chart. MODS overlay uses rate sliders and preserves history space; IMPORT supports multi-selection.
- [ ] Song Setup: audio/tempo/preview, metadata, difficulty rename and
  background, import/export, delete/restore accessible. First click selects,
  second click or Enter opens Chart Editor.
- [ ] Chart Editor: separate upper ruler and full-song overview, readable
  3×3 compass and counts, START, snap/zoom, transform tools, rate entry,
  TIME copy and TEST; direction keys never author notes.
- [ ] Comparison: equal maximal squares, stable positions and same song time,
  independent draft/undo, no extra seek on focus change.
- [ ] SETTINGS: display/audio-offset and binding groups; conflicts, cancellation,
  calibration, fullscreen and font fallback.
- [ ] SKINS: first profile tab owns rename, HSV wheel and hex agree with preview,
  same preview bounds across tabs, save/error/dirty guard and restore.
- [ ] PLAY/TEST: readable route at 50% and 200–600 px/s; diagonal, repeat and
  reversal patterns; no scored-HUD scrims; combo/accuracy and right-gutter grades do not
  obscure route. Cursor hidden in scored active play, visible in spectator playback; pause/leave guard works.
- [ ] RESULTS: actual grades, saved attempt history via PLAY, retry, correct
  older-revision/rules and mod labels; WATCH REPLAY, pause and backward/forward seek, including while paused. Autoplay never saves a score.
- [ ] All screens: Esc navigation without persistent title/back strip,
  visible focus, one command per gesture, modal input ownership, literal
  Unicode text, scrolling, resize/drag, usable small and wide layouts.
  Review 800×450 through 1920×810 and portrait, Windows DPI 100–200%.
- [ ] Keep menu animations separate from active gameplay timing; evaluate
  accessibility/reduced-motion behavior without assuming mockup features exist.

See [engineering checklist](../../../docs/REFACTORING_PLAN.md#t13).
