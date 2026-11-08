#include "core/route.h"
#include "core/route_view.h"
#include "core/judgement.h"
#include "core/chart_json.h"
#include "app/demo_transport.h"
#include "app/audio_device.h"
#include "app/local_music.h"
#include "app/chart_file.h"
#include "app/chart_playback.h"
#include "app/music_picker.h"
#include "app/chart_editor_ui.h"
#include "app/app_input.h"
#include "app/app_runtime_policy.h"
#include "app/app_smoke_checks.h"
#include "app/app_frame_renderer.h"
#include "app/menu_controller.h"
#include "app/chart_editor_layout.h"
#include "app/editor_session.h"
#include "app/editor_file_actions.h"
#include "app/editor_set_controller.h"
#include "app/listen_session.h"
#include "app/skin_profile_actions.h"
#include "app/visual_style.h"
#include "app/route_renderer.h"
#include "app/game_session.h"
#include "app/gameplay_ui.h"
#include "app/song_library.h"
#include "app/library_playback.h"
#include "app/library_browser.h"
#include "app/game_settings.h"
#include "app/skin.h"
#include "app/skin_menu.h"
#include "app/menu_ui.h"
#include "app/native_close.h"
#include "app/screen_flow.h"
#include "app/session_transition.h"
#include "app/quit_media.h"
#include "app/editor_discard_guard.h"
#include "app/viewport.h"

#include "raylib.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Production screens share one window and one input/update/render loop.
namespace {
using locus::app::AppState;
using locus::app::FrameInput;
using locus::app::PollInput;
using locus::app::MenuEvents;
using locus::app::HandleMenuInput;
using locus::app::Screen;
using locus::app::ScreenName;
using locus::app::EditorPending;
using locus::app::kSmokeFrames;
using locus::app::UpdateState;
using locus::app::ChartReadyForResults;
using locus::app::FinishSession;
using locus::app::UiFont;
using locus::app::LoadUiFont;
using locus::app::RenderFrame;
using locus::app::GameplayView;
using locus::app::GameplayCamera;
using locus::app::GameplayViewportScale;
using locus::app::GameplayRightSidebarWidth;
using locus::app::kGameplayWorldViewport;
using locus::app::DrawPlayGrid;
using locus::app::DrawResultsScreen;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;

double MonotonicSeconds() {
    using Clock = std::chrono::steady_clock;
    static const auto epoch = Clock::now();
    return std::chrono::duration<double>(Clock::now() - epoch).count();
}

bool HasArgument(int argc, char** argv, const char* argument) {
    return argc == 2 && std::strcmp(argv[1], argument) == 0;
}



} // namespace

