#pragma once

#include "app/chart_editor_ui.h"
#include "app/game_settings.h"
#include "app/screen_flow.h"

#include <array>
#include <optional>

namespace locus::app {

struct FrameInput {
    bool closeRequested = false;
    std::optional<Screen> requestedScreen;
    bool quitConfirmed = false;
    bool quitCanceled = false;
    bool startPressed = false;
    bool pausePressed = false;
    bool restartPressed = false;
    bool homePressed = false;
    bool fullscreenPressed = false;
    bool chooseMusicPressed = false;
    bool chooseChartPressed = false;
    bool openEditorPressed = false;
    bool openPlayPressed = false;
    bool openSkinPressed = false;
    bool openSettingsPressed = false;
    bool backPressed = false;
    bool editorSave = false;
    bool editorSaveAs = false;
    bool editorOpen = false;
    bool editorNew = false;
    bool editorTest = false;
    int practiceSpeedIndex = -1;
    int baseSpeedDelta = 0; // [ / ] and numpad - / + while playing
    std::array<bool, 4> arrowPressed{};
    std::array<double, 4> arrowCapturedAt{};
    std::array<bool, 4> arrowHeld{};
    locus::app::EditorControls editor;
};

FrameInput PollInput(bool debugScreens, Screen currentScreen,
                     const GameSettings& settings, double eventTimeSeconds);

// Native quit-modal key/mouse priority and the Editor BACK hitbox belong
// to input collection, not to the application lifecycle/composition root.
FrameInput PollFrameInput(bool debugScreens, const AppState& state,
                          const GameSettings& settings,
                          double eventTimeSeconds);

} // namespace locus::app
