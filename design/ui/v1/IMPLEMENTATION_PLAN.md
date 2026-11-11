# Menu/UI migration plan — approved direction, implementation pending

## Scope: menus only (hard boundary)

The approved soft blue/rose theme and **all new hover, press, focus, modal and screen UI animations** are for application **menus and editors only**: Home, Song Library, Settings (including calibrator), SKIN configuration, editor SET/ROUTE/FIELD controls, Listen and post-game Results, plus dialogs outside active gameplay.

**No new animations during PLAY**, including the playfield, character/avatar, chart path, turns, hit indicators, accuracy HUD, pause overlay or input feedback. Do not hook `UiMotionState`, `UiDialogMotion` or animated menu widgets into `Screen::Play`, `GameSession`, `PlayPresentation`, `gameplay_ui`, `route_renderer` or `path_mesh`. This UI project does not authorize any gameplay rendering or camera changes.

Existing gameplay/camera movement, scoring effects and PLAY→Results outro remain exactly as they are; they are **outside this redesign**. Existing navigation fade is also left unchanged until explicitly approved. A menu preview is not permission to alter gameplay.


## Current state

- `src/app/ui_button.h` already implements shared chrome, drawing and three styles used by menu, editor and skin. **Evolve this, do not add an unrelated widget framework.**
- `src/app/transition_easing.h` provides smoothstep/ease; `src/app/screen_fade_transition.h` provides 65 ms exit + 95 ms entrance; `src/app/play_outro_transition.h` handles the separate in-game result transition.
- `menu_ui.cpp`, `chart_editor_ui.cpp`, `skin_menu.cpp` and `app_frame_renderer.cpp` still contain locally hardcoded colors, borders and text styles. Centralize those without moving business logic.
- `viewport.h` keeps UI coordinates in a 960 × 540 virtual canvas; continue sharing layout rectangles between renderer and input for proper window scaling.

## Common UI foundation

1. Semantic `UiTheme` colors/spacing/radius/typography tokens, independent of user-configurable gameplay SKIN.
2. One family of Button, IconButton, Toggle, TextInput, Slider, Tabs, Panel, Dialog, ScrollList, Toast and keyboard focus/hover/press/disabled states. Keep hit testing and drawing in sync; never move hitboxes to match animation.
3. Shared text measurement and clipping for UTF-8 labels; preserve Windows font fallback until an approved appropriately licensed font is provided.
4. A **menu-only** presentation API (`UiMotionState`/`UiDialogMotion`) for control states and dialogs. Stable monotonic clock; animation must never run during PLAY or change gameplay scoring, input timestamps, selection, saved chart data or audio.

## Motion targets — proposals for approval, not implemented yet

| Interaction | Initial target | Presentation |
| --- | ---: | --- |
| Hover | 130 ms | soft fill/border interpolation, no glow |
| Press | 80 ms | subtle downward movement or scale ≤ 1% |
| Focus/selected | 160 ms | color + visible focus ring |
| Screen change | reuse 65 + 95 ms | preserve current `ScreenFadeTransition` until separately tuned |
| Modal open/close | 170/130 ms | quiet fade and ≤ 6 px movement |
| Notices | about 170 ms | fade in/out; critical messages remain readable |

These are **menu animation** durations only, measured in wall-clock time rather than frames. Honor reduced-motion settings, support keyboard access and don't hide controls behind hover-only effects. There are **no gameplay animation targets** in this plan.

## Migration order (one approved mockup → one C++ integration)

1. Shared theme + primitives + motion scaffolding; behavior must remain unchanged.
2. Main menu/Home: approved centered layout and brand; fully functional Play, Editor, Skins, Settings, Exit.
3. Song library: nested charts, import/export, selection, restore/delete, error/empty states.
4. Settings: controls, display, input rebinding, global-offset manual field and metronome calibrator.
5. SKIN screen: profiles, color wheel and sliders; keep gameplay skin separate from app theme.
6. Editor shell: SET/ROUTE/FIELD and up to three comparison panes, saving, timeline navigation, test play and modals.
7. Listen, Results and menu dialogs. **PLAY and its pause/HUD overlays are explicitly excluded**: no new transitions, micro-interactions or gameplay effects.

## Quality gates

- Review each screen as a code-rendered prototype before merging any related UI code.
- Check 960×540, small/larger windows, 16:9 and non-16:9; mouse alignment, focus indicators, disabled/error/confirmation states.
- Verify Escape and quit confirmations, modified keyboard binds, paused Play, editor unsaved work, Ctrl+S, Song Set import and offset calibration.
- Observe **menu-only** UI motion on Windows hardware. Keep PLAY unmodified; menu input never takes precedence over gameplay input.
- Do not add generic components speculatively; extract when a common need is demonstrated.
