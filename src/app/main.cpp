#include "core/route.h"
#include "core/judgement.h"
#include "app/demo_transport.h"
#include "app/audio_device.h"
#include "app/local_music.h"
#include "app/chart_playback.h"
#include "app/music_picker.h"
#include "app/chart_editor_ui.h"
#include "app/app_input.h"
#include "app/screen_action_buttons.h"
#include "app/app_runtime_policy.h"
#include "app/play_screen_actions.h"
#include "app/play_adjustments.h"
#include "app/play_presentation.h"
#include "app/screen_fade_transition.h"
#include "app/app_smoke_checks.h"
#include "app/app_frame_renderer.h"
#include "app/image_asset_texture.h"
#include "app/menu_controller.h"
#include "app/editor_session.h"
#include "app/editor_file_actions.h"
#include "app/editor_set_controller.h"
#include "app/editor_screen_actions.h"
#include "app/editor_entry_policy.h"
#include "app/listen_session.h"
#include "app/menu_music_preview.h"
#include "app/audio_volume_fade.h"
#include "app/skin_profile_actions.h"
#include "app/visual_style.h"
#include "app/game_session.h"
#include "app/gameplay_ui.h"
#include "app/song_library.h"
#include "app/song_set_storage.h"
#include "app/library_browser.h"
#include "app/library_launch_actions.h"
#include "app/game_settings.h"
#include "app/skin.h"
#include "app/skin_menu.h"
#include "app/menu_ui.h"
#include "app/native_close.h"
#include "app/window_mode.h"
#include "app/screen_flow.h"
#include "app/session_transition.h"
#include "app/quit_media.h"
#include "app/editor_discard_guard.h"

#include "raylib.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

