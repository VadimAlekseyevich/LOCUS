# LOCUS — Nightbloom visual reference

Open [index.html](index.html) locally. This is an offline HTML/CSS/JavaScript
mockup with illustrative data and a reviewer-only page selector. It uses a
960×540 design reference, not the actual native UI layout. Its simulated
controls and example values are not production feature/status claims.

Production documentation: [GDD](../../../docs/GDD.md),
[UI architecture](../../../docs/UI_RML_MIGRATION.md),
[README](../../../README.md), [Difficulty V4](../../../docs/CHART_DIFFICULTY.md).

## Visual direction

Dark violet-blue base, sky-blue interactions, violet selection, sparse lilac
emphasis. Approved locus. wordmark and LO icon geometry are in [v1](../v1/README.md).
Palette and design tokens: [tokens.json](tokens.json), [theme.css](theme.css).
Layout and illustrative controls: [styles.css](styles.css), `screens.js`,
`app.js`, `model.js`. Browser data resets on refresh.

## Current native screens (reference for review)

- HOME: approved centered branding, Play/Edit/Skin/Settings and Exit.
- Song selection: wide scrollable tree and search on the right, actual Song
  Set metadata, Difficulty V4, event/key counts and chart-specific history
  on the left; PLAY/EDIT/IMPORT/RESCAN. Enter starts selected chart.
  Artwork only when supplied by the set; no synthetic background.
- EDIT: Song Setup combines metadata, artwork, audio, tempo map, preview start
  and difficulty list; click again or Enter opens Chart Editor (installing an attached new draft if needed). Chart Editor
  has upper ruler, compass/START/tools, equal square comparison previews,
  bottom Play/RST, full-song overview, numeric 1–100% preview rate and
  copyable milliseconds. Esc returns to Song Setup.
- SETTINGS: DISPLAY and AUDIO / OFFSET; key-binding groups for PLAY/MENUS/EDITOR.
- SKINS: SKINS first (rename and profiles), PATH, PLAYER, COLORS; one stable
  preview rectangle, hex after the HSV wheel in the left panel.
- RESULTS: actual completed attempt and virtualized history; no replay.
- Active PLAY/TEST: separate raylib gameplay, not a native or HTML scene.
  Combo, accuracy, optional practice rate and right-gutter grades. Esc pause.

Native menus have no permanent title/back strip, use adaptive layout and
semantic commands. The mockup may intentionally differ in navigation,
controls or simulated data; runtime RML/RCSS and C++ are authoritative.
[Review](REVIEW.md) defines acceptance, not proof of visual completion.

## Brand assets

The runtime icon and wordmark are PNGs under `assets/ui/`. Regenerate from
the approved SVGs with `python design/ui/v1/render_brand.py` (CairoSVG/Pillow).
See [v1 brand source](../v1/README.md).
