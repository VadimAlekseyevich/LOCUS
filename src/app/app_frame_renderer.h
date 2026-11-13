#pragma once

#include "app/screen_flow.h"
#include "app/run_results.h"
#include "app/gameplay_ui.h"
#include "app/image_asset_texture.h"
#include "app/chart_editor_ui.h"
#include "app/editor_discard_guard.h"
#include "app/editor_set_controller.h"
#include "app/editor_comparison.h"
#include "app/local_music.h"
#include "app/chart_playback.h"
#include "app/song_library.h"
#include "app/menu_ui.h"
#include "app/game_settings.h"
#include "app/skin_menu.h"
#include "core/route.h"
#include "core/chart_editor.h"
#include "raylib.h"

#include <string>
#include <filesystem>

namespace locus::app {

class NativeMenuUi;

// Application UI font, owned only when loaded from the user's OS.
struct UiFont {
    Font value{};
    bool owned = false;
};
UiFont LoadUiFont();

// One editor transport state used by all comparison panes, song-time
// readout and beat ruler. Prepared before entering the renderer.
struct EditorRenderClock {
    double songSeconds = 0.0;
    double tick = 0.0;
    double practiceRate = 1.0;
    bool playing = false;
    bool paused = false;
};

// Purely presentational frame: no app state, song library or editor mutation.
void RenderFrame(const AppState& state, bool debugScreens, Font font,
                 const locus::core::Chart& demo,
                 const GameplayView& gameplay,
                 const locus::app::LocalMusic& localSong,
                 const locus::core::ChartEditor& editor,
                 const locus::app::EditorUiState& editorUi,
                 const EditorRenderClock& editorClock,
                 const std::string& editorStatus, EditorPending editorPending,
                 const locus::app::EditorSetView& editorSetView,
                 const locus::app::EditorComparisonFrame& comparisons,
                 const locus::app::EditorSetRequest* editorSetPending,
                 const std::filesystem::path& editorClosePending,
                 bool anyEditorDirty,
                 const std::string& musicError, double now,
                  const locus::app::SongLibrary& library,
                  const HomeMenuUi& homeUi,
                  const HomeBrandAssets& homeBrand,
                  const locus::app::SongSelectUi& libraryUi,
                  const locus::app::GameSettings& settings,
                  const locus::app::SettingsUi& settingsUi,
                  const locus::app::Skin& draftSkin,
                  const locus::app::SkinUi& skinUi,
                  const std::string& menuStatus,
                  float screenFadeOpacity = 0.0f,
                  const ImageAssetTexture* background = nullptr,
                  const RunResult* displayedResult = nullptr,
                  bool historicalResult = false,
                  NativeMenuUi* nativeMenu = nullptr);

} // namespace locus::app
