# LOCUS — UI architecture

## Runtime boundaries

| Screen | Native RmlUi responsibility | raylib responsibility |
| --- | --- | --- |
| HOME | Responsive menu and buttons | Brand texture/background |
| PLAY menu | Search, virtualized Song Set/chart tree, history, footer commands and MODS overlay | Artwork/background and fallback screen |
| EDIT | Song Setup (metadata, audio/tempo table, difficulty list), Chart Editor tools, direction pad, transport, comparison picker | Route preview, upper note ruler, whole-song overview and fallback authoring UI |
| SETTINGS | Responsive settings, key bindings and calibration controls | Calibration visuals where needed and fallback |
| SKINS | Skin profiles, colors/settings fields and actions | Hue/SV graphics and live gameplay preview, fallback |
| RESULTS | Score summary, actions, virtualized saved history | Artwork/background and fallback |
| Scored PLAY / TEST | None | Entire gameplay, camera, route, input timing, HUD and spectator timeline |

RML and RCSS live in `assets/ui/rml`. `common.rcss` is the component styling
foundation. `design/ui/v2` is the Nightbloom visual reference, not a runtime
dependency. `NativeMenuUi` owns the RmlUi context and documents; page-specific
NativePlayUi, NativeEditorUi, NativeSettingsUi, NativeSkinUi and NativeResultsUi
present state and emit semantic commands. SongLibrary, ChartEditor, editor
workspace, GameSettings and Skin remain authoritative.

## Ownership and lifecycle rules

1. Raylib polls platform input once per frame; RmlUi receives the relevant
   events and emits semantic actions. Native controls must not synthesize
   legacy-coordinate clicks. Canvas input uses the published layout snapshot;
   inactive fallback controls must not receive input.
2. Native text fields commit through their established focus/blur or explicit
   confirmation contracts. Esc cancels the current edit before navigating
   back. Exclusive rebinding/calibration takes precedence over menu commands.
3. Modal confirmation and comparison overlays own their input. Outside
   clicks cannot activate an underlying editor note or button. Hidden/recycled
   elements retire their focus before destruction.
4. Native layout uses the real context and density-independent pixels, not
   a fixed 960×540 letterbox. Canvas coordinates map through the measured
   editor layout; resize/DPI layout is refreshed before hit-testing.
5. Native screens omit persistent navigation headers. Esc returns from Chart Editor to Song Setup and otherwise navigates back subject to text focus and guards. Editor layout uses measured snapshots and maximum available equal square previews. SKINS owns rename on the first profile tab, and every skin tab shares one preview rectangle.
6. Generated flex buttons use a `button-label` span. `UpdateRmlText`
   mutates existing text nodes and escapes user-controlled content when a
   wrapper must be restored. Stable controls are not rebuilt on text changes.
7. Published catalog revision and cached frame views separate structural
   changes from selection. PLAY, RESULTS, SET and COMPARE materialize only
   their visible rows plus overscan. Commands retain stable IDs/paths or
   verified model indices, never viewport-local row numbers.
8. Gameplay and editor route previews consume exact-time prepared geometry.
   Menu animations and UI navigation do not change scored input time.
   Skin previews may draw independently; scored gameplay never uses RmlUi.

## Graphics and resources

The RmlUi adapter uses raylib's white texture for untextured geometry and
premultiplied-alpha blending. Song artwork is shown only when the selected
Song Set supplies a valid image. Required RML, RCSS and brand resources are
declared in `cmake/ui_assets_manifest.cmake` and staged beside the executable.
Native presenters are optional targets; disabling LOCUS_ENABLE_RMLUI keeps
the fallback UI build. The portable core configuration does not fetch either
graphics library.

## Validation and remaining work

Windows CTest exercises production RML documents, viewport layout, bindings,
native event handling, virtual lists and editor workflows. CI checks the
standalone ZIP from a Unicode path outside the checkout. Headless tests do
not establish GPU rasterization quality, physical key latency, audio sync
or actual usability. Manual testing remains necessary on 16:9, 16:10,
ultrawide and narrow displays, including scrolling, keyboard focus and
comparison panes. See [the audit plan](REFACTORING_PLAN.md) for acceptance
criteria. These UI boundaries do not change chart, skin, results or Song Set
formats.
