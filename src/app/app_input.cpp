#include "app/app_input.h"

#include "app/native_close.h"
#include "app/app_quit_dialog_layout.h"
#include "app/play_modal_layout.h"
#include "app/screen_action_buttons.h"
#include "app/chart_editor_layout.h"
#include "raylib.h"

namespace locus::app {

// 1. Input: raylib already pumps native events at the end of each drawn frame.
// F1-F4 are diagnostic shortcuts ONLY when --debug-screens is explicitly enabled.
FrameInput PollInput(bool debugScreens, Screen currentScreen,
                     const locus::app::GameSettings& settings,
                     double eventTimeSeconds) {
    FrameInput input;
    const auto shortcut = [&](ShortcutAction action) {
        return ShortcutPressed(settings, action);
    };
    const bool back = shortcut(ShortcutAction::Back);
    const bool gameplayKeys = currentScreen == Screen::Play ||
                              currentScreen == Screen::Results;
    input.closeRequested = ConsumeNativeCloseRequest() ||
                           (back && currentScreen == Screen::Home);
    // The pause key (default ESC) NEVER performs navigation from PLAY.
    // The Home/Leave action uses a separate confirmation before exiting.
    input.backPressed = back && currentScreen != Screen::Home &&
                        currentScreen != Screen::Play;
    if (debugScreens) {
        if (IsKeyPressed(KEY_F1)) input.requestedScreen = Screen::Home;
        else if (IsKeyPressed(KEY_F2)) input.requestedScreen = Screen::Play;
        else if (IsKeyPressed(KEY_F3)) input.requestedScreen = Screen::Editor;
        else if (IsKeyPressed(KEY_F4)) input.requestedScreen = Screen::Results;
    }
    input.startPressed = IsKeyPressed(KEY_ENTER);
    input.pausePressed = gameplayKeys
        ? IsKeyPressed(settings.Key(GameAction::Pause))
        : currentScreen == Screen::Listen &&
            shortcut(ShortcutAction::ListenPause);
    input.restartPressed = gameplayKeys
        ? IsKeyPressed(settings.Key(GameAction::Retry))
        : currentScreen == Screen::Listen &&
            shortcut(ShortcutAction::ListenReplay);
    input.homePressed = (gameplayKeys || currentScreen == Screen::Listen) &&
                        IsKeyPressed(settings.Key(GameAction::Home));
    input.fullscreenPressed = shortcut(ShortcutAction::Fullscreen);
    input.chooseMusicPressed = shortcut(ShortcutAction::ChooseMusic);
    input.chooseChartPressed = shortcut(ShortcutAction::ChooseChart);
    input.openEditorPressed = shortcut(ShortcutAction::OpenEditor);
    input.openPlayPressed = shortcut(ShortcutAction::OpenLibrary);
    input.openSkinPressed = shortcut(ShortcutAction::OpenSkin);
    input.openSettingsPressed = shortcut(ShortcutAction::OpenSettings);
    input.editorSave = shortcut(ShortcutAction::EditorSave);
    input.editorSaveAs = shortcut(ShortcutAction::EditorSaveAs);
    input.editorOpen = shortcut(ShortcutAction::EditorOpen);
    input.editorNew = shortcut(ShortcutAction::EditorNew);
    input.editorTest = shortcut(ShortcutAction::EditorTest);
    input.practiceSpeedIndex = -1;
    for (int i = 0; i < 5; ++i) {
        if (IsKeyPressed(settings.Key(static_cast<GameAction>(
                static_cast<int>(GameAction::Practice100) + i)))) {
            input.practiceSpeedIndex = i;
            break;
        }
    }
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (currentScreen == Screen::Play && !ctrl && !shift) {
        const int faster = settings.Key(GameAction::SpeedUp);
        const int slower = settings.Key(GameAction::SpeedDown);
        const int more = settings.Key(GameAction::PreviewUp);
        const int less = settings.Key(GameAction::PreviewDown);
        input.baseSpeedDelta = static_cast<int>(IsKeyPressed(faster)) -
                               static_cast<int>(IsKeyPressed(slower));
        input.baseSpeedHeld = static_cast<int>(IsKeyDown(faster)) -
                              static_cast<int>(IsKeyDown(slower));
        input.previewTimeDelta = static_cast<int>(IsKeyPressed(more)) -
                                 static_cast<int>(IsKeyPressed(less));
        input.previewTimeHeld = static_cast<int>(IsKeyDown(more)) -
                                static_cast<int>(IsKeyDown(less));
    }
    // IsKeyPressed() captures fresh key-downs only; held keys never re-score.
    // Use the same steady clock origin as DemoTransport, not frame duration.
    // Each physical arrow has its own captured key-down time, including
    // the two independent components of a diagonal turn.
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
        input.editor = locus::app::PollEditorControls(settings);
    return input;
}

FrameInput PollFrameInput(bool debugScreens, const AppState& state,
                          const GameSettings& settings,
                          double eventTimeSeconds, bool playPaused) {
    if (state.quitConfirmation) {
        // Repeated native X presses cannot bypass an already-open modal.
        ConsumeNativeCloseRequest();
        FrameInput input;
        input.quitConfirmed = IsKeyPressed(KEY_Y);
        input.quitCanceled = IsKeyPressed(KEY_N) ||
                             IsKeyPressed(KEY_ESCAPE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const QuitDialogChoice choice = QuitDialogPointerChoice(
                state.screen, GetMousePosition(),
                GetScreenWidth(), GetScreenHeight());
            if (choice == QuitDialogChoice::Confirm)
                input.quitConfirmed = true;
            else if (choice == QuitDialogChoice::Cancel)
                input.quitCanceled = true;
        }
        return input;
    }
    if (state.levelExitConfirmation) {
        ConsumeNativeCloseRequest(); // Modal owns all gameplay and input.
        FrameInput input;
        input.levelExitConfirmed = IsKeyPressed(KEY_Y);
        input.levelExitCanceled = IsKeyPressed(KEY_N) ||
                                  IsKeyPressed(KEY_ESCAPE);
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const Vector2 mouse = GetMousePosition();
            const int width = GetScreenWidth(), height = GetScreenHeight();
            input.levelExitConfirmed |= PlayButtonHit(
                kLeaveLevelConfirmButton, mouse, width, height);
            input.levelExitCanceled |= PlayButtonHit(
                kLeaveLevelCancelButton, mouse, width, height);
        }
        return input;
    }
    FrameInput input = PollInput(debugScreens, state.screen,
                                 settings, eventTimeSeconds);
    if (state.screen == Screen::Play && playPaused &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = GetMousePosition();
        const int width = GetScreenWidth(), height = GetScreenHeight();
        if (PlayButtonHit(kPlayResumeButton, mouse, width, height))
            input.pausePressed = true;
        else if (PlayButtonHit(kPlayRetryButton, mouse, width, height))
            input.restartPressed = true;
        else if (PlayButtonHit(kPlayLeaveButton, mouse, width, height))
            input.leaveRequested = true;
    }
    if (state.screen == Screen::Editor && input.editor.mouseClick) {
        const Vector2 mouse = input.editor.mouse;
        const auto hit = [&](Rectangle r) {
            return CheckCollisionPointRec(mouse, r);
        };
        if (hit(detail::kEditorBack)) input.backPressed = true;
        else if (hit(detail::kEditorFileSave)) input.editorSave = true;
        else if (hit(detail::kEditorFileSaveAs)) input.editorSaveAs = true;
        else if (hit(detail::kEditorFileOpen)) input.editorOpen = true;
        else if (hit(detail::kEditorFileNew)) input.editorNew = true;
    }
    if ((state.screen == Screen::Listen || state.screen == Screen::Results) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 pointer = GetMousePosition();
        const int width = GetScreenWidth(), height = GetScreenHeight();
        const auto hit = [&](Rectangle r) {
            return MenuActionHit(r, pointer, width, height);
        };
        if (state.screen == Screen::Listen) {
            if (hit(kListenPauseButton)) input.pausePressed = true;
            else if (hit(kListenReplayButton)) input.restartPressed = true;
            else if (hit(kListenChooseButton)) input.chooseMusicPressed = true;
            else if (hit(kListenHomeButton)) input.homePressed = true;
        } else {
            if (hit(kResultsReplayButton)) input.restartPressed = true;
            else if (hit(kResultsLibraryButton)) input.homePressed = true;
            else if (hit(kResultsEditorButton)) input.openEditorPressed = true;
        }
    }
    return input;
}

} // namespace locus::app
