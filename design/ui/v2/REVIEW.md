# LOCUS Interface Studio · review checklist

This is a **visual/interaction acceptance checklist**, not a runtime test plan.

## Navigation and clarity
- [ ] Home has **no** secondary corner logo; wordmark remains centered.
- [ ] Skins and Settings have sufficient contrast alongside Play/Editor.
- [ ] Exit looks like a deliberate secondary button; Escape asks for confirmation.
- [ ] All screens are reachable by visible navigation; Ctrl+K offers quick jumping.
- [ ] Buttons have hover, active and keyboard-focus states, with no excessive animation.
- [ ] No invented shortcut hints or meaningless always-visible decorations.

## Library and Song Sets
- [ ] Song rows expand to show difficulties and highlight the chosen chart.
- [ ] Rating has speed, rhythm, coordination, reading, stamina and peak.
- [ ] Play, Listen, Edit and SET management are distinct actions.
- [ ] Empty library and zero-result search are intentionally designed.
- [ ] Delete is recoverable and destructive modal buttons remain explicit.
- [ ] Background belongs to the Song Set, not to a difficulty.

## Editor
- [ ] Tabs read **SET / ROUTE / AUDIO / FIELD**.
- [ ] SET manages difficulty rows with click-to-open and no redundant OPEN/PREV/NEXT.
- [ ] ROUTE has visual direction-pad counts; diagonals contribute to cardinal totals.
- [ ] Flip H/V and 90° rotation are easy to find without duplicating tools.
- [ ] Comparison selection allows up to two references plus active chart.
- [ ] ROUTE practice rates live beside bottom transport, not a top-level toolbar.
- [ ] AUDIO owns song-wide BPM table; FIELD owns initial heading only.
- [ ] Timeline ruler and whole-song overview are distinct, stable zones.
- [ ] Save / unsaved / delete / quit dialogs preserve meaningful user choices.
- [ ] Long chart names, Cyrillic names and error messages can be displayed.

## Settings / Skins / Others
- [ ] Settings has game, display, keybindings and global offset.
- [ ] Key capture explicitly distinguishes PLAY, MENUS/LISTEN and EDITOR.
- [ ] Calibration shows a distinct in-progress and completed state (18 taps).
- [ ] Skins retains the real PATH/PLAYER/COLORS/MY SKINS categories and each real control.
- [ ] Skin preview is separate from application theme and has no sound.
- [ ] Listen has coherent transport controls; Results prioritizes actual judgment metrics.
- [ ] PLAY preview is **static**, with no new visual motion proposals.

## Responsive and accessibility
- [ ] At 960×540, overflow scrolls inside workspace; controls are not hidden behind overlays.
- [ ] At 1280×720 / 1440×900, library/editor regions remain legible.
- [ ] Keyboard Tab, Enter, Escape and Ctrl+K operate consistently.
- [ ] Focus outline and muted text remain legible; reduced-motion preference works.
- [ ] Dialog buttons can be reached by keyboard; destructive actions are not defaulted silently.

## Prototype boundaries
- [ ] All example song and score data is clearly labeled sample.
- [ ] No demo action suggests native file/audio/gameplay work actually occurred.
- [ ] No changes are made to C++ or production formats during design review.
