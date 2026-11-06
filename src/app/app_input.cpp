#include "app/app_input.h"

#include "app/native_close.h"
#include "raylib.h"

namespace locus::app {

// 1. Input: raylib already pumps native events at the end of each drawn frame.
// F1-F4 are diagnostic shortcuts ONLY when --debug-screens is explicitly enabled.
FrameInput PollInput(bool debugScreens, Screen currentScreen,
                     const locus::app::GameSettings& settings,
                     double eventTimeSeconds) {
    FrameInput input;
    const bool escape = IsKeyPressed(KEY_ESCAPE);
    // Esc always goes BACK outside Home, including ROUTE editor.
    // Only Esc on Home or the window X requests app termination.
    input.closeRequested = locus::app::ConsumeNativeCloseRequest() ||
                           (escape && currentScreen == Screen::Home);
    input.backPressed = escape && currentScreen != Screen::Home;
    if (debugScreens) {
        if (IsKeyPressed(KEY_F1)) input.requestedScreen = Screen::Home;
        else if (IsKeyPressed(KEY_F2)) input.requestedScreen = Screen::Play;
        else if (IsKeyPressed(KEY_F3)) input.requestedScreen = Screen::Editor;
        else if (IsKeyPressed(KEY_F4)) input.requestedScreen = Screen::Results;
    }
    input.startPressed = IsKeyPressed(KEY_ENTER);
    const bool gameplayKeys = currentScreen == Screen::Play ||
                              currentScreen == Screen::Results;
    input.pausePressed = IsKeyPressed(gameplayKeys
        ? settings.Key(locus::app::GameAction::Pause) : KEY_SPACE);
    input.restartPressed = IsKeyPressed(gameplayKeys
        ? settings.Key(locus::app::GameAction::Retry) : KEY_R);
    input.homePressed = IsKeyPressed(gameplayKeys
        ? settings.Key(locus::app::GameAction::Home) : KEY_H) &&
        !(currentScreen == Screen::Editor &&
          locus::app::IsGameDirectionKey(settings, KEY_H));
    input.fullscreenPressed = IsKeyPressed(KEY_F11);
    input.chooseMusicPressed = IsKeyPressed(KEY_O) &&
        !(currentScreen == Screen::Editor &&
          locus::app::IsGameDirectionKey(settings, KEY_O));
    input.chooseChartPressed = IsKeyPressed(KEY_C);
    input.openEditorPressed = IsKeyPressed(KEY_E);
    input.openPlayPressed = IsKeyPressed(KEY_P);
    input.openSkinPressed = IsKeyPressed(KEY_K);
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    input.openSettingsPressed = !control && IsKeyPressed(KEY_S);
    input.editorSave = control && IsKeyPressed(KEY_S) && !shift;
    input.editorSaveAs = control && IsKeyPressed(KEY_S) && shift;
    input.editorOpen = control && IsKeyPressed(KEY_O);
    input.editorNew = control && IsKeyPressed(KEY_N);
    input.editorTest = IsKeyPressed(KEY_T) &&
        !(currentScreen == Screen::Editor &&
          locus::app::IsGameDirectionKey(settings, KEY_T));
    input.practiceSpeedIndex = IsKeyPressed(KEY_ONE) ? 0
                              : IsKeyPressed(KEY_TWO) ? 1
                              : IsKeyPressed(KEY_THREE) ? 2
                              : IsKeyPressed(KEY_FOUR) ? 3
                              : IsKeyPressed(KEY_FIVE) ? 4 : -1;
    if (currentScreen == Screen::Play && !control && !shift) {
        // Gameplay bindings take priority over speed hotkeys.
        const auto available = [&](int key) {
            for (int bound : settings.keys)
                if (key == bound) return false;
            return true;
        };
        const bool decrease =
            (available(KEY_LEFT_BRACKET) && IsKeyPressed(KEY_LEFT_BRACKET)) ||
            (available(KEY_KP_SUBTRACT) && IsKeyPressed(KEY_KP_SUBTRACT));
        const bool increase =
            (available(KEY_RIGHT_BRACKET) && IsKeyPressed(KEY_RIGHT_BRACKET)) ||
            (available(KEY_KP_ADD) && IsKeyPressed(KEY_KP_ADD));
        input.baseSpeedDelta = static_cast<int>(increase) -
                               static_cast<int>(decrease);
    }
    // IsKeyPressed() captures fresh key-downs only; held keys never re-score.
    // Use the same steady clock origin as DemoTransport, not frame duration.
    // Absolute direction input; a diagonal is a two-key stroke.
    // The original four enum indices still map to Up, Right, Down, Left.
    for (int i = 0; i < 4; ++i) {
        const int key = settings.keys[static_cast<std::size_t>(i)];
        input.arrowPressed[i] = IsKeyPressed(key);
        input.arrowHeld[i] = IsKeyDown(key);
        if (input.arrowPressed[i]) input.arrowCapturedAt[i] = eventTimeSeconds;
    }
    // The chart editor consumes the raylib UTF-8 character queue.
    // Never drain it on SKIN: its numeric/name text inputs own that queue.
    if (currentScreen == Screen::Editor)
        input.editor = locus::app::PollEditorControls(settings, input.arrowHeld);
    return input;
}

} // namespace locus::app
