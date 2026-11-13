# LOCUS — interactive UI prototype

Open [index.html](index.html) locally in a browser. This is a **clickable HTML/CSS/JavaScript prototype** using the real 960 × 540 LOCUS virtual menu resolution. The Page selector above the window is a reviewer-only control, not part of the game UI.

## Native UI port (in progress)

- The C++ PLAY song-selection screen uses the new **352px compact left column** with room for run history and a **564px song tree** on the right. Mouse hitboxes and keyboard/tree navigation were updated together.
- The native right-hand tree searches song titles, artists and difficulty titles. Search typing cannot accidentally launch PLAY/EDIT; F5 refresh and chart launches still work.
- The menu theme and reusable button classes use the violet/sky palette. Settings now presents **Display / Audio**, with volume/offset under Audio and no Hit Flash setting.
- Song Set backgrounds already fill the window and are now less heavily darkened on the selector.
- Native completed-attempt **JSON persistence and per-chart Results history** are implemented. PLAY shows real records and opens a saved result read-only. **Replay playback is intentionally not implemented.** See [Results storage](../../../docs/RESULTS.md).
- Song Set title, artist, **set-wide mapper and tags** now have native persistent
  storage, indexed values, a validated atomic edit API and editor SET view data.
  Difficulty/chart JSON deliberately has **no tags**. Native SET text fields
  now edit the managed Song Set's shared metadata (Enter to save, Esc to cancel).
- SET, ROUTE and AUDIO now use a shared native editor header and upper note ruler,
  with one bottom whole-song timeline, Play/Restart icons, playback-rate menu and
  millisecond copy button. ROUTE uses an icon toolrail, larger square previews,
  a right-hand direction pad and START controls; AUDIO has two columns.
- SKINS now uses the same dark violet/sky menu palette, a full-width tab bar,
  skin name in the header and actions beneath the preview. The existing
  functional wheel, sliders, swatches, skin profiles and controls remain.
- Further visual polish and interaction review may be needed. Scored gameplay,
  chart timing and chart file formats were not changed.

## Design direction — Nightbloom

The interface uses a gently dark violet-blue base, sky-blue interactive accents, softer violet selection states and sparse soft lilac emphasis on important values. It remains a dark application, not a bright website.

The palette is defined in [tokens.json](tokens.json) and [theme.css](theme.css). [song-backdrop.svg](song-backdrop.svg) supplies a fullscreen *illustrative* Song Set background for the HTML PLAY song-selection screen. The approved **glyph geometry** of the wordmark and LO app icon is unchanged, with recolored vector assets in [v1](../v1/).

The UI is composed of the existing reusable controls and the layout in [styles.css](styles.css). Styling, focus, hover, pressed and selected states are all interactive. Reduced-motion settings are supported. The static scored-gameplay reference has **no UI transitions or animations**.

## PLAY — song/beatmap selection

**PLAY on Home is the song/beatmap selection screen**, not the scored gameplay screen.

- The illustrative background fills **the entire 960 × 540 screen** behind the content. There is **no dedicated central artwork container**.
- **Left:** Song Set title, artist, mapper, tags, BPM, duration, current difficulty, its metrics and the per-chart Results list, plus PLAY and EDIT actions.
- **Right:** a much wider scrollable song/difficulty tree, search and Song Set/import/rescan actions.
- Selecting another chart switches the summary and results; no invented osu! leaderboard, mods or gameplay mechanics.

The actual full-screen song background should eventually be sourced from the selected Song Set. The SVG artwork here is purely a design placeholder, not an implemented disk-backed image loader.

## Other screens

Home: four prominent actions and Exit, with the approved \`locus.\` wordmark.

Editor: SET / ROUTE / AUDIO. The zoomable **note ruler stays above** the workspace. The bottom full-song timeline holds miniature Play/Restart controls on the left and playback-speed dropdown and copyable timestamp on the right. ROUTE has icon tools on the left of the expanded preview, a 3×3 direction pad, start heading, editor speed/path length and equal-sized compare panes. SET edits Song title, Artist, Mapper and Tags. AUDIO edits chart title/preview timing and the shared tempo map.

Settings: Display and Audio, with key bindings alongside. Skin: PATH / PLAYER / COLORS / MY SKINS and a dynamic preview. Results: a contextual record for one chart, accessible from that chart's history or the simulated completion flow.

## Data and behavior during the native UI port

Already implemented in C++: managed Song Set metadata, shared audio/background
images, song-wide tempo map and preview position, chart title, difficulty
calculation, UTF-8 text, editor transformations, skin settings and the menu
theme. The Song Set's mapper and tags are now persisted and searchable.

Implemented in native C++: completion-history storage and read-only reopening
of prior Results; the records in the HTML prototype are still sample data. The
remaining refinements to visual layout and input bindings are UI work, not
new chart formats. Tags are **never per difficulty**.

## Limitations and scope

This stage includes the HTML/CSS/SVG prototype, runtime icon/wordmark PNGs and native **PLAY/SET/ROUTE/AUDIO/SKINS/Results/Settings UI**. Scored gameplay, scoring calculations, chart data formats and judgement timing are unchanged. The application copies the PNG assets next to the executable on build. The in-memory sample data resets on refresh; demo actions are not an implementation of native features.

No new animation, transitions or visual HUD elements are introduced to scored gameplay.

Check [REVIEW.md](REVIEW.md) for acceptance points.

## Regenerate the real app icon

The **actual running game's icon** is `assets/ui/app-icon.png` (used by `SetWindowIcon`); Home loads `assets/ui/wordmark.png`. Both have been regenerated from the SVG artwork, not merely previewed in HTML. To rebuild them again:

```bash
python -m pip install cairosvg Pillow
python design/ui/v1/render_brand.py
```

Output is 256×256 and 460×136 respectively, with transparent PNG outside the icon circle and the exact approved LO geometry. The existing Windows binary itself is not rebuilt by this design commit; launch a new build to see the updated icon.
