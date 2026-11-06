# LOCUS — independent gameplay skins

Skins are a **global rendering profile**, not part of a song, chart,
difficulty, scoring rules, timing, or authored route. A chart renders exactly
the same underlying geometry and judgement under any skin.

## Runtime and storage

- `src/app/skin.h/.cpp`: versioned `Skin` model, JSON parsing, validation,
  default values, atomic file save.
- `src/app/skin_menu.h/.cpp`: standalone SKIN menu with PATH, PLAYER/HUD,
  and COLORS pages, slider controls, numeric text inputs, live path preview,
  switch controls, and an editable UTF-8 skin name.
- `src/app/skin_controls.h/.cpp`: centralized values and independent slider
  and typed-value safety bounds; shared by UI and tests.
- The active profile is loaded at startup from
  `UserDataDirectory()/Skins/active.locus-skin.json`. On Windows this is
  typically `%LOCALAPPDATA%\\LOCUS\\Skins\\active.locus-skin.json`.
- The SKIN menu uses a **draft**. SAVE applies it to every Play session,
  including editor Test Play. BACK/Escape discards unsaved changes.
- LOAD reads any skin JSON into the draft; SAVE AS exports a shareable
  `*.locus-skin.json` and activates it. RESET changes only the draft
  until SAVE. CTRL+O, CTRL+S, CTRL+SHIFT+S are available as shortcuts.
- Charts continue to be `*.locus.json` under Songs, and do not include
  any skin fields or per-chart appearance overrides.

## Editable profile fields (version 1)

- PATH: ahead distance `lookaheadBeats` (default **4 beats**),
  width, outline scale, fade-curve exponent, opacity, junction radius
  and glow switch.
- PLAYER/HUD: marker radius, camera pixels per route cell, marker glow,
  hit-effect scale, visibility of the bottom score HUD.
- COLORS: independent RGBA (0–255 integer channels) for background, path,
  outline, junctions, player, player core, HUD background, HUD text,
  and hit flash. Every channel has its own slider and numeric input.

## Sliders, manual input, and interaction

Numeric parameters use a **draggable slider** for everyday values and an
adjacent **typeable field** for precise or more extreme values. Click a field
to select its current number; type the replacement, then press Enter or click
outside to validate and apply. Escape restores the previous value. Invalid,
NaN/infinite, nonnumeric, or out-of-bounds entries are rejected without
mutating the preview. Dot and comma decimal separators are supported.

| Parameter | Slider range | Typed range |
| --- | --- | --- |
| Path lookahead (beats) | 0.5–12 | 0.25–64 |
| Path width (px) | 1–12 | 0.25–40 |
| Outline scale | 1–3 | 1–8 |
| Fade curve exponent | 0.5–2.5 | 0.1–8 |
| Path opacity | 0–1 | 0–1 |
| Junction size (px) | 3–18 | 1–64 |
| Player size (px) | 6–22 | 1–80 |
| Camera scale (px/cell) | 30–80 | 10–250 |
| Hit flash scale | 0.5–2 | 0.1–8 |
| RGBA channels | 0–255 | 0–255 (integer) |

A number outside the slider range stays intact and the slider thumb remains
pinned to the nearest end (in gold); only explicitly dragging the slider
brings the number into the slider range. The three boolean presentation
options are now **switches**, not plus/minus buttons.
The JSON schema remains version 1 and stays separate from charts.

Defaults preserve the existing forward-only four-beat path behavior and
existing route authoring look. The playfield clips drawn segments against
`[currentBeat, currentBeat + lookaheadBeats]`, so changing the number of beats
never exposes a trail *behind* the player. The opacity/width functions share
the same configured horizon. All chart timing and scoring code remains
unchanged. The editor ROUTE *authoring* view stays developer-controlled to
avoid unintentionally changing editing hitboxes; only scored Play/Test uses
the global skin.

## Safety and compatibility

- JSON `version: 1` is required; missing keys, malformed colors, excessive
  file size, non-finite or out-of-range numbers are rejected.
- Failed LOAD does not change the draft or active skin. Failed SAVE does not
  activate the draft. Corrupt active skin on startup falls back to defaults.
- No texture paths, scripts or resources from skins are loaded.
- `tests/skin_tests.cpp` covers parsing, round-trip, validation and file I/O.
  `tests/path_fade_tests.cpp` preserves the four-beat default contract.

See `mvp/examples/default.locus-skin.json` for a complete profile.
