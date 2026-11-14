# LOCUS — UI architecture

## Runtime boundaries

- **RmlUi:** HOME, song/chart selection (PLAY menu), Settings, and the common
  Editor chrome (BACK, SET/ROUTE/AUDIO tabs and transport controls).
- **raylib:** scored gameplay, SKINS, Results, editor note ruler, whole-song
  overview, chart/route previews, SET and AUDIO authoring views, native
  confirmation dialogs. Gameplay never runs through RmlUi.
- RML and RCSS are stored in `assets/ui/rml`. `common.rcss` is the
  component styling foundation. `design/ui/v2` is the approved Nightbloom
  visual reference, not a runtime dependency.
- `NativeMenuUi` owns the RmlUi context and documents.
  `NativeSettingsUi` and `NativeEditorUi` are page-specific presenters.
  `SongLibrary`, `SongSelectUi`, `GameSettings` and
  `EditorUiState` remain the sources of data and interaction state.

## Ownership and lifecycle rules

1. Every frame is polled through raylib once. RmlUi converts pointer and
   keyboard input into semantic actions. It **must not** synthesize a click
   in a legacy coordinate-based UI. When `EditorControls::nativeChrome` is
   true, the old editor header, rate, time and transport hitboxes are ignored.
   The raylib authoring compass, previews and tempo map still work.
2. Exclusive text editing, keyboard rebinding and offset calibration belong
   to their existing domain input handlers. RmlUi must not consume their
   character queue or trigger global shortcuts while they are active.
3. Modal confirmation has priority over all menu pages. A native editor
   rate popup consumes the complete outside press/release gesture when it is
   dismissed; it cannot activate the underlying note/tempo editor.
4. On page activation the old document loses keyboard focus. Resize and DPI
   changes cause RmlUi layout to be updated **before** pointer hit-testing.
   Editor chrome uses exactly the raylib `ViewportTransform::Menu`
   letterbox transform.
5. RmlUi flex buttons need explicit `<span class="button-label">` children.
   All dynamic captions use `UpdateRmlText` to update existing text nodes
   in-place rather than replacing entire subtrees. If a label node is absent,
   the helper restores the wrapper and escapes user text.
6. `SongLibrary::Revision()` changes after a successful scan or metadata
   mutation. The song browser keys its DOM rebuild to that revision and
   selection/search, rather than scanning all chart metadata every frame.
   Summary notice updates do not rebuild the song tree.

## Graphics and resources

- The RmlUi adapter uses raylib's default white texture for untextured
  geometry, premultiplied-alpha blending and a bundled PNG wordmark drawn at
  its RmlUi layout rectangle.
- Library artwork appears only when the selected Song Set provides a
  valid background image. No synthetic song background is generated.
- RmlUi renders in physical screen pixels after raylib's 960×540 virtual
  editor camera is closed. Scored gameplay keeps its independent camera,
  input timing and rendering path.

## Validation and current limitations

Windows Release and CTest cover the headless menu layout, flexible button
labels, generated binding rows, editor chrome ownership, viewport alignment,
song library changes and gameplay rules. CI also verifies bundled RML, RCSS
and the wordmark. **Headless CTest cannot confirm actual GPU rendering.**
Manual HOME, PLAY, Settings and Editor checks are still required at 16:9,
16:10 and ultrawide resolutions before declaring visual parity.

The remaining authoring views in SET, ROUTE and AUDIO are intentionally
raylib-owned until their migration can be verified without losing features.
There is no chart, skin, results or Song Set format change in this UI work.
