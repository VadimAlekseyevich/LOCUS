#pragma once

#include <optional>

namespace locus::app {

constexpr int kSmokeFrames = 60;

enum class Screen {
    Home, Play, Editor, Results, Listen, SongSelect, Skin, Settings
};

struct AppState {
    bool running = true;
    bool quitConfirmation = false;
    bool levelExitConfirmation = false;
    int frames = 0;
    Screen screen = Screen::Home;
};

// The navigation policy knows nothing about raylib, file dialogs, audio or
// the chart editor. Those systems only supply the actions for each frame.
struct NavigationInput {
    bool closeRequested = false;
    std::optional<Screen> requestedScreen;
    bool quitConfirmed = false;
    bool quitCanceled = false;
    bool startPressed = false;
    bool restartPressed = false;
    bool homePressed = false;
    bool backPressed = false;
    bool openEditorPressed = false;
    bool openSkinPressed = false;
    bool openSettingsPressed = false;
    bool openPlayPressed = false;
    bool chooseChartPressed = false;
    bool leaveRequested = false;
    bool levelExitConfirmed = false;
    bool levelExitCanceled = false;
};

const char* ScreenName(Screen screen) noexcept;
void UpdateScreenFlow(AppState& state, const NavigationInput& input,
                      bool smokeWindow) noexcept;

} // namespace locus::app
