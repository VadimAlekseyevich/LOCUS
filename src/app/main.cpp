#include "core/route.h"
#include "core/judgement.h"
#include "app/demo_transport.h"
#include "app/audio_device.h"
#include "app/local_music.h"
#include "app/chart_playback.h"
#include "app/music_picker.h"
#include "app/chart_editor_ui.h"
#include "app/app_input.h"
#include "app/library_file_actions.h"
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
#include "app/editor_comparison.h"
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
#ifdef LOCUS_WITH_RMLUI
#include "app/native_menu_ui.h"
#endif
#include "app/run_results.h"
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
#include "app/editor_save_modal_layout.h"
#include "app/chart_editor_layout.h"
#include "core/tempo_map.h"

#include "raylib.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
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
    locus::app::HomeBrandAssets homeBrand;
    homeBrand.Load();
    locus::app::HomeMenuUi homeUi;
#ifdef LOCUS_WITH_RMLUI
    locus::app::NativeMenuUi nativeMenu;
    if (!nativeMenu.Initialize())
        std::cerr << "RMLUI_UNAVAILABLE_FALLING_BACK_TO_LEGACY_UI\n";
#endif
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
    gameSession.SetGlobalOffsetMs(settings.globalOffsetMs);
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
    locus::app::EditorComparisonCache editorComparison;
    std::wstring comparisonCacheChartPath;
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
    locus::app::LibraryFileActions libraryFileActions{
        library, libraryUi, menuStatus, musicError,
        {locus::app::ChooseLibraryImportFile,
         locus::app::ChooseSongSetExportFile, MonotonicSeconds,
         locus::app::ChooseSongBackgroundFile}
    };
    locus::app::PlayPresentation presentation;
    locus::app::PlayScreenActions playActions{
        {state, gameSession, judge, transport, chartPlayback,
         customChart, sessionCompleted, activeChart, presentation.Outro()}
    };
    if (!audioDevice.Initialize())
        std::cerr << "AUDIO_DEVICE_UNAVAILABLE\n";
    transport.Initialize(demo); // Borrows AudioDevice; failure does not close it.
    // Dedicated calibration stream. It does not touch scoring transports,
    // chart data or the player's existing audio decoder.
    locus::app::DemoTransport calibrationTransport;
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
    const locus::app::RunResultsStore resultsStore(userData / L"Results");
    locus::app::RunKey playedRunKey;
    std::string playedSongTitle = "BUILT-IN DEMO";
    std::optional<locus::app::RunResult> displayedResult;
    bool historicalResult = false;
    const auto currentRunKey = [&]() -> locus::app::RunKey {
        if (libraryUi.selectedSong == 0)
            return {"builtin-demo", "original"};
        const int index = libraryUi.selectedSong - 1;
        if (index < 0 || index >= static_cast<int>(library.Songs().size()))
            return {};
        const auto* chart = libraryUi.Selected(library);
        if (!chart) return {};
        return {library.Songs()[static_cast<std::size_t>(index)].id,
                chart->file.filename().u8string()};
    };
    locus::app::ListenSession listener{
        localSong, loadedAudioPath, musicError,
        locus::app::ChooseLocalMusicFile, MonotonicSeconds
    };

    while (state.running) {
        const bool wasQuitConfirmation = state.quitConfirmation;
        const bool wasLevelExitConfirmation = state.levelExitConfirmation;
        const bool wasAnyModal = wasQuitConfirmation || wasLevelExitConfirmation;
        const double inputNow = MonotonicSeconds();
        if (state.screen == Screen::SongSelect) {
            const auto key = currentRunKey();
            if (key != libraryUi.resultKey) {
                std::string error;
                libraryUi.results = resultsStore.Load(key, error);
                libraryUi.resultKey = key;
                libraryUi.resultScroll = 0;
                if (!error.empty()) libraryUi.notice = "RESULTS: " + error;
            }
        }
        FrameInput input = locus::app::PollFrameInput(
            debugScreens, state, settings, inputNow,
            state.screen == Screen::Play &&
                (customChart ? chartPlayback.Paused() : transport.Paused()));
        const auto [repeatSpeed, repeatPreview] = adjustmentRepeater.Advance(
            input.baseSpeedHeld, input.previewTimeHeld, GetFrameTime());
        input.baseSpeedDelta += repeatSpeed;
        input.previewTimeDelta += repeatPreview;
        MenuEvents menuEvents;
        if (!(wasAnyModal || input.closeRequested)) {
#ifdef LOCUS_WITH_RMLUI
            if (state.screen == Screen::Home && nativeMenu.HomeAvailable())
                nativeMenu.UpdateHome(input, menuStatus);
            else if (state.screen == Screen::SongSelect && nativeMenu.Available())
                menuEvents = nativeMenu.Update(library, libraryUi, input);
            else if (state.screen == Screen::Settings &&
                     nativeMenu.SettingsAvailable()) {
                const bool exclusiveKeyboard =
                    settingsUi.capturing >= 0 ||
                    settingsUi.editingGlobalOffset ||
                    settingsUi.calibrationOpen;
                menuEvents = HandleMenuInput(state, input, library, libraryUi,
                    settings, settingsUi, draftSkin, skinUi, homeUi, inputNow, true);
                nativeMenu.UpdateSettings(settings, settingsUi, input,
                    menuEvents, exclusiveKeyboard);
            } else
#endif
                menuEvents = HandleMenuInput(state, input, library, libraryUi,
                    settings, settingsUi, draftSkin, skinUi, homeUi, inputNow);
        }
        if (state.screen == Screen::SongSelect &&
            menuEvents.viewResultIndex >= 0 &&
            menuEvents.viewResultIndex <
                static_cast<int>(libraryUi.results.size())) {
            displayedResult = libraryUi.results[
                static_cast<std::size_t>(menuEvents.viewResultIndex)];
            historicalResult = true;
            input.requestedScreen = Screen::Results;
            input.startPressed = false;
            input.backPressed = false;
        }
        if (state.screen == Screen::Results && historicalResult) {
            input.startPressed = false;
            input.restartPressed = false;
            input.openEditorPressed = false;
        }
        if (menuEvents.calibrationStop) calibrationTransport.Shutdown();
        if (menuEvents.calibrationStart) {
            locus::core::Chart clickGuide = demo;
            clickGuide.bpm = locus::app::GlobalOffsetCalibrator::kBpm;
            clickGuide.lengthBeats =
                locus::app::GlobalOffsetCalibrator::kTotalQuarterBeats;
            calibrationTransport.Shutdown();
            if (!calibrationTransport.Initialize(clickGuide)) {
                settingsUi.calibrationOpen = false;
                settingsUi.notice = "METRONOME COULD NOT START: CHECK AUDIO DEVICE";
                settingsUi.noticeError = true;
            } else {
                const double startNow = MonotonicSeconds();
                calibrationTransport.SetVolume(settings.volume);
                calibrationTransport.Start(startNow);
                settingsUi.calibration.Start();
                settingsUi.calibrationExpired = false;
            }
        }
        if (menuEvents.calibrationTap && settingsUi.calibrationOpen &&
            calibrationTransport.Playing()) {
            // Use the captured physical key-down timestamp, not the later
            // UI handler's time; both clocks share MonotonicSeconds().
            settingsUi.calibration.Tap(calibrationTransport.SongSeconds(
                menuEvents.calibrationTapAt));
            if (settingsUi.calibration.Complete())
                calibrationTransport.Stop();
        }
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
            calibrationTransport.SetVolume(settings.volume);
            gameSession.SetGlobalOffsetMs(settings.globalOffsetMs);
        
            persistSettings();
        }
        // Comparison popup owns ALL actions until it closes. Global
        // file/Back shortcuts are decoded before EditorSetController sees
        // the click, so suppress them here too. Otherwise clicking outside
        // the popup could accidentally save, replace or leave the draft.
        if (state.screen == Screen::Editor &&
            editorUi.tab == locus::app::EditorTab::Route &&
            editorUi.comparisonPickerOpen) {
            input.backPressed = false;
            input.homePressed = false;
            input.editorNew = false;
            input.editorOpen = false;
            input.editorSave = false;
            input.editorSaveAs = false;
            input.chooseMusicPressed = false;
            input.editorTest = false;
            input.requestedScreen.reset();
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
        const bool saveDraftClick = editorModalClick &&
            locus::app::MenuActionHit(locus::app::kEditorSaveDraftButton,
                GetMousePosition(), GetScreenWidth(), GetScreenHeight());
        const bool discardDraftClick = editorModalClick &&
            locus::app::MenuActionHit(locus::app::kEditorDiscardDraftButton,
                GetMousePosition(), GetScreenWidth(), GetScreenHeight());
        const bool cancelDraftClick = editorModalClick &&
            locus::app::MenuActionHit(locus::app::kEditorCancelDraftButton,
                GetMousePosition(), GetScreenWidth(), GetScreenHeight());
        const Screen previousScreen = state.screen;
        if (previousScreen == Screen::Editor && editorSet.HasPending() &&
            !wasQuitConfirmation) {
            if (input.closeRequested) {
                // The native quit dialog takes priority over editor modals.
                editorSet.CancelPending();
            } else {
                const auto* pendingSet = editorSet.Pending();
                const bool dirty = pendingSet && pendingSet->discardsDraft;
                const bool save = dirty &&
                    (IsKeyPressed(KEY_S) || saveDraftClick);
                bool approve = IsKeyPressed(KEY_Y) ||
                    (dirty ? discardDraftClick : editorModalConfirm);
                const bool cancel = IsKeyPressed(KEY_N) ||
                    IsKeyPressed(KEY_ESCAPE) ||
                    (dirty ? cancelDraftClick : editorModalCancel);
                if (save) {
                    double saveNow = MonotonicSeconds();
                    approve = editorFiles.SaveAll(saveNow);
                }
                input = FrameInput{}; // No actions beneath a modal confirmation.
                editorSet.Confirm(approve, cancel);
            }
        }
        // Inactive comparison panes own real dirty drafts. Closing one
        // requires an explicit Save, Discard, or Cancel before any other
        // editor action is allowed through.
        if (previousScreen == Screen::Editor && !wasQuitConfirmation &&
            !editorFiles.PendingClose().empty() && !input.closeRequested) {
            const bool save = IsKeyPressed(KEY_S) || saveDraftClick;
            const bool discard = IsKeyPressed(KEY_Y) || discardDraftClick;
            const bool cancel = IsKeyPressed(KEY_N) ||
                                IsKeyPressed(KEY_ESCAPE) || cancelDraftClick;
            input = FrameInput{};
            if (save || discard || cancel) {
                double saveNow = MonotonicSeconds();
                editorFiles.ResolveClose(save, discard, saveNow);
            }
        }
        if (previousScreen == Screen::Editor && !wasQuitConfirmation) {
            bool approvedBySaving = false;
            if (editorDiscard.Pending() != EditorPending::None &&
                (IsKeyPressed(KEY_S) || saveDraftClick) &&
                editorFiles.PendingClose().empty()) {
                double saveNow = MonotonicSeconds();
                approvedBySaving = editorFiles.SaveAll(saveNow);
                if (!approvedBySaving) input = FrameInput{};
            }
            const auto outcome = editorDiscard.Handle(
                input, editorFiles.AnyDirty(),
                approvedBySaving || IsKeyPressed(KEY_Y) || discardDraftClick,
                IsKeyPressed(KEY_N) || IsKeyPressed(KEY_ESCAPE) ||
                    cancelDraftClick);
            if (outcome == locus::app::EditorDiscardOutcome::ChangesKept)
                editorStatus = "CHANGES KEPT - CTRL+S TO SAVE";
        }
        // Native window close has its own modal. S saves ALL dirty editor
        // documents before confirming quit; Y quits without saving, N stays.
        if (wasQuitConfirmation && editorFiles.AnyDirty() &&
            (IsKeyPressed(KEY_S) ||
             (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
              locus::app::EditorQuitSavePointerHit(
                  state.screen, GetMousePosition(),
                  GetScreenWidth(), GetScreenHeight())))) {
            const bool skinHasEdits = state.screen == Screen::Skin &&
                (skinUi.dirty || skinUi.editingName ||
                 skinUi.editingNumberRow >= 0 || skinUi.editingHex);
            if (skinHasEdits) {
                // "Save charts and quit" must not erase an unrelated unsaved
                // skin. Cancel the quit modal and save the skin explicitly.
                input.quitConfirmed = false;
                input.quitCanceled = false;
            } else {
                double saveNow = MonotonicSeconds();
                if (editorFiles.SaveAll(saveNow)) {
                    input.quitConfirmed = true;
                    input.quitCanceled = false;
                } else {
                    input.quitConfirmed = false;
                    input.quitCanceled = false;
                }
            }
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
        // The metronome is owned by the settings overlay. Never let it
        // survive navigation, an OS quit confirmation or app shutdown.
        if (settingsUi.calibrationOpen &&
            (state.screen != Screen::Settings || state.quitConfirmation)) {
            settingsUi.calibrationOpen = false;
            calibrationTransport.Shutdown();
        }
        if (previousScreen == Screen::Editor && state.screen == Screen::Home) {
            // Back/Home occurs only after the dirty workspace prompt has
            // been approved or there are no changes. Free all parked drafts,
            // histories and the shared editor decoder instead of presenting
            // already-discarded changes at the next quit confirmation.
            editorFiles.NewDraft();
        }
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
        if (settingsUi.calibrationOpen && calibrationTransport.Playing()) {
            calibrationTransport.Update(now);
            if (!calibrationTransport.Playing() &&
                !settingsUi.calibration.Complete())
                settingsUi.calibrationExpired = true;
        }
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
            // Independent editor preview controls live beside the compass.
            // They are presentation settings, never chart edits or SKIN
            // changes. Consume the click before note/pattern handlers.
            if (editorUi.tab == locus::app::EditorTab::Route &&
                input.editor.mouseClick && !editorUi.comparisonPickerOpen &&
                !editorSet.HasPending() && editorFiles.PendingClose().empty()) {
                const int speedDelta = locus::app::detail::
                    EditorSpeedControlDelta(input.editor.mouse);
                const int lengthDelta = locus::app::detail::
                    EditorPathControlDelta(input.editor.mouse);
                if (speedDelta || lengthDelta) {
                    const bool changed = speedDelta
                        ? settings.StepEditorPreviewSpeed(speedDelta)
                        : settings.StepEditorPreviewLength(lengthDelta);
                    if (changed) persistSettings();
                    input.editor.mouseClick = false;
                }
            }
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
            if (menuEvents.import)
                libraryFileActions.Apply(menuEvents, false, now);
            loadedNewChart = locus::app::LaunchLibrarySelection(
                libraryLaunch, menuEvents, now);
            if (menuEvents.openSet && state.screen == Screen::Editor)
                editorUi.tab = locus::app::EditorTab::Set;
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
            if (state.screen == Screen::Play) {
                displayedResult.reset();
                historicalResult = false;
                if (previousScreen != Screen::Results)
                    playCameFromEditor = previousScreen == Screen::Editor;
            } else if (previousScreen == Screen::Results &&
                       state.screen != Screen::Results) {
                displayedResult.reset();
                historicalResult = false;
            }
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
                playedRunKey = {"builtin-demo", "original"};
                playedSongTitle = "BUILT-IN DEMO";
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
                    {
                        const auto& song = library.Songs()[
                            static_cast<std::size_t>(songIndex)];
                        playedBackgroundPath = song.backgroundImage;
                        playedRunKey = {song.id,
                            playedChartPath.filename().u8string()};
                        playedSongTitle = song.title;
                    }
                }
            }
            if (state.screen == Screen::Editor) {
                const auto action = locus::app::ChooseEditorEntry(
                    previousScreen, state.screen, playCameFromEditor,
                    customChart, !playedChartPath.empty());
                if (editorFiles.AnyDirty()) {
                    // A TEST launch may be followed by another PLAY or a
                    // library visit. Even an entry classified KeepLoaded
                    // must restore the ORIGINAL audio before resuming edits.
                    std::string restoreError;
                    if (editorFiles.RestoreWorkspaceAudio(restoreError))
                        editorStatus = "UNSAVED EDITOR WORKSPACE RESTORED";
                    else
                        editorStatus = "EDITOR AUDIO RESTORE FAILED: " + restoreError;
                } else if (action == locus::app::EditorEntryAction::NewDraft) {
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
        if (state.screen == Screen::Play) {
            const bool alreadyCompleted = sessionCompleted;
            playActions.Update(input, now);
            // Record exactly once at the completed-outro edge, never from
            // editor TEST, an unfinished run or an open Results screen.
            if (!alreadyCompleted && sessionCompleted &&
                !playCameFromEditor && !gameSession.IsEditorTest() &&
                !playedRunKey.Empty() && judge.ScorableCount() > 0 &&
                judge.Counts().Resolved() ==
                    static_cast<int>(judge.ScorableCount())) {
                locus::app::RunResult record;
                record.songSetId = playedRunKey.songSetId;
                record.difficultyFile = playedRunKey.difficultyFile;
                record.songTitle = playedSongTitle;
                record.chartTitle = chartTitle;
                record.grades = judge.Counts();
                record.notesJudged = static_cast<int>(judge.ScorableCount());
                record.practiceRate = playActions.Transport(now).practiceSpeed;
                std::string error;
                if (resultsStore.Save(record, error)) {
                    displayedResult = record;
                    historicalResult = false;
                    libraryUi.resultKey = {}; // Reload the history on return.
                } else {
                    libraryUi.notice = "RESULT NOT SAVED: " + error;
                    std::cerr << libraryUi.notice << '\n';
                }
            }
        } else if (state.screen == Screen::Results && !historicalResult) {
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
        // Do not sample the scored audio stream during ordinary editing or
        // menus: the editor is about to sample that SAME transport once for
        // its synchronized preview and ruler.
        const bool showGameplay = state.screen == Screen::Play ||
            state.screen == Screen::Results ||
            visibleState.screen == Screen::Play ||
            visibleState.screen == Screen::Results;
        const auto transportView = showGameplay
            ? playActions.Transport(renderNow)
            : locus::app::PlayTransportView{};
        const GameplayView gameplay = presentation.Prepare(
            {activeChart, judge, gameSession.Flash(), chartTitle,
             transportView.songSeconds, transportView.offsetSeconds,
             transportView.paused, sessionCompleted, customChart,
             transportView.practiceSpeed, transportView.status,
             settings.hitFlash, settings, activeSkin},
            state.screen == Screen::Play,
            state.quitConfirmation || state.levelExitConfirmation, renderNow);
        // The editor and COMPARE pane routes all see the same one clock
        // snapshot; no transport sampling happens inside drawing methods.
        locus::app::EditorRenderClock editorClock;
        if (visibleState.screen == Screen::Editor) {
            const auto audioFrame = chartPlayback.Snapshot(renderNow);
            editorClock.playing = audioFrame.playing;
            editorClock.paused = audioFrame.paused;
            editorClock.practiceRate = audioFrame.practiceRate;
            const bool preservedFocusTime =
                std::isfinite(editorUi.focusSongSeconds);
            editorClock.songSeconds = audioFrame.playing
                ? audioFrame.visualSeconds
                : preservedFocusTime ? editorUi.focusSongSeconds
                    : editor.AudioSecondsAtBeat(editor.SelectedBeat());
            // A focus click never seeks. Keep the exact shared millisecond
            // even while stopped; the newly focused chart's authored cursor
            // is only a nearest-tick projection for note editing.
            editorClock.tick = audioFrame.playing || preservedFocusTime
                ? locus::core::TempoBeatAtSeconds(
                    editor.Data().route, editor.Data().offsetSeconds,
                    editorClock.songSeconds) * editor.Data().route.ticksPerBeat
                : static_cast<double>(editor.SelectedBeat());
        }
        const auto editorSetView = visibleState.screen == Screen::Editor
            ? editorSet.View() : locus::app::EditorSetView{};
        if (visibleState.screen == Screen::Editor &&
            editorChartPath != comparisonCacheChartPath) {
            editorComparison.Reset();
            comparisonCacheChartPath = editorChartPath;
        }
        const auto comparisonFrame = visibleState.screen == Screen::Editor
            ? editorComparison.Prepare(
                editor, editorUi, editorSetView, localSong.DurationSeconds(),
                editorClock.songSeconds, settings.editorPreviewLengthMs,
                [&editorFiles](const std::filesystem::path& path) {
                    return editorFiles.ParkedEditor(path);
                })
            : locus::app::EditorComparisonFrame{};
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
#ifdef LOCUS_WITH_RMLUI
        if (visibleState.screen != Screen::SongSelect &&
            visibleState.screen != Screen::Home &&
            visibleState.screen != Screen::Settings) nativeMenu.Hide();
#endif
        RenderFrame(visibleState, debugScreens, uiFont.value, demo,
                    gameplay, localSong, editor, editorUi, editorClock,
                    editorStatus, editorDiscard.Pending(),
                    editorSetView, comparisonFrame, editorSet.Pending(),
                    editorFiles.PendingClose(), editorFiles.AnyDirty(),
                    musicError, renderNow,
                    library, homeUi, homeBrand, libraryUi, settings, settingsUi,
                    draftSkin, skinUi, menuStatus,
                    state.quitConfirmation ? 0.0f : fadeFrame.opacity,
                    &songBackground,
                    displayedResult ? &*displayedResult : nullptr,
                    historicalResult
#ifdef LOCUS_WITH_RMLUI
                    , &nativeMenu
#endif
                    );
    }

    persistHotkeySkin(); // Flush pending adjustments on exit.
    // Both borrowed streams must unload BEFORE closing the shared device.
    chartPlayback.Stop();
    menuMusic.Shutdown();
#ifdef LOCUS_WITH_RMLUI
    nativeMenu.Shutdown(); // Rml glyph textures before raylib context.
#endif
    songBackground.Shutdown(); // GPU texture before CloseWindow.
    homeBrand.Unload();
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
