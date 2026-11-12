# LOCUS · Interface Studio v2

**Status:** complete, navigable **design prototype**. The game's C++ UI, gameplay, chart formats and persistence are **not modified** by this directory.

Open [index.html](index.html) locally in a current desktop browser. No build process, dependencies, fonts, CDN or server required. The wordmark is loaded from the **approved** `../v1/wordmark.svg`. The corner `LO` logo is intentionally absent from Home; the app icon remains unchanged.

## What's included

| Window / workspace | Design coverage | Interactive demonstration |
| --- | --- | --- |
| **Home** | Centered approved wordmark; four clearly weighted buttons; visible Exit | Navigation, keyboard focus, quit confirmation |
| **Song Library** | Song tree with nested difficulties, selected chart, rating breakdown, Set metadata, artwork | Song/chart selection, search, empty state, Play / Edit / Listen, import/export modals |
| **Settings** | Game, Display, Key bindings, Audio & offset | Switches, frame cap, volume, bind capture for PLAY/MENUS/EDITOR, offset, sample 18-tap calibration |
| **Skins** | PATH, PLAYER, COLORS, MY SKINS, permanent preview panel | Ranges/toggles, element colors, profile selection, Save/Save As/Delete mock flows |
| **Editor · SET** | Song Set identity, shared audio/artwork, difficulty list | Select/create/duplicate difficulty, import/export, delete/restore, confirmation |
| **Editor · ROUTE** | Pattern toolbar, large route pane, up to 2 additional comparisons, direction pad with press counts, inspector | Add/remove sample notes, flip/rotate, copy/paste/undo/redo, pane focus, preview speed/path range |
| **Editor · AUDIO** | Difficulty name, song preview start, shared BPM/offset table | Edit title, add/delete/change sample tempo marker |
| **Editor · FIELD** | Initial heading, read-only field and gameplay SKIN context | Change initial heading; start heading is not duplicated in ROUTE |
| **Editor timeline** | Selected-beat ruler, zoom/snap, whole-song overview, transport and practice rates | Seek, choose rates, toggle preview state; no audio played |
| **Listen** | Clean audio preview workspace and controls | Example seek, pause/restart, choose-music modal |
| **PLAY (reference)** | Representative playfield + accuracy/judgment HUD | Pause, resume, retry confirmation, leave confirmation, sample Results |
| **Results** | Accuracy, Perfect/Good/Bad/Miss, judged notes, action hierarchy | Retry, Library, Editor |
| **Components & states** | Color/typography tokens, button styles, inputs, switches, tags, recovery dialogs | Dialog gallery, focus/disabled/success/error/empty states |

There is **one reusable modal component** covering quit, unsaved work, difficulty/Set removal, skin deletion, leave/retry, import/export, Save As, sample file errors, success and chart comparison.

## Design rules

- **Quiet hierarchy.** Avoid instructional paragraphs scattered around controls, duplicate toolbars, repeated section titles, redundant keys on visible buttons and decorative visual noise.
- **Semantic actions.** Primary action uses dusty rose; important secondary actions use soft blue; routine actions are neutral; destructive actions demand explicit confirmation.
- **Stable layout.** Toolbars stay near their content; the chart editor has dedicated tool, viewport, inspector and timeline zones. Comparisons never take over the timeline.
- **Menu interactions only.** Home hover is fast (~55 ms). No animations, overlays or added effects are introduced to scored PLAY. Even the prototype PLAY controls have no CSS transitions.
- **One design language.** Colors, density, typography, shape, focus rings, disabled, error, hover and selection states are shared across screens. Layout adapts to smaller windows using internal scrolling instead of overlapping controls.
- **Accurate ownership.** Song-wide audio/timing/artwork live in SET/AUDIO. Initial heading belongs to FIELD. Preview speed in ROUTE is editor-only. Real gameplay SKIN settings live in Skins.
- **Recoverability.** Destructive actions are clearly named and confirmed. Unsaved work is protected in the demonstration's editor navigation flow.

## How to review

1. Open `index.html`. Home is the starting screen. The vertical rail on inner screens is a **prototype navigation aid**; the eventual app can choose its final shell based on review.
2. Follow **Play → select difficulty → Edit → ROUTE → Compare → AUDIO → SET → FIELD**.
3. Open **Skins** and try sliders, color changes and profiles; return to **Settings** and try rebinding, Display and the illustrative calibrator.
4. Open **Components & states** above the preview to inspect dialogs, error/empty states and design tokens.
5. Test a narrow window, high-contrast focus via **Tab**, **Esc**, and **Ctrl+K** (quick navigation). In Editor, **Ctrl+S** operates only on in-memory sample state.

## Deliberate boundaries

This is a **design prototype**, not an alternate application. Titles, songs, scoring results, waveforms, routes and color presets are **fictional sample data**, not a reading of your library. No filesystem access, native file picker, game input capture, audio output, chart serialization, skin persistence or real difficulty calculation is connected. Buttons for such actions show a prototype dialog/notice rather than claiming an operation succeeded on disk.

This directory is the design source for visual review before any further C++ migration. The historical Home-only mockup remains in `../v1/` for brand assets, but **v2 is the current interface design reference**.
