#include "app/menu_controller.h"

#include "app/viewport.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace locus::app {


MenuEvents HandleMenuInput(const AppState& state, FrameInput& input,
                           const locus::app::SongLibrary& library,
                           locus::app::SongSelectUi& libraryUi,
                           locus::app::GameSettings& settings,
                           locus::app::SettingsUi& settingsUi,
                           locus::app::Skin& draftSkin,
                           locus::app::SkinUi& skinUi) {
    MenuEvents events;
    const bool mouseClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const auto transform = locus::app::ViewportTransform::Menu(
        GetScreenWidth(), GetScreenHeight());
    const bool validMouse = transform.scale > 0.0f;
    const Vector2 mouse = transform.ToVirtual(GetMousePosition());

    if (state.screen == Screen::Skin) {
        if (locus::app::SkinMenuEditingText(skinUi)) {
            // Enter and Escape belong to the field, not the menu navigation.
            // Clicking away attempts a commit; invalid numbers keep focus.
            input.backPressed = false;
            input.homePressed = false;
            input.requestedScreen.reset();
            if (mouseClick && validMouse) {
                if (!locus::app::FinishSkinTextInput(draftSkin, skinUi, true))
                    return events;
            } else {
                locus::app::HandleSkinTextInput(draftSkin, skinUi);
                return events;
            }
        }
        if (validMouse) {
            locus::app::UpdateSkinSliderDrag(
                mouse, IsMouseButtonDown(MOUSE_BUTTON_LEFT), draftSkin, skinUi);
        }
        const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (ctrl && IsKeyPressed(KEY_S)) {
            events.skinCommand = shift ? locus::app::SkinCommand::SaveAs
                                       : locus::app::SkinCommand::Save;
            return events;
        }
        if (ctrl && IsKeyPressed(KEY_O)) {
            events.skinCommand = locus::app::SkinCommand::Load;
            return events;
        }
    }
    if (state.screen == Screen::Settings && settingsUi.capturing >= 0) {
        // Esc cancels rebind, not the settings screen or application. Using
        // GetKeyPressed preserves arbitrary keys (WASD, arrows, numpad...)
        // rather than only character input from GetCharPressed().
        input.backPressed = false;
        input.homePressed = false;
        input.requestedScreen.reset();
        input.fullscreenPressed = false;
        if (IsKeyPressed(KEY_ESCAPE)) {
            settingsUi.capturing = -1;
            settingsUi.notice = "KEY BINDING CANCELED";
            settingsUi.noticeError = false;
        } else {
            const int key = GetKeyPressed();
            if (key != KEY_NULL) {
                std::string error;
                if (settings.Rebind(
                        static_cast<locus::app::GameAction>(settingsUi.capturing),
                        key, error)) {
                    settingsUi.notice = "KEY BINDING SAVED";
                    settingsUi.noticeError = false;
                    events.saveSettings = true;
                    settingsUi.capturing = -1;
                } else {
                    settingsUi.notice = error;
                    settingsUi.noticeError = true;
                }
            }
        }
        return events;
    }

    if (state.screen == Screen::SongSelect && 
        libraryUi.pendingDelete != LibraryDeletePending::None) {
        // Destructive confirmation is modal; Y/N/Esc cannot simultaneously
        // navigate to another screen or activate the current difficulty.
        input.startPressed = false;
        input.backPressed = false;
        input.homePressed = false;
        input.chooseChartPressed = false;
        input.requestedScreen.reset();
        if (IsKeyPressed(KEY_Y)) {
            events.trashSongSet =
                libraryUi.pendingDelete == LibraryDeletePending::SongSet;
            events.trashDifficulty =
                libraryUi.pendingDelete == LibraryDeletePending::Difficulty;
            libraryUi.pendingDelete = LibraryDeletePending::None;
        } else if (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE)) {
            libraryUi.pendingDelete = LibraryDeletePending::None;
        }
        return events;
    }
    if (state.screen == Screen::SongSelect) {
        if (IsKeyPressed(KEY_UP)) libraryUi.MoveSong(-1, library);
        if (IsKeyPressed(KEY_DOWN)) libraryUi.MoveSong(1, library);
        if (IsKeyPressed(KEY_LEFT)) libraryUi.MoveChart(-1, library);
        if (IsKeyPressed(KEY_RIGHT)) libraryUi.MoveChart(1, library);
        if (IsKeyPressed(KEY_F5)) events.refresh = true;
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
            libraryUi.MoveSong(wheel > 0.0f ? -1 : 1, library);
        if (input.chooseChartPressed) {
            events.import = true;
            events.importIntoSelected = IsKeyDown(KEY_LEFT_SHIFT) ||
                                        IsKeyDown(KEY_RIGHT_SHIFT);
        }
        if (input.startPressed) events.playSelected = true;
        if (IsKeyPressed(KEY_E)) events.editSelected = true;
        if (IsKeyPressed(KEY_N)) events.newDifficulty = true;
        if (IsKeyPressed(KEY_U)) events.restoreDeleted = true;
        const bool control = IsKeyDown(KEY_LEFT_CONTROL) ||
                             IsKeyDown(KEY_RIGHT_CONTROL);
        if (control && IsKeyPressed(KEY_D))
            events.duplicateDifficulty = true;
        if (IsKeyPressed(KEY_DELETE)) {
            libraryUi.pendingDelete =
                (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                ? LibraryDeletePending::SongSet
                : LibraryDeletePending::Difficulty;
            events.playSelected = false;
            events.editSelected = false;
            events.newDifficulty = false;
            events.duplicateDifficulty = false;
            return events;
        }
    }

    if (!mouseClick || !validMouse) return events;
    const auto hit = [&](Rectangle area) {
        return CheckCollisionPointRec(mouse, area);
    };

    if (state.screen == Screen::Home) {
        for (int i = 0; i < 5; ++i) {
            if (!hit(locus::app::HomeMenuButton(i))) continue;
            switch (i) {
                case 0: input.requestedScreen = Screen::SongSelect; break;
                case 1: input.requestedScreen = Screen::Editor; break;
                case 2: input.requestedScreen = Screen::Skin; break;
                case 3: input.requestedScreen = Screen::Settings; break;
                case 4: input.closeRequested = true; break;
            }
            return events;
        }
    } else if (state.screen == Screen::SongSelect) {
        if (hit(locus::app::MenuBackButton())) input.backPressed = true;
        if (hit(locus::app::LibraryImportButton())) events.import = true;
        if (hit(locus::app::LibraryRefreshButton())) events.refresh = true;
        if (hit(locus::app::LibraryPlayButton())) events.playSelected = true;
        if (hit(locus::app::LibraryEditButton())) events.editSelected = true;
        if (hit(locus::app::LibraryNewDifficultyButton()))
            events.newDifficulty = true;
        for (int i = 0; i < 6; ++i) {
            if (hit(locus::app::LibrarySongRow(i)))
                libraryUi.SelectSong(libraryUi.firstVisibleSong + i, library);
        }
        for (int i = 0; i < 5; ++i) {
            if (hit(locus::app::LibraryChartRow(i)))
                libraryUi.SelectChart(libraryUi.firstVisibleChart + i, library);
        }
    } else if (state.screen == Screen::Skin) {
        if (hit(locus::app::MenuBackButton())) input.backPressed = true;
        else events.skinCommand = locus::app::SkinMenuClick(mouse, draftSkin, skinUi);
    } else if (state.screen == Screen::Settings) {
        if (hit(locus::app::MenuBackButton())) {
            input.backPressed = true;
            return events;
        }
        for (int i = 0; i < locus::app::kGameActionCount; ++i) {
            if (!hit(locus::app::SettingsBindingButton(i))) continue;
            settingsUi.capturing = i;
            settingsUi.notice = "PRESS A NEW KEY  /  ESC CANCELS";
            settingsUi.noticeError = false;
            return events;
        }
        // Pages use the same hitboxes as the draw code and share persistent
        // bindings on the right. A page switch does not edit settings.
        if (hit(SettingsGameTab())) {
            settingsUi.page = SettingsPage::Game;
            return events;
        }
        if (hit(SettingsDisplayTab())) {
            settingsUi.page = SettingsPage::Display;
            return events;
        }

        if (settingsUi.page == SettingsPage::Game) {
            const auto changeVolume = [&](int delta) {
                const int current = static_cast<int>(
                    std::lround(settings.volume * 10.0f));
                settings.volume = std::clamp(current + delta, 0, 10) / 10.0f;
                events.saveSettings = true;
            };
            if (hit(SettingsVolumeMinus())) changeVolume(-1);
            else if (hit(SettingsVolumePlus())) changeVolume(1);
            else if (hit(SettingsFlashButton())) {
                settings.hitFlash = !settings.hitFlash;
                events.saveSettings = true;
            } else if (hit(SettingsChordMinus())) {
                settings.chordWindowMs = std::max(40, settings.chordWindowMs - 10);
                events.saveSettings = true;
            } else if (hit(SettingsChordPlus())) {
                settings.chordWindowMs = std::min(160, settings.chordWindowMs + 10);
                events.saveSettings = true;
            }
        } else {
            if (hit(SettingsVsyncButton())) {
                settings.vsync = !settings.vsync;
                events.applyDisplaySettings = true;
                events.saveSettings = true;
            } else if (hit(SettingsFpsMinus()) || hit(SettingsFpsPlus())) {
                const int previous = settings.fpsLimit;
                settings.StepFpsLimit(hit(SettingsFpsPlus()) ? 1 : -1);
                if (settings.fpsLimit != previous) {
                    events.applyDisplaySettings = true;
                    events.saveSettings = true;
                }
            } else if (hit(SettingsShowFpsButton())) {
                settings.showFps = !settings.showFps;
                events.saveSettings = true;
            } else if (hit(SettingsFullscreenButton())) {
                settings.fullscreen = !settings.fullscreen;
                events.toggleFullscreen = true;
                events.saveSettings = true;
            }
        }
    }
    return events;
}

// 3. Render: display the selected state; never choose or modify it here.

} // namespace locus::app
