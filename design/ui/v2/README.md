# LOCUS — interactive UI prototype

Open [index.html](index.html) locally in a browser. This is a **clickable HTML/CSS/JavaScript prototype** using the real 960 × 540 LOCUS virtual menu resolution. The Page selector above the window is a reviewer-only control, not part of the game UI.

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

## Limitations and scope

This iteration updates the HTML prototype, SVG sources, theme tokens **and the runtime Home/Windows-icon PNG assets** in `assets/ui`. C++ rendering and gameplay code, scoring, persistent run history and data formats are unchanged. The application copies the PNG assets next to the executable on build. The in-memory sample data resets on refresh; demo actions are not an implementation of native features.

No new animation, transitions or visual HUD elements are introduced to scored gameplay.

Check [REVIEW.md](REVIEW.md) for acceptance points.

## Regenerate the real app icon

The **actual running game's icon** is `assets/ui/app-icon.png` (used by `SetWindowIcon`); Home loads `assets/ui/wordmark.png`. Both have been regenerated from the SVG artwork, not merely previewed in HTML. To rebuild them again:

```bash
python -m pip install cairosvg Pillow
python design/ui/v1/render_brand.py
```

Output is 256×256 and 460×136 respectively, with transparent PNG outside the icon circle and the exact approved LO geometry. The existing Windows binary itself is not rebuilt by this design commit; launch a new build to see the updated icon.
