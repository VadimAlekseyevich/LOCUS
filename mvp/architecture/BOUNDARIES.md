# LOCUS — Current Module Boundaries

**Status:** Descriptive inventory, reconciled 2026-11-06.  
This file describes the existing code organization; it does **not** authorize a new architectural layer or require a rewrite. The early 4×4 Wrap architecture proposal is retained in Git history, not enforced on the current eight-direction game.

## Guiding rules

1. **Authored route is immutable while scoring.** Wrong input changes a grade, never route movement.
2. **Music timestamps drive play.** Render FPS and window size never decide a note's saved audio time.
3. **View boundaries are not world boundaries.** The red route moves in an infinite coordinate plane and is **visually clipped** to a centered square. No modulo Wrap.
4. **Editor uses the same route and grading model as Play.** Authoring can alter a chart draft, scored TEST evaluates a snapshot.
5. **Presentation is separate.** A global skin changes path/marker/HUD and border colors, not audio timestamps, route geometry or scoring.
6. **Keep the scope small.** New abstraction layers, separate render engines or framework migrations require a concrete product problem, not speculative future tasks.

## Code currently in the repository

| Area | Existing files / responsibility |
| --- | --- |
| Core spatial model | `src/core/route.*`, `route_view.*`: chart geometry, eight headings and sampled movement |
| Core scoring | `src/core/judgement.*`: one grade/event, timing windows, weighted accuracy, scorable-event completion |
| Core timing | `src/core/song_clock.*`: monotonic clock mapping and rate/pause behavior |
| Core charts | `src/core/chart_json.*`: strict chart v2 persistence and legacy v1 reads |
| Core editor state | `src/core/chart_editor.*`: note editing, groups, snap/tick projections and history |
| App session/audio | `src/app/game_session.*`, `chart_playback.*`, `demo_transport.*`, `local_music.*`: start/retry/pause, audio transport, player input |
| App rendering | `src/app/route_renderer.*`, `hit_flash.h`, `main.cpp`: route drawing, clipped square, results/HUD |
| Editor UI | `src/app/chart_editor_ui.*`, `chart_editor_actions.cpp`, `chart_editor_layout.h`: tabs, navigation, hitboxes, text input |
| Local files & library | `src/app/chart_file.*`, `chart_save.cpp`, `local_file_io.*`, `song_library.*`, `music_picker.*`: safe local import/save/select |
| Settings & skins | `src/app/game_settings.*`, `skin.*`, `skin_menu.*`, `skin_controls.*`, `menu_ui.*`: rebinds, appearance, screens |
| Native platform close | `src/app/native_close.*`: reversible Windows X/Alt+F4 handling and confirmation |
| Application shell | `src/app/main.cpp`: screen state, menu workflow, gameplay-session coordination |
| Tests & build | `tests/*`, `CMakeLists.txt`, `.github/workflows/windows_ci.yml`: headless model/integration regression checks and Windows x64 build |

`locus_core` contains the chart/route/timing/judgement core independent of raylib. `locus` is the Windows/raylib game executable. The A-07 sound probe is an **isolated historical diagnostic**, not the game executable.

## Cross-cutting behavioral contracts

- **Chart format:** new saves use v2 `timeMs` integer notes, `offsetMs` integer, independently chosen `snapDivision` and musical BPM. No save operation may inadvertently quantize or move existing audio timestamps.
- **Input:** four configured directional keys are shared by Play and editor ROUTE; the default is D/F/J/K, and diagonals use two adjacent directions. Arrow keys primarily navigate/edit the timeline.
- **World and view:** one square field-size parameter (default 8 logical cells), no editable start X/Y, screen-height centered gameplay square with a skin-adjustable outline. The left in-game sidebar is empty, the right holds grade counts/accuracy.
- **Scoring:** last judged scorable note ends Play; the final Miss uses its full late window, including a silent tail beyond audio end where necessary. Unscored empty/test-after-last sessions end on audio completion.
- **Persistence:** songs, charts, settings and skins reside in local user directories; relative audio references are checked to avoid escaping the chart folder. They are not remote uploads.
- **Exit:** Esc navigates Back except on Home, where it requests confirmation; Windows X and Alt+F4 also request approval before actually closing.

## Tests and limitations

CTest and headless CLI flags cover the core, chart editing, serialization, score accuracy, basic app state/layout assertions and dependency builds. **Green CI is not a rendered screenshot or a measured speaker/keyboard latency test.** Any remaining interactive UX issue belongs to the short [roadmap](../ROADMAP.md).

See [GDD](../GDD.md), [TIMING](TIMING.md), [UI_FLOW](UI_FLOW.md), [CHART_FORMAT](CHART_FORMAT.md), [VISUAL_SKINS](VISUAL_SKINS.md).
