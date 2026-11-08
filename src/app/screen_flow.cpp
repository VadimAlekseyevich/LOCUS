#include "app/screen_flow.h"

namespace locus::app {

const char* ScreenName(Screen screen) noexcept {
    switch (screen) {
        case Screen::Home: return "Home";
        case Screen::Play: return "Play";
        case Screen::Editor: return "Editor";
        case Screen::Results: return "Results";
        case Screen::Listen: return "Listen";
        case Screen::SongSelect: return "Song Library";
        case Screen::Skin: return "Skin";
        case Screen::Settings: return "Settings";
    }
    return "Unknown";
}

void UpdateScreenFlow(AppState& state, const NavigationInput& input,
                      bool smokeWindow) noexcept {
    if (smokeWindow && state.frames >= kSmokeFrames) {
        state.running = false;
        return;
    }
    if (state.quitConfirmation) {
        if (input.quitConfirmed) {
            state.running = false;
            return;
        }
        if (input.quitCanceled) state.quitConfirmation = false;
        ++state.frames;
        return;
    }
    if (input.closeRequested) {
        state.quitConfirmation = true;
        ++state.frames;
        return;
    }
    if (input.requestedScreen) {
        state.screen = *input.requestedScreen;
    } else if ((input.homePressed || input.backPressed) &&
               state.screen != Screen::Home) {
        // Return to the selected Song Set's menu listening position.
        state.screen = (state.screen == Screen::Play ||
                        state.screen == Screen::Results)
            ? Screen::SongSelect : Screen::Home;
    } else if (state.screen == Screen::Home) {
        if (input.openEditorPressed) state.screen = Screen::Editor;
        else if (input.openSkinPressed) state.screen = Screen::Skin;
        else if (input.openSettingsPressed) state.screen = Screen::Settings;
        else if (input.startPressed || input.openPlayPressed ||
                 input.chooseChartPressed) state.screen = Screen::SongSelect;
    } else if (state.screen == Screen::Results &&
               (input.startPressed || input.restartPressed)) {
        state.screen = Screen::Play;
    }
    ++state.frames;
}

} // namespace locus::app