// Production screens share one window and one input/update/render loop.
namespace {
using locus::app::AppState;
using locus::app::FrameInput;
using locus::app::MenuEvents;
using locus::app::HandleMenuInput;
using locus::app::Screen;
using locus::app::EditorPending;
using locus::app::kSmokeFrames;
using locus::app::UpdateState;
using locus::app::UiFont;
using locus::app::LoadUiFont;
using locus::app::RenderFrame;
using locus::app::GameplayView;

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
    locus::app::SongLibrary library(userData / L"Songs");
    std::string menuStatus;
    bool hotkeySkinSavePending = false;
    double hotkeySkinSaveAfter = 0.0;
    const auto persistHotkeySkin = [&]() {
        if (!hotkeySkinSavePending) return;
        std::string error;
        if (!locus::app::SaveSkin(activeSkinFile, activeSkin, error)) {
            std::cerr << "SKIN_HOTKEY_SAVE_FAILED " << error << '\n';
            menuStatus = "SKIN SETTINGS NOT SAVED: " + error;
        }
        hotkeySkinSavePending = false;
    };
    if (!library.Refresh(menuStatus))
        std::cerr << "LIBRARY_SCAN_FAIL " << menuStatus << '\n';
    locus::app::SongSelectUi libraryUi;
    libraryUi.notice = menuStatus;
    if (settings.fullscreen && !locus::app::SetBorderlessDesktop(true)) {
        settings.fullscreen = false;
        settingsUi.notice = "BORDERLESS DESKTOP MODE UNAVAILABLE";
        settingsUi.noticeError = true;
    }

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
         locus::app::ChooseSongSetExportFile,
         locus::app::ChooseSongBackgroundFile}
    };
    locus::app::EditorScreenActions editorActions{
        {editorSession, editorFiles, editorSet, editorUi,
         {activeChart, chartTitle, judge, gameSession, customChart},
         musicError, MonotonicSeconds}
    };
    const locus::app::SessionTransitionContext transitions{
        activeChart, demo, judge, gameSession, chartPlayback, localSong, editorUi,
        customChart, sessionCompleted, chartTitle
    };
    const locus::app::LibraryLaunchContext libraryLaunch{
        library, libraryUi, editorFiles, transitions, loadedAudioPath,
        state, musicError
    };
    locus::app::PlayPresentation presentation;
    locus::app::PlayScreenActions playActions{
        {state, gameSession, judge, transport, chartPlayback,
         customChart, sessionCompleted, activeChart, presentation.Outro()}
    };
    if (!audioDevice.Initialize())
        std::cerr << "AUDIO_DEVICE_UNAVAILABLE\n";
    transport.Initialize(demo); // Borrows AudioDevice; failure does not close it.
    locus::app::MenuMusicPreview menuMusic;
    menuMusic.Initialize(demo);
    locus::app::ImageAssetTexture songBackground;
    locus::app::AudioVolumeFade gameplayVolume;
    gameplayVolume.Full();
    bool drainingGameplayAudio = false;
    bool drainingCustomAudio = false;
    transport.SetVolume(settings.volume);
    localSong.SetVolume(settings.volume);


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
    locus::app::PlayAdjustmentRepeater adjustmentRepeater;
    locus::app::ScreenFadeTransition screenFade;
    bool playCameFromEditor = false;
    std::filesystem::path playedChartPath;
    std::filesystem::path playedBackgroundPath;
    locus::app::ListenSession listener{
        localSong, loadedAudioPath, musicError,
        locus::app::ChooseLocalMusicFile, MonotonicSeconds
    };

    while (state.running) {
        const bool wasQuitConfirmation = state.quitConfirmation;
        const bool wasLevelExitConfirmation = state.levelExitConfirmation;
        const bool wasAnyModal = wasQuitConfirmation || wasLevelExitConfirmation;
        FrameInput input = locus::app::PollFrameInput(
            debugScreens, state, settings, MonotonicSeconds(),
            state.screen == Screen::Play &&
                (customChart ? chartPlayback.Paused() : transport.Paused()));
        const auto [repeatSpeed, repeatPreview] = adjustmentRepeater.Advance(
            input.baseSpeedHeld, input.previewTimeHeld, GetFrameTime());
        input.baseSpeedDelta += repeatSpeed;
        input.previewTimeDelta += repeatPreview;
        const MenuEvents menuEvents =
            (wasAnyModal || input.closeRequested) ? MenuEvents{} :
            HandleMenuInput(state, input, library, libraryUi, settings,
                            settingsUi, draftSkin, skinUi);
        if (menuEvents.toggleFullscreen &&
            !locus::app::SetBorderlessDesktop(settings.fullscreen)) {
            settings.fullscreen = !settings.fullscreen;
            settingsUi.notice = "DISPLAY MODE SWITCH FAILED";
            settingsUi.noticeError = true;
        }
        if (menuEvents.applyDisplaySettings) applyDisplaySettings();
        if (state.screen == Screen::Skin)
            skinProfiles.Apply(menuEvents.skinCommand);
        if (menuEvents.saveSettings) {
            transport.SetVolume(settings.volume);
            localSong.SetVolume(settings.volume);
        
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
        const bool editorModalClick = state.screen == Screen::Editor &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const bool editorModalConfirm = editorModalClick &&
            locus::app::MenuActionHit(locus::app::kEditorModalConfirmButton,
                 GetMousePosition(), GetScreenWidth(), GetScreenHeight());
        const bool editorModalCancel = editorModalClick &&
            locus::app::MenuActionHit(locus::app::kEditorModalCancelButton,
                 GetMousePosition(), GetScreenWidth(), GetScreenHeight());
        const Screen previousScreen = state.screen;
        if (previousScreen == Screen::Editor && editorSet.HasPending() &&
            !wasQuitConfirmation) {
            if (input.closeRequested) {
                // The native quit dialog takes priority over editor modals.
                editorSet.CancelPending();
            } else {
                const bool approve = IsKeyPressed(KEY_Y) || editorModalConfirm;
                const bool cancel = IsKeyPressed(KEY_N) ||
                                    IsKeyPressed(KEY_ESCAPE) || editorModalCancel;
                input = FrameInput{}; // No actions beneath a modal confirmation.
                editorSet.Confirm(approve, cancel);
            }
        }
        if (previousScreen == Screen::Editor && !wasQuitConfirmation) {
            const auto outcome = editorDiscard.Handle(
                input, editor.Dirty(),
                IsKeyPressed(KEY_Y) || editorModalConfirm,
                IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE) ||
                    editorModalCancel);
            if (outcome == locus::app::EditorDiscardOutcome::ChangesKept)
                editorStatus = "CHANGES KEPT - CTRL+S TO SAVE";
        }
        // TEST is a temporary view of the *same unsaved editor document*.
        // Returning through SongSelect previously reopened the last selected
        // saved difficulty, making the edited chart appear to vanish.
        if (state.screen != Screen::Play &&
            locus::app::EditorTestExitToEditor(
                state.screen, playCameFromEditor,
                input.backPressed, input.homePressed)) {
            input.requestedScreen = Screen::Editor;
            input.backPressed = false;
            input.homePressed = false;
        }
        // Editor discard confirmation must filter F11 before any UI action
        // takes effect; only the approved navigation request may proceed.
        if (input.fullscreenPressed && !input.closeRequested) {
            if (locus::app::SetBorderlessDesktop(!settings.fullscreen)) {
                settings.fullscreen = !settings.fullscreen;
                persistSettings();
            }
        }
        if (input.levelExitConfirmed && playCameFromEditor)
            input.requestedScreen = Screen::Editor;
        UpdateState(state, input, smokeWindow);
        if (state.quitConfirmation || state.levelExitConfirmation)
            input = FrameInput{}; // Exclusive modal input.
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
        const bool anyModal = state.quitConfirmation || state.levelExitConfirmation;
        if (wasLevelExitConfirmation && !anyModal &&
            state.screen != Screen::Play) {
            // A confirmed leave is not a modal dismissal: do not resume
            // audio from a level that we just left.
            quitMedia.Reset();
        } else {
            locus::app::UpdateQuitMedia(
                quitMedia, wasAnyModal, anyModal, state.screen,
                customChart, now, gameSession, transport, chartPlayback, localSong);
        }
        // Freeze visual outro and independent library preview whenever the
        // native confirmation blocks input; preserve easing progress.
        if (!wasAnyModal && anyModal) {
            if (state.screen == Screen::SongSelect) menuMusic.Pause(now);
            if (state.screen == Screen::Play || state.screen == Screen::Results)
                presentation.Pause(now);
        } else if (wasAnyModal && !anyModal) {
            if (state.screen == Screen::SongSelect) menuMusic.Resume(now);
            if (state.screen == Screen::Play || state.screen == Screen::Results)
                presentation.Resume(now);
        }
        bool loadedNewSong = false;
        // Home uses E to enter Editor and E also rotates its heading.
        // Ignore that initial key-down; start editing on the next frame.
        if (state.screen == Screen::Editor &&
            previousScreen == Screen::Editor &&
            !state.quitConfirmation && !state.levelExitConfirmation &&
            editorDiscard.Pending() == EditorPending::None) {
            if (editorActions.Apply(input, now))
                state.screen = Screen::Play;
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
            loadedNewChart = locus::app::LaunchLibrarySelection(
                libraryLaunch, menuEvents, now);
        }

        // Menu listening never borrows the chart's scored stream. A change
        // of selected audio or preview time fades out the old track and
        // starts the replacement at silence, then eases it back up.
        if (state.screen == Screen::SongSelect && !state.quitConfirmation) {
            const locus::app::LibrarySong* selected = nullptr;
            if (libraryUi.selectedSong > 0 &&
                libraryUi.selectedSong <= static_cast<int>(library.Songs().size()))
                selected = &library.Songs()[static_cast<std::size_t>(
                    libraryUi.selectedSong - 1)];
            std::string error;
            if (!menuMusic.Select(selected, libraryUi.Selected(library), now, error))
                libraryUi.notice = "MENU MUSIC FAILED: " + error;
        } else if (previousScreen == Screen::SongSelect &&
                   state.screen != Screen::SongSelect) {
            menuMusic.FadeOut(now);
        }
        if (state.screen != previousScreen) {
            if (state.screen == Screen::Play && previousScreen != Screen::Results)
                playCameFromEditor = previousScreen == Screen::Editor;
            if (state.screen == Screen::Play) {
                presentation.EnterPlay();
                drainingGameplayAudio = false;
                // The outgoing menu song fades first; the new gameplay
                // stream starts silent and eases up just afterwards.
                gameplayVolume.FadeIn(now, locus::app::kAudioFadeOutSeconds);
            } else if ((previousScreen == Screen::Play ||
                        previousScreen == Screen::Results) &&
                       state.screen != Screen::Results) {
                // The editor and Listen share the scored decoder; stop
                // immediately there rather than a delayed drain that could
                // stop a newly loaded authoring stream.
                drainingGameplayAudio = state.screen != Screen::Editor &&
                                        state.screen != Screen::Listen;
                drainingCustomAudio = customChart;
                gameplayVolume.FadeOut(now);
            }
            locus::app::ApplySessionTransition(
                transitions, previousScreen, state.screen, loadedNewChart, now);
            if (state.screen == Screen::Play && previousScreen != Screen::Results) {
                // Freeze document and artwork identity at launch. Another
                // library selection later must not change the scored song.
                playedChartPath.clear();
                playedBackgroundPath.clear();
                if (customChart) {
                    if (previousScreen == Screen::Editor)
                        playedChartPath = std::filesystem::path(editorChartPath);
                    else if (const auto* picked = libraryUi.Selected(library))
                        playedChartPath = picked->file;
                    int songIndex = -1, difficultyIndex = -1;
                    if (!playedChartPath.empty() && library.FindChart(
                            playedChartPath, songIndex, difficultyIndex) &&
                        songIndex >= 0 &&
                        songIndex < static_cast<int>(library.Songs().size()))
                        playedBackgroundPath = library.Songs()[
                            static_cast<std::size_t>(songIndex)].backgroundImage;
                }
            }
            if (state.screen == Screen::Editor) {
                const auto action = locus::app::ChooseEditorEntry(
                    previousScreen, state.screen, playCameFromEditor,
                    customChart, !playedChartPath.empty());
                if (action == locus::app::EditorEntryAction::NewDraft) {
                    editorFiles.NewDraft();
                } else if (action ==
                           locus::app::EditorEntryAction::OpenPlayedDifficulty) {
                    std::string error;
                    if (!editorFiles.OpenFromLibrary(playedChartPath.wstring(), error)) {
                        editorStatus = "EDIT FAILED: " + error;
                        editorFiles.NewDraft();
                    }
                }
            }
        } else if (state.screen == Screen::Play) {
            const float nextSpeed = locus::app::AdjustBaseSpeed(
                activeSkin.baseSpeedPxPerSecond, input.baseSpeedDelta);
            const float nextPreview = locus::app::AdjustPreviewTimeMs(
                activeSkin.lookaheadMs, input.previewTimeDelta);
            const bool speedChanged = nextSpeed != activeSkin.baseSpeedPxPerSecond;
            const bool previewChanged = nextPreview != activeSkin.lookaheadMs;
            if (speedChanged || previewChanged) {
                activeSkin.baseSpeedPxPerSecond = nextSpeed;
                activeSkin.lookaheadMs = nextPreview;
                draftSkin = activeSkin;
                skinUi.saved = activeSkin;
                presentation.ShowAdjustment(
                    previewChanged
                        ? "PREVIEW " + std::to_string(
                              static_cast<int>(nextPreview)) + " MS"
                        : "SPEED " + std::to_string(
                              static_cast<int>(nextSpeed)) + " PX/S", now);
                // Repeat reaches 25 adjustments/s: persist only after idle.
                hotkeySkinSavePending = true;
                hotkeySkinSaveAfter = now + 0.25;
            }
            playActions.HandleControls(input, now);
        }
        if (state.screen == Screen::Listen)
            listener.Update(loadedNewSong, input.restartPressed,
                            input.startPressed, input.pausePressed, now);
        if (state.screen == Screen::Editor)
            editorSession.UpdatePreview(now);
        if (state.screen == Screen::Play)
            playActions.Update(input, now);
        else if (state.screen == Screen::Results) {
            if (customChart) chartPlayback.Update(now);
            else transport.Update(now);
        }
        if (drainingGameplayAudio && state.screen != Screen::Play &&
            state.screen != Screen::Results) {
            if (drainingCustomAudio) chartPlayback.Update(now);
            else transport.Update(now);
            if (gameplayVolume.Finished(now)) {
                if (drainingCustomAudio) chartPlayback.Stop();
                else transport.Stop();
                drainingGameplayAudio = false;
            }
        }
        if (state.screen == Screen::Play || state.screen == Screen::Results ||
            drainingGameplayAudio) {
            const float gain = settings.volume * gameplayVolume.Gain(now);
            if (customChart) localSong.SetVolume(gain);
            else transport.SetVolume(gain);
        } else {
            // Editor/Listen own the same decoder and need unattenuated audio.
            localSong.SetVolume(settings.volume);
            transport.SetVolume(settings.volume);
        }
        if (hotkeySkinSavePending &&
            (now >= hotkeySkinSaveAfter || state.screen != Screen::Play))
            persistHotkeySkin();
        menuMusic.Update(now, settings.volume);
        const double renderNow = MonotonicSeconds();
        if (state.screen != previousScreen)
            screenFade.Begin(previousScreen, state.screen, renderNow);
        const auto fadeFrame = screenFade.Frame(renderNow);
        // Input and music have already switched. Only the picture holds its
        // outgoing screen for 65ms; Play itself is never delayed.
        AppState visibleState = state;
        if (!state.quitConfirmation)
            visibleState.screen = fadeFrame.visibleScreen;
        const auto transportView = playActions.Transport(renderNow);
        const GameplayView gameplay = presentation.Prepare(
            {activeChart, judge, gameSession.Flash(), chartTitle,
             transportView.songSeconds, transportView.offsetSeconds,
             transportView.paused, sessionCompleted, customChart,
             transportView.practiceSpeed, transportView.status,
             settings.hitFlash, settings, activeSkin},
            state.screen == Screen::Play,
            state.quitConfirmation || state.levelExitConfirmation, renderNow);
        const auto editorSetView = visibleState.screen == Screen::Editor
            ? editorSet.View() : locus::app::EditorSetView{};
        // Artwork lives at Song Set scope, independent of difficulty/BPM.
        // Cached GPU resources change only when the selected image changes.
        std::filesystem::path artPath;
        if (visibleState.screen == Screen::SongSelect) {
            if (libraryUi.selectedSong > 0 &&
                libraryUi.selectedSong <= static_cast<int>(library.Songs().size()))
                artPath = library.Songs()[static_cast<std::size_t>(
                    libraryUi.selectedSong - 1)].backgroundImage;
        } else if (visibleState.screen == Screen::Editor) {
            artPath = editorSetView.backgroundPath;
        } else if ((visibleState.screen == Screen::Play ||
                    visibleState.screen == Screen::Results) && customChart) {
            artPath = playedBackgroundPath;
        }
        std::string imageError;
        if (!songBackground.Sync(artPath, imageError) && !imageError.empty()) {
            const std::string notice = "BACKGROUND FAILED: " + imageError;
            if (state.screen == Screen::Editor) {
                editorUi.feedback = notice;
                editorUi.feedbackError = true;
            } else {
                libraryUi.notice = notice;
            }
        }
        RenderFrame(visibleState, debugScreens, uiFont.value, demo,
                    gameplay, localSong, editor, editorUi, chartPlayback,
                    editorStatus, editorDiscard.Pending(),
                    editorSetView, editorSet.Pending(),
                    musicError, renderNow,
                    library, libraryUi, settings, settingsUi,
                    draftSkin, skinUi, menuStatus,
                    state.quitConfirmation ? 0.0f : fadeFrame.opacity,
                    &songBackground);
    }

    persistHotkeySkin(); // Flush pending adjustments on exit.
    // Both borrowed streams must unload BEFORE closing the shared device.
    chartPlayback.Stop();
    menuMusic.Shutdown();
    songBackground.Shutdown(); // GPU texture before CloseWindow.
    localSong.Shutdown();
    transport.Shutdown();
    audioDevice.Shutdown();
    if (uiFont.owned) {
        UnloadFont(uiFont.value);
    }
    locus::app::SetBorderlessDesktop(false);
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
