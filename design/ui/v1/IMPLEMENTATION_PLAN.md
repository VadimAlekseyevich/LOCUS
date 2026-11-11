# UI migration plan — approved direction, implementation pending

## Current state

- `src/app/ui_button.h` already implements shared chrome, drawing and three styles used by menu, editor and skin. **Evolve this, do not add an unrelated widget framework.**
- `src/app/transition_easing.h` provides smoothstep/ease; `src/app/screen_fade_transition.h` provides 65 ms exit + 95 ms entrance; `src/app/play_outro_transition.h` handles the separate in-game result transition.
- `menu_ui.cpp`, `chart_editor_ui.cpp`, `skin_menu.cpp` and `app_frame_renderer.cpp` still contain locally hardcoded colors, borders and text styles. Centralize those without moving business logic.
- `viewport.h` keeps UI coordinates in a 960 × 540 virtual canvas; continue sharing layout rectangles between renderer and input for proper window scaling.

## Common UI foundation

1. Semantic `UiTheme` colors/spacing/radius/typography tokens, independent of user-configurable gameplay SKIN.
2. One family of Button, IconButton, Toggle, TextInput, Slider, Tabs, Panel, Dialog, ScrollList, Toast and keyboard focus/hover/press/disabled states. Keep hit testing and drawing in sync; never move hitboxes to match animation.
3. Shared text measurement and clipping for UTF-8 labels; preserve Windows font fallback until an approved appropriately licensed font is provided.
4. A presentation-only `UiMotion` API for state transition progress (opacity, small translation, color, scale). Stable monotonic clock; animation must never change gameplay scoring, input timestamps, selection, saved chart data or audio.

## Motion targets — proposals for approval, not implemented yet

| Interaction | Initial target | Presentation |
| --- | ---: | --- |
| Hover | 130 ms | soft fill/border interpolation, no glow |
| Press | 80 ms | subtle downward movement or scale ≤ 1% |
| Focus/selected | 160 ms | color + visible focus ring |
| Screen change | reuse 65 + 95 ms | preserve current `ScreenFadeTransition` until separately tuned |
| Modal open/close | 170/130 ms | quiet fade and ≤ 6 px movement |
| Notices | about 170 ms | fade in/out; critical messages remain readable |

All durations are wall-clock rather than frame-count based. Honor reduced-motion settings, support keyboard access and don't hide controls behind hover-only effects. Never animate critical scoring cues with UI motion.

## Migration order (one approved mockup → one C++ integration)

1. Shared theme + primitives + motion scaffolding; behavior must remain unchanged.
2. Main menu/Home: approved centered layout and brand; fully functional Play, Editor, Skins, Settings, Exit.
3. Song library: nested charts, import/export, selection, restore/delete, error/empty states.
4. Settings: controls, display, input rebinding, global-offset manual field and metronome calibrator.
5. SKIN screen: profiles, color wheel and sliders; keep gameplay skin separate from app theme.
6. Editor shell: SET/ROUTE/FIELD and up to three comparison panes, saving, timeline navigation, test play and modals.
7. Listen, Results and shared dialogs. Gameplay playfield geometry and rhythm readability are **not** subject to automatic visual redesign.

## Quality gates

- Review each screen as a code-rendered prototype before merging any related UI code.
- Check 960×540, small/larger windows, 16:9 and non-16:9; mouse alignment, focus indicators, disabled/error/confirmation states.
- Verify Escape and quit confirmations, modified keyboard binds, paused Play, editor unsaved work, Ctrl+S, Song Set import and offset calibration.
- Observe live UI motion on Windows hardware. Keep music clock and scoring independent of UI transitions.
- Do not add generic components speculatively; extract when a common need is demonstrated.
