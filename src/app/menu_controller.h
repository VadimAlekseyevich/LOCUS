#pragma once

#include "app/app_input.h"
#include "app/game_settings.h"
#include "app/menu_ui.h"
#include "app/skin_menu.h"
#include "app/song_library.h"

namespace locus::app {

struct MenuEvents {
    bool import = false;
    bool importIntoSelected = false;
    bool exportSelected = false;
    bool attachBackground = false;
    bool removeBackground = false;
    bool refresh = false;
    bool playSelected = false;
    bool editSelected = false;
    bool newDifficulty = false;
    bool duplicateDifficulty = false;
    bool trashDifficulty = false;
    bool trashSongSet = false;
    bool restoreDeleted = false;
    bool saveSettings = false;
    bool calibrationStart = false;
    bool calibrationStop = false;
    bool calibrationTap = false;
    int previewStartDeltaMs = 0; // Song Set's menu-only listening origin.
    bool toggleFullscreen = false;
    bool applyDisplaySettings = false;
    locus::app::SkinCommand skinCommand = locus::app::SkinCommand::None;
};


MenuEvents HandleMenuInput(const AppState& state, FrameInput& input,
                            const SongLibrary& library, SongSelectUi& libraryUi,
                            GameSettings& settings, SettingsUi& settingsUi,
                            Skin& draftSkin, SkinUi& skinUi);

} // namespace locus::app