int main(int argc, char** argv) {
    if (HasArgument(argc, argv, "--loop-check")) {
        if (!locus::app::CheckLoopState()) {
            std::cerr << "LOOP_STATE_FAIL\n";
            return 1;
        }
        std::cout << "LOOP_STATE_PASS frames=" << kSmokeFrames << '\n';
        return 0;
    }
    if (HasArgument(argc, argv, "--screen-check")) {
        if (!locus::app::CheckScreenTransitions()) {
            std::cerr << "SCREEN_STATE_FAIL\n";
            return 1;
        }
        std::cout << "SCREEN_STATE_PASS transitions=7\n";
        return 0;
    }
    if (HasArgument(argc, argv, "--session-check")) {
        if (!locus::app::CheckSessionFlow()) {
            std::cerr << "SESSION_FLOW_FAIL\n";
            return 1;
        }
        std::cout << "SESSION_FLOW_PASS turns=9 retry=true results=true\n";
        return 0;
    }

    const bool smokeWindow = HasArgument(argc, argv, "--window-smoke");
    const bool debugScreens = HasArgument(argc, argv, "--debug-screens");
    if (argc != 1 && !smokeWindow && !debugScreens) {
        std::cerr << "Usage: locus.exe [--loop-check | --screen-check | "
                     "--session-check | --window-smoke | --debug-screens]\n";
        return 2;
    }

    // Rendering must swap on the monitor's vertical blank. Without VSync
    // a vertical path can jump sideways partway down the screen while the
    // monitor scans two different camera positions (screen tearing).
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(kWindowWidth, kWindowHeight, "LOCUS");
    if (!IsWindowReady()) {
        std::cerr << "WINDOW_INIT_FAILED\n";
        return 1;
    }
    SetWindowMinSize(800, 450);
    SetExitKey(KEY_NULL); // Esc is Back; quitting always needs explicit consent.
    if (!locus::app::InstallNativeCloseGuard()) {
        std::cerr << "WINDOW_CLOSE_GUARD_FAILED\n";
        CloseWindow();
        return 1;
    }
    UiFont uiFont = LoadUiFont();
    std::cout << "LOCUS_WINDOW_OPEN\n";

    const auto userData = locus::app::UserDataDirectory();
    locus::app::GameSettings settings;
    locus::app::SettingsUi settingsUi;
    std::string settingsError;
    if (!locus::app::LoadGameSettings(userData / L"settings.json",
                                      settings, settingsError)) {
        settingsUi.notice = "DEFAULTS USED: " + settingsError;
        settingsUi.noticeError = true;
    }
    const auto applyDisplaySettings = [&settings]() {
        // Change the Windows swap interval live; the GPU driver may still
        // override the request. Never tie musical time to rendering.
        if (settings.vsync) SetWindowState(FLAG_VSYNC_HINT);
        else ClearWindowState(FLAG_VSYNC_HINT);
        // A 0 cap means unlimited; VSync, if enabled, still paces frames.
        SetTargetFPS(settings.fpsLimit);
    };
    applyDisplaySettings();
    // A single selected skin applies to every chart, independently of Songs.
    const auto activeSkinFile = userData / L"Skins" / L"active.locus-skin.json";
    locus::app::Skin activeSkin;
    locus::app::Skin draftSkin;
    locus::app::SkinUi skinUi;
    std::string skinError;
    if (!locus::app::LoadSkin(activeSkinFile, activeSkin, skinError)) {
        skinUi.notice = "SKIN DEFAULTS USED: " + skinError;
        skinUi.noticeError = true;
    }
    draftSkin = activeSkin;
    skinUi.saved = activeSkin;
    double speedNoticeUntil = -1.0;
    locus::app::SongLibrary library(userData / L"Songs");
    std::string menuStatus;
    if (!library.Refresh(menuStatus))
        std::cerr << "LIBRARY_SCAN_FAIL " << menuStatus << '\n';
    locus::app::SongSelectUi libraryUi;
    libraryUi.notice = menuStatus;
    if (settings.fullscreen) ToggleBorderlessWindowed();

    AppState state;
    const locus::core::Chart demo = locus::core::MakeDemoChart();
    locus::core::Chart activeChart = demo;
    locus::core::ChartEditor editor;
    locus::app::EditorUiState editorUi;
    locus::core::TurnJudge judge(activeChart); // Stable reference across chart loads.
    locus::app::AudioDevice audioDevice; // Outlives every borrowed music stream.
    locus::app::DemoTransport transport;
    locus::app::LocalMusic localSong;
    locus::app::ChartPlayback chartPlayback(localSong);
    locus::app::GameSession gameSession(activeChart, judge, transport, chartPlayback);
    bool customChart = false;
    bool sessionCompleted = false;
    std::wstring editorChartPath;
    std::wstring editorAudioPath;
    std::wstring loadedAudioPath; // Reuse already-decoded audio for each difficulty.
    locus::app::EditorDiscardGuard editorDiscard;
    std::string editorStatus = "O ATTACH AUDIO  |  CTRL+S SAVE  |  T TEST PLAY";
    int editorPreviewStartBeat = 0; // R returns to this preview origin.
    std::string chartTitle = "BUILT-IN DEMO";
    std::string musicError;
    locus::app::EditorSession editorSession{
        editor, editorUi, chartPlayback, localSong,
        editorAudioPath, editorPreviewStartBeat, editorStatus
    };
    locus::app::EditorFileActions editorFiles{
        {editor, editorUi, localSong, chartPlayback, editorChartPath,
         editorAudioPath, loadedAudioPath, editorPreviewStartBeat, editorStatus,
         library, libraryUi, menuStatus},
        {locus::app::ChooseChartFile, locus::app::ChooseLocalMusicFile,
         locus::app::ChooseChartSaveFile, MonotonicSeconds}
    };
    locus::app::EditorSetController editorSet{
        {library, editorFiles, editor, editorUi, editorChartPath,
         editorStatus, libraryUi, menuStatus},
        {locus::app::ChooseLibraryImportFile,
         locus::app::ChooseSongSetExportFile}
    };
    const locus::app::SessionTransitionContext transitions{
        activeChart, demo, judge, gameSession, chartPlayback, localSong, editorUi,
        customChart, sessionCompleted, chartTitle
    };
    if (!audioDevice.Initialize())
        std::cerr << "AUDIO_DEVICE_UNAVAILABLE\n";
    transport.Initialize(demo); // Borrows AudioDevice; failure does not close it.
    transport.SetVolume(settings.volume);
    localSong.SetVolume(settings.volume);
    gameSession.SetChordWindowMs(settings.chordWindowMs);

    const auto persistSettings = [&]() {
        std::string failure;
        if (!locus::app::SaveGameSettings(userData / L"settings.json",
                                          settings, failure)) {
            settingsUi.notice = "SETTINGS NOT SAVED: " + failure;
            settingsUi.noticeError = true;
        } else {
            settingsUi.notice = "SETTINGS SAVED";
            settingsUi.noticeError = false;
        }
    };

    locus::app::SkinProfileActions skinProfiles{
        activeSkin, draftSkin, skinUi, activeSkinFile,
        {locus::app::ChooseSkinFile, locus::app::ChooseSkinSaveFile}
    };
    skinProfiles.RefreshProfiles();
    locus::app::QuitMediaCoordinator quitMedia;
    locus::app::ListenSession listener{
        localSong, loadedAudioPath, musicError,
        locus::app::ChooseLocalMusicFile, MonotonicSeconds
    };

    while (state.running) {
        const bool wasQuitConfirmation = state.quitConfirmation;
        // While the modal is open, Y/N/Esc or its buttons are the ONLY
        // accepted actions; key presses cannot reach the editor or gameplay.
        FrameInput input = wasQuitConfirmation
            ? FrameInput{} : PollInput(debugScreens, state.screen, settings, MonotonicSeconds());
        if (wasQuitConfirmation) {
            locus::app::ConsumeNativeCloseRequest(); // Discard repeated X while modal.
            input.quitConfirmed = IsKeyPressed(KEY_Y);
            input.quitCanceled = IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                const auto transform = state.screen == Screen::Play
                    ? locus::app::ViewportTransform::Gameplay(
                          GetScreenWidth(), GetScreenHeight())
                    : locus::app::ViewportTransform::Menu(
                          GetScreenWidth(), GetScreenHeight());
                if (transform.scale > 0.0f) {
                    const Vector2 virtualMouse = transform.ToVirtual(GetMousePosition());
                    if (CheckCollisionPointRec(virtualMouse, kQuitConfirmButton))
                        input.quitConfirmed = true;
                    else if (CheckCollisionPointRec(virtualMouse, kQuitCancelButton))
                        input.quitCanceled = true;
                }
            }
        }
        if (!wasQuitConfirmation && state.screen == Screen::Editor &&
            input.editor.mouseClick &&
            CheckCollisionPointRec(input.editor.mouse,
                                   locus::app::detail::kEditorBack)) {
            input.backPressed = true;
        }
        const bool importFromHome =
            !wasQuitConfirmation && !input.closeRequested &&
            state.screen == Screen::Home && input.chooseChartPressed;
        const MenuEvents menuEvents =
            (wasQuitConfirmation || input.closeRequested) ? MenuEvents{} :
            HandleMenuInput(state, input, library, libraryUi, settings,
                            settingsUi, draftSkin, skinUi);
        if (menuEvents.toggleFullscreen) ToggleBorderlessWindowed();
        if (menuEvents.applyDisplaySettings) applyDisplaySettings();
        if (state.screen == Screen::Skin)
            skinProfiles.Apply(menuEvents.skinCommand);
        if (menuEvents.saveSettings) {
            transport.SetVolume(settings.volume);
            localSong.SetVolume(settings.volume);
            gameSession.SetChordWindowMs(settings.chordWindowMs);
            persistSettings();
        }
        // Typing into a field cannot simultaneously leave the editor or
        // trigger a file dialog, shortcut, heading rotation, or Test Play.
        if (state.screen == Screen::Editor && editorUi.EditingText()) {
            input.homePressed = false;
            input.backPressed = false;
            // An OS window-close request must still show confirmation,
            // even when a chart text field currently has focus.
            input.requestedScreen.reset();
            input.editorNew = false;
            input.editorOpen = false;
            input.editorSave = false;
            input.editorSaveAs = false;
            input.chooseMusicPressed = false;
            input.editorTest = false;
        }
        const Screen previousScreen = state.screen;
        if (previousScreen == Screen::Editor && editorSet.HasPending() &&
            !wasQuitConfirmation) {
            if (input.closeRequested) {
                // The native quit dialog takes priority over editor modals.
                editorSet.CancelPending();
            } else {
                const bool approve = IsKeyPressed(KEY_Y);
                const bool cancel = IsKeyPressed(KEY_N) ||
                                    IsKeyPressed(KEY_ESCAPE);
                input = FrameInput{}; // No actions beneath a modal confirmation.
                editorSet.Confirm(approve, cancel);
            }
        }
        if (previousScreen == Screen::Editor && !wasQuitConfirmation) {
            const auto outcome = editorDiscard.Handle(
                input, editor.Dirty(), IsKeyPressed(KEY_Y),
                IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE));
            if (outcome == locus::app::EditorDiscardOutcome::ChangesKept)
                editorStatus = "CHANGES KEPT - CTRL+S TO SAVE";
        }
        // Editor discard confirmation must filter F11 before any UI action
        // takes effect; only the approved navigation request may proceed.
        if (input.fullscreenPressed && !input.closeRequested) {
            ToggleBorderlessWindowed(); // F11: fullscreen desktop / resizable window
            settings.fullscreen = !settings.fullscreen;
            persistSettings();
        }
        UpdateState(state, input, smokeWindow);
        if (state.quitConfirmation) input = FrameInput{}; // No actions beneath dialog.
        if (previousScreen != Screen::Skin && state.screen == Screen::Skin) {
            // PLAY can change global skin speed without visiting SKIN.
            // Entering the editor always previews the actual active skin,
            // not a stale draft from a previous screen visit.
            draftSkin = activeSkin;
            skinUi.saved = activeSkin;
            skinUi.dirty = false;
            skinProfiles.RefreshProfiles();
        }
        if (previousScreen == Screen::Skin && state.screen != Screen::Skin) {
            // Back/Esc cancels all unsaved skin edits.
            draftSkin = activeSkin;
            skinUi.dirty = false;
            skinUi.editingName = false;
            skinUi.editingNumberRow = -1;
            skinUi.numberBuffer.clear();
            skinUi.hexBuffer.clear();
            skinUi.editingHex = false;
            skinUi.draggingRow = -1;
            skinUi.draggingColor = 0;
        }
        if (!state.running) {
            break;
        }
        double now = MonotonicSeconds();
        // Only the transport paused by the quit modal may be resumed.
        locus::app::UpdateQuitMedia(
            quitMedia, wasQuitConfirmation, state.quitConfirmation, state.screen,
            customChart, now, gameSession, transport, chartPlayback, localSong);
        bool loadedNewSong = false;
        // Home uses E to enter Editor and E also rotates its heading.
        // Ignore that initial key-down; start editing on the next frame.
        if (state.screen == Screen::Editor &&
            previousScreen == Screen::Editor &&
            !state.quitConfirmation &&
            editorDiscard.Pending() == EditorPending::None) {
            const auto uiAction = editorSession.ApplyControls(input.editor, now);
            // The Song Set tab owns its own hitboxes. A click there must
            // never accidentally edit a route or bypass the discard prompt.
            if (editorUi.tab == locus::app::EditorTab::Set) {
                editorSet.HandleClick(input.editor.mouse,
                                      input.editor.mouseClick,
                                      input.editor.mouseWheel);
                // Import/export can invoke a native Windows file picker.
                now = MonotonicSeconds();
            }
            if (uiAction.chooseAudio) input.chooseMusicPressed = true;
            if (uiAction.testPlay) input.editorTest = true;

            if (input.editorNew) {
                editorFiles.NewDraft();
            } else if (input.editorOpen) {
                editorFiles.Open(now, musicError);
            } else if (input.chooseMusicPressed) {
                editorFiles.AttachAudio(now);
            } else if (input.editorSave || input.editorSaveAs) {
                editorFiles.Save(input.editorSaveAs, now);
            } else if (input.editorTest) {
                const locus::app::EditorTestTarget target{
                    activeChart, chartTitle, judge, gameSession, customChart
                };
                if (editorSession.BeginTest(target, now, musicError))
                    state.screen = Screen::Play;
            }
        }
        if (input.chooseMusicPressed &&
            (state.screen == Screen::Home || state.screen == Screen::Listen)) {
            loadedNewSong = listener.ChooseAudio(state.screen, now);
            if (loadedNewSong) state.screen = Screen::Listen;
        }

        bool loadedNewChart = false;
        if (state.screen == Screen::SongSelect) {
            if (menuEvents.refresh)
                locus::app::RefreshLibraryBrowser(library, libraryUi, menuStatus);
            if (menuEvents.import || importFromHome) {
                const auto selected = locus::app::ChooseLibraryImportFile();
                now = MonotonicSeconds();
                if (!selected.path.empty()) {
                    if (std::filesystem::path(selected.path).extension() ==
                            L".locuspack") {
                        std::filesystem::path installedSet;
                        if (!library.ImportPackage(
                                std::filesystem::path(selected.path),
                                installedSet, musicError)) {
                            libraryUi.notice = "PACKAGE IMPORT FAILED: " + musicError;
                        } else {
                            for (std::size_t i = 0; i < library.Songs().size(); ++i) {
                                if (library.Songs()[i].folder == installedSet) {
                                    libraryUi.SelectSong(
                                        static_cast<int>(i) + 1, library);
                                    break;
                                }
                            }
                            libraryUi.notice = "SONG SET PACKAGE IMPORTED";
                        }
                    } else if (menuEvents.importIntoSelected &&
                        libraryUi.selectedSong > 0 &&
                        libraryUi.selectedSong <=
                            static_cast<int>(library.Songs().size()) &&
                        library.Songs()[libraryUi.selectedSong - 1].managed) {
                        const auto setFolder =
                            library.Songs()[libraryUi.selectedSong - 1].folder;
                        locus::app::ImportLibraryChartToSet(
                            library, libraryUi,
                            std::filesystem::path(selected.path),
                            setFolder, menuStatus);
                    } else if (menuEvents.importIntoSelected) {
                        libraryUi.notice = "SELECT A MANAGED SONG SET FIRST";
                    } else {
                        locus::app::ImportLibraryChart(
                            library, libraryUi,
                            std::filesystem::path(selected.path), menuStatus);
                    }
                } else if (!selected.canceled) {
                    libraryUi.notice = "IMPORT FAILED: " + selected.error;
                }
            }
            if (menuEvents.exportSelected) {
                if (libraryUi.selectedSong > 0 &&
                    libraryUi.selectedSong <=
                        static_cast<int>(library.Songs().size())) {
                    const auto& set = library.Songs()[libraryUi.selectedSong - 1];
                    const auto suggestion =
                        (std::filesystem::current_path() /
                         (set.folder.filename().wstring() + L".locuspack")).wstring();
                    const auto chosen =
                        locus::app::ChooseSongSetExportFile(suggestion);
                    now = MonotonicSeconds();
                    if (!chosen.path.empty()) {
                        if (library.ExportPackage(set.folder,
                                std::filesystem::path(chosen.path), musicError))
                            libraryUi.notice = "SONG SET EXPORTED";
                        else
                            libraryUi.notice = "EXPORT FAILED: " + musicError;
                    } else if (!chosen.canceled) {
                        libraryUi.notice = "EXPORT FAILED: " + chosen.error;
                    }
                } else {
                    libraryUi.notice = "SELECT A SONG SET TO EXPORT";
                }
            }
            if (menuEvents.restoreDeleted) {
                if (!library.RestoreLastDeleted(musicError))
                    libraryUi.notice = "RESTORE FAILED: " + musicError;
                else
                    libraryUi.notice = "RESTORED FROM TRASH";
                libraryUi.SelectSong(libraryUi.selectedSong, library);
            }
            if (menuEvents.trashDifficulty) {
                if (const auto* selected = libraryUi.Selected(library)) {
                    const auto oldSong = libraryUi.selectedSong;
                    const auto oldChart = libraryUi.selectedChart;
                    const auto file = selected->file;
                    if (!library.TrashDifficulty(file, musicError))
                        libraryUi.notice = "DELETE FAILED: " + musicError;
                    else {
                        libraryUi.SelectSong(oldSong, library);
                        libraryUi.SelectChart(oldChart, library);
                        libraryUi.notice = "DIFFICULTY MOVED TO TRASH - U RESTORE";
                    }
                }
            }
            if (menuEvents.trashSongSet) {
                if (libraryUi.selectedSong > 0 &&
                    libraryUi.selectedSong <=
                        static_cast<int>(library.Songs().size())) {
                    const auto folder =
                        library.Songs()[libraryUi.selectedSong - 1].folder;
                    if (!library.TrashSongSet(folder, musicError))
                        libraryUi.notice = "DELETE FAILED: " + musicError;
                    else {
                        libraryUi.SelectSong(libraryUi.selectedSong, library);
                        libraryUi.notice = "SONG SET MOVED TO TRASH - U RESTORE";
                    }
                }
            }
            if (menuEvents.duplicateDifficulty) {
                if (const auto* selected = libraryUi.Selected(library)) {
                    const auto original = selected->file;
                    std::filesystem::path copy;
                    if (!library.DuplicateDifficulty(original, copy, musicError))
                        libraryUi.notice = "COPY FAILED: " + musicError;
                    else {
                        int s = -1, c = -1;
                        if (library.FindChart(copy, s, c)) {
                            libraryUi.SelectSong(s + 1, library);
                            libraryUi.SelectChart(c, library);
                        }
                        libraryUi.notice = "DIFFICULTY COPIED";
                    }
                }
            }
            if (menuEvents.newDifficulty) {
                if (libraryUi.selectedSong > 0 &&
                    libraryUi.selectedSong <=
                        static_cast<int>(library.Songs().size())) {
                    const auto& set = library.Songs()[libraryUi.selectedSong - 1];
                    if (!set.managed) {
                        libraryUi.notice = "IMPORT LEGACY CHART AS A NEW SONG SET FIRST";
                    } else if (editorFiles.NewDifficultyFromLibrary(
                                   set.audioFile.wstring(), set.folder.wstring(),
                                   musicError)) {
                        state.screen = Screen::Editor;
                    } else {
                        libraryUi.notice = "NEW DIFFICULTY FAILED: " + musicError;
                    }
                } else {
                    libraryUi.notice = "SELECT AN INSTALLED SONG FIRST";
                }
            } else if (menuEvents.editSelected) {
                const auto* selected = libraryUi.Selected(library);
                if (!selected) {
                    state.screen = Screen::Editor; // Demo uses the existing draft.
                } else {
                    if (editorFiles.OpenFromLibrary(
                            selected->file.wstring(), musicError)) {
                        state.screen = Screen::Editor;
                    } else {
                        libraryUi.notice = "EDIT FAILED: " + musicError;
                    }
                }
            } else if (menuEvents.playSelected) {
                if (libraryUi.selectedSong == 0) {
                    customChart = false;
                    activeChart = demo;
                    chartTitle = "BUILT-IN DEMO";
                    gameSession.ClearTestOrigin();
                    judge.SetOffsetSeconds(0.0);
                    state.screen = Screen::Play;
                } else if (const auto* selected = libraryUi.Selected(library)) {
                    const locus::app::LibraryPlaybackContext playbackContext{
                        localSong, loadedAudioPath, activeChart, chartTitle,
                        chartPlayback, judge, gameSession, customChart
                    };
                    if (locus::app::ActivateLibraryDifficulty(
                            *selected, playbackContext, now, musicError)) {
                        loadedNewChart = true;
                        state.screen = Screen::Play;
                    } else {
                        libraryUi.notice = "PLAY FAILED: " + musicError;
                    }
                }
            }
        }

        if (state.screen != previousScreen) {
            locus::app::ApplySessionTransition(
                transitions, previousScreen, state.screen, loadedNewChart, now);
        } else if (state.screen == Screen::Play) {
            if (input.baseSpeedDelta != 0) {
                const float next = locus::app::AdjustBaseSpeed(
                    activeSkin.baseSpeedPxPerSecond, input.baseSpeedDelta);
                if (next != activeSkin.baseSpeedPxPerSecond) {
                    activeSkin.baseSpeedPxPerSecond = next;
                    std::string error;
                    if (!locus::app::SaveSkin(activeSkinFile, activeSkin, error)) {
                        std::cerr << "SKIN_SPEED_SAVE_FAILED " << error << '\n';
                        menuStatus = "SPEED NOT SAVED: " + error;
                    }
                    draftSkin = activeSkin;
                    skinUi.saved = activeSkin;
                    speedNoticeUntil = now + 1.5;
                }
            }
            if (customChart && input.practiceSpeedIndex >= 0) {
                const double speeds[] = {1.0, 0.75, 0.5, 0.25, 0.1};
                gameSession.SetPracticeSpeed(customChart, speeds[input.practiceSpeedIndex], now);
            }
            if (input.restartPressed ||
                (input.requestedScreen && *input.requestedScreen == Screen::Play)) {
                sessionCompleted = false;
                gameSession.Restart(customChart, now);
            } else if (input.pausePressed) {
                gameSession.TogglePause(customChart, now);
            }
        }
        if (state.screen == Screen::Listen)
            listener.Update(loadedNewSong, input.restartPressed,
                            input.startPressed, input.pausePressed, now);
        if (state.screen == Screen::Editor)
            editorSession.UpdatePreview(now);
        if (state.screen == Screen::Play) {
            gameSession.Update(customChart, now,
                               {input.arrowPressed, input.arrowCapturedAt, input.arrowHeld});
            const bool audioFinished = customChart
                ? chartPlayback.Finished() : transport.Finished();
            if (ChartReadyForResults(judge, audioFinished)) {
                FinishSession(state, judge);
                sessionCompleted = true;
                // Stop the music immediately if the last judged note occurs
                // before the end of the WAV/MP3. The transport and chart
                // playback are not allowed to keep running on Results.
                gameSession.Stop(customChart);
            }
        }
        const double renderNow = MonotonicSeconds();
        const double songTime = customChart ? chartPlayback.AudioSeconds(renderNow)
                                            : transport.SongSeconds(renderNow);
        const bool paused = customChart ? chartPlayback.Paused() : transport.Paused();
        const bool finished = customChart ? chartPlayback.Finished() : transport.Finished();
        const char* status = customChart
            ? (finished ? "CHART FINISHED"
               : paused ? "PAUSED  |  SPACE RESUME"
               : songTime < 0.0 ? "SILENT PREROLL  |  ARROWS TO TURN"
                                : "CUSTOM SONG  |  ARROWS TO TURN")
            : transport.Status();
        const GameplayView gameplay{activeChart, judge, gameSession.Flash(),
                                     chartTitle, songTime,
                                    customChart ? chartPlayback.Beat(renderNow)
                                                : transport.Beat(renderNow),
                                    paused, sessionCompleted, customChart,
                                    customChart ? chartPlayback.Speed() : 1.0,
                                    status, settings.hitFlash, settings, activeSkin,
                                    speedNoticeUntil};
        RenderFrame(state, debugScreens, uiFont.value, demo,
                    gameplay, localSong, editor, editorUi, chartPlayback,
                    editorStatus, editorDiscard.Pending(),
                    editorSet.View(), editorSet.Pending(),
                    musicError, renderNow,
                    library, libraryUi, settings, settingsUi,
                    draftSkin, skinUi, menuStatus);
    }

    // Both borrowed streams must unload BEFORE closing the shared device.
    chartPlayback.Stop();
    localSong.Shutdown();
    transport.Shutdown();
    audioDevice.Shutdown();
    if (uiFont.owned) {
        UnloadFont(uiFont.value);
    }
    locus::app::RemoveNativeCloseGuard();
    CloseWindow();
    std::cout << "LOCUS_WINDOW_CLOSED\n";
    if (smokeWindow) {
        if (IsWindowReady() || state.frames != kSmokeFrames) {
            std::cerr << "WINDOW_SMOKE_FAIL\n";
            return 1;
        }
        std::cout << "WINDOW_SMOKE_PASS frames=" << state.frames << '\n';
    }
    return 0;
}
