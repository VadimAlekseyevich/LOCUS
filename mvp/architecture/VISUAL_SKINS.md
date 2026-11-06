# LOCUS — independent gameplay skins

Skins are a **global rendering profile**, not part of a song, chart,
difficulty, scoring rules, timing, or authored route. A chart renders exactly
the same underlying geometry and judgement under any skin.

## Runtime and storage

- `src/app/skin.h/.cpp`: versioned `Skin` model, JSON parsing, validation,
  default values, atomic file save.
- `src/app/skin_menu.h/.cpp`: standalone SKIN menu with PATH, PLAYER/HUD,
  and COLORS pages, live path preview, optional UTF-8 skin name.
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

- PATH: ahead distance `lookaheadBeats` (0.5–16, default **4 beats**),
  width, outline scale, fade-curve exponent, opacity, junction radius
  and glow toggle.
- PLAYER/HUD: marker radius, camera pixels per route cell, marker glow,
  hit-effect scale, visibility of the bottom score HUD.
- COLORS: independent RGBA (0–255 each channel) for background, path,
  outline, junctions, player, player core, HUD background, HUD text,
  and hit flash.

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
