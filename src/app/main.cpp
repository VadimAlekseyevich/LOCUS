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
#include "app/menu_controller.h"
#include "app/chart_editor_layout.h"
#include "app/editor_document.h"
#include "app/editor_session.h"
#include "app/editor_file_actions.h"
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
using locus::app::GameplayView;
using locus::app::GameplayCamera;
using locus::app::GameplayViewportScale;
using locus::app::GameplayRightSidebarWidth;
using locus::app::kGameplayWorldViewport;
using locus::app::DrawPlayGrid;
using locus::app::DrawResultsScreen;

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
constexpr Color kBackground = {21, 23, 33, 255};

double MonotonicSeconds() {
    using Clock = std::chrono::steady_clock;
    static const auto epoch = Clock::now();
    return std::chrono::duration<double>(Clock::now() - epoch).count();
}

bool HasArgument(int argc, char** argv, const char* argument) {
    return argc == 2 && std::strcmp(argv[1], argument) == 0;
}

// Platform-specific input is reduced to navigation actions. The screen
// transition policy stays independently testable without a window.
void UpdateState(AppState& state, const FrameInput& input, bool smokeWindow) {
    locus::app::UpdateScreenFlow(state, {
        input.closeRequested, input.requestedScreen,
        input.quitConfirmed, input.quitCanceled,
        input.startPressed, input.restartPressed,
        input.homePressed, input.backPressed,
        input.openEditorPressed, input.openSkinPressed,
        input.openSettingsPressed, input.openPlayPressed,
        input.chooseChartPressed
    }, smokeWindow);
}

// A scored chart ends when its LAST eligible note has been judged, not when
// the remaining song audio runs out. Empty charts and editor TEST positions
// after the last note have no scorable event: only these need the audio-end
// fallback. GameSession extends the scoring clock briefly past audio end
// so a final late hit or Miss is evaluated before showing Results.
bool ChartReadyForResults(const locus::core::TurnJudge& judge,
                          bool audioFinished) noexcept {
    // If there are scorable notes, all of them must actually be evaluated.
    // Audio can stop before the final note's 180ms Miss window expires;
    // GameSession keeps a silent judgement tail in that case.
    return judge.AllScorableJudged() ||
           (audioFinished && judge.ScorableCount() == 0);
}

void FinishSession(AppState& state, locus::core::TurnJudge& judge) {
    judge.FinishRemainingMisses();
    state.screen = Screen::Results;
}

// The installed Windows UI font is read from the OS, not bundled or redistributed.
// A missing/unreadable TTF gracefully falls back to raylib's built-in font.
struct UiFont {
    Font value{};
    bool owned = false;
};

UiFont LoadUiFont() {
#ifdef _WIN32
    const char* windowsDirectory = std::getenv("WINDIR");
    const std::string fontsDirectory = (windowsDirectory && windowsDirectory[0] != '\0')
                                           ? std::string(windowsDirectory) + "\\Fonts\\"
                                           : "C:\\Windows\\Fonts\\";
    // Include Latin and Cyrillic glyphs for Unicode local track titles.
    // Windows provides these font faces; no third-party fonts are bundled.
    std::vector<int> glyphs;
    glyphs.reserve(95 + 304);
    for (int codepoint = 32; codepoint <= 126; ++codepoint) {
        glyphs.push_back(codepoint);
    }
    for (int codepoint = 0x0400; codepoint <= 0x052F; ++codepoint) {
        glyphs.push_back(codepoint);
    }
    const char* fontFiles[] = {"segoeui.ttf", "arial.ttf"};
    for (const char* filename : fontFiles) {
        const std::string path = fontsDirectory + filename;
        if (!FileExists(path.c_str())) {
            continue;
        }
        Font candidate = LoadFontEx(path.c_str(), 64, glyphs.data(),
                                    static_cast<int>(glyphs.size()));
        // LoadFontEx may return the default font if rasterization fails.
        if (IsFontValid(candidate) &&
            candidate.texture.id != GetFontDefault().texture.id) {
            SetTextureFilter(candidate.texture, TEXTURE_FILTER_BILINEAR);
            std::cout << "UI_FONT_SYSTEM=" << filename << '\n';
            return UiFont{candidate, true};
        }
    }
#endif
    std::cout << "UI_FONT_FALLBACK=raylib-default\n";
    return UiFont{GetFontDefault(), false};
}

void DrawTextLabel(Font font, const char* message, Vector2 position,
                   float size, Color color) {
    DrawTextEx(font, message, position, size, 1.0f, color);
}

// Shared virtual hitboxes for the global quit modal. Keeping them outside
// the screen controller means pressing X from any screen is handled alike.
constexpr Rectangle kQuitConfirmButton{263.0f, 329.0f, 196.0f, 54.0f};
constexpr Rectangle kQuitCancelButton{502.0f, 329.0f, 196.0f, 54.0f};

void DrawQuitConfirmation(Font font, const AppState& state,
                          const locus::core::ChartEditor& editor,
                          const locus::app::EditorUiState& editorUi,
                          const locus::app::SkinUi& skinUi) {
    if (!state.quitConfirmation) return;
    // The outer application does not terminate until a second explicit
    // action. Reassure users about unsaved edits before they choose.
    DrawRectangle(0, 0, kWindowWidth, kWindowHeight, Color{7, 11, 19, 220});
    const Rectangle card{177.0f, 146.0f, 606.0f, 264.0f};
    DrawRectangleRounded(card, 0.085f, 10, Color{34, 42, 59, 255});
    DrawRectangleRoundedLinesEx(card, 0.085f, 10, 2.0f,
                                Color{214, 183, 127, 255});
    DrawTextLabel(font, "EXIT LOCUS?", {309.0f, 177.0f}, 36.0f,
                  Color{246, 233, 211, 255});
    DrawTextLabel(font, "This will close the game.", {307.0f, 236.0f},
                  21.0f, Color{216, 227, 234, 255});
    if (state.screen == Screen::Editor &&
        (editor.Dirty() || editorUi.EditingText()))
        DrawTextLabel(font, "UNSAVED CHART CHANGES WILL BE LOST",
                      {266.0f, 272.0f}, 17.0f,
                      Color{251, 188, 164, 255});
    else if (state.screen == Screen::Skin &&
             (skinUi.dirty || skinUi.editingName || skinUi.editingNumberRow >= 0))
        DrawTextLabel(font, "UNSAVED SKIN CHANGES WILL BE LOST",
                      {273.0f, 272.0f}, 17.0f,
                      Color{251, 188, 164, 255});
    DrawRectangleRounded(kQuitConfirmButton, 0.16f, 5,
                         Color{91, 50, 54, 255});
    DrawRectangleRounded(kQuitCancelButton, 0.16f, 5,
                         Color{38, 91, 82, 255});
    DrawTextLabel(font, "Y  -  QUIT", {305.0f, 343.0f}, 23.0f,
                  Color{251, 226, 217, 255});
    DrawTextLabel(font, "N  -  STAY", {545.0f, 343.0f}, 23.0f,
                  Color{216, 251, 237, 255});
    DrawTextLabel(font, "ESC ALSO CANCELS", {394.0f, 394.0f}, 13.0f,
                  Color{170, 189, 205, 255});
}

void DrawHomeScreen(Font font, const locus::app::SongLibrary& library,
                    const std::string& message) {
    locus::app::DrawMainMenu(font, library, message);
}

void DrawLocalMusicScreen(Font font, const locus::app::LocalMusic& song,
                          double now, const std::string& musicError) {
    DrawTextLabel(font, "YOUR MUSIC  /  LISTEN MODE", Vector2{88.0f, 112.0f},
                  35.0f, Color{163, 233, 228, 255});
    DrawTextLabel(font, song.DisplayName().c_str(), Vector2{89.0f, 187.0f},
                  25.0f, Color{231, 232, 246, 255});

    const double duration = song.DurationSeconds();
    const double elapsed = std::clamp(song.SongSeconds(now), 0.0, duration);
    DrawRectangle(89, 255, 782, 9, Color{55, 66, 84, 255});
    if (duration > 0.0) {
        DrawRectangle(89, 255,
                      static_cast<int>(782.0 * (elapsed / duration)), 9,
                      Color{121, 223, 213, 255});
    }
    const int currentSeconds = static_cast<int>(elapsed);
    const int totalSeconds = static_cast<int>(duration);
    DrawTextLabel(font,
                  TextFormat("%02d:%02d / %02d:%02d",
                             currentSeconds / 60, currentSeconds % 60,
                             totalSeconds / 60, totalSeconds % 60),
                  Vector2{89.0f, 284.0f}, 22.0f,
                  Color{210, 223, 234, 255});

    const char* status = song.Finished() ? "TRACK COMPLETE"
                         : song.Paused() ? "PAUSED"
                         : song.Playing() ? "PLAYING" : "READY";
    DrawTextLabel(font, status, Vector2{89.0f, 332.0f}, 23.0f,
                  Color{147, 226, 222, 255});
    DrawTextLabel(font, "SPACE  PAUSE / RESUME     R / ENTER  REPLAY",
                  Vector2{89.0f, 378.0f}, 19.0f,
                  Color{237, 219, 163, 255});
    DrawTextLabel(font, "O  CHOOSE ANOTHER SONG      H  HOME",
                  Vector2{89.0f, 411.0f}, 19.0f,
                  Color{210, 222, 233, 255});
    DrawTextLabel(font,
                  musicError.empty()
                      ? "NO TURN CHART YET - SONG PREVIEW ONLY"
                      : musicError.c_str(),
                  Vector2{89.0f, 446.0f}, 17.0f,
                  musicError.empty() ? Color{173, 183, 207, 255}
                                     : Color{241, 150, 150, 255});
}

void RenderFrame(const AppState& state, bool debugScreens, Font font,
                 const locus::core::Chart& demo,
                 const GameplayView& gameplay,
                 const locus::app::LocalMusic& localSong,
                 const locus::core::ChartEditor& editor,
                 const locus::app::EditorUiState& editorUi,
                 const locus::app::ChartPlayback& editorTransport,
                 const std::string& editorStatus, EditorPending editorPending,
                 const std::string& musicError, double now,
                  const locus::app::SongLibrary& library,
                  const locus::app::SongSelectUi& libraryUi,
                  const locus::app::GameSettings& settings,
                  const locus::app::SettingsUi& settingsUi,
                  const locus::app::Skin& draftSkin,
                  const locus::app::SkinUi& skinUi,
                  const std::string& menuStatus) {
    BeginDrawing();
    ClearBackground(state.screen == Screen::Play
                        ? gameplay.skin.background : kBackground);

    // Menus/editor retain their original 960x540 16:9 virtual viewport.
    // Gameplay instead scales its 540x540 playfield to the actual window
    // height, even when the window is not 16:9 (landscape assumption).
    // For portrait windows, width limits the square so no side disappears.
    const auto transform = state.screen == Screen::Play
        ? locus::app::ViewportTransform::Gameplay(GetScreenWidth(), GetScreenHeight())
        : locus::app::ViewportTransform::Menu(GetScreenWidth(), GetScreenHeight());
    Camera2D camera{};
    camera.offset = Vector2{transform.offsetX, transform.offsetY};
    camera.target = Vector2{0.0f, 0.0f};
    camera.zoom = transform.scale;
    BeginMode2D(camera);

    // No wordmark or global help text during scoring: only field + hits.
    if (state.screen != Screen::Play) {
        DrawTextLabel(font, "LOCUS", Vector2{50.0f, 38.0f}, 30.0f,
                      Color{223, 225, 238, 255});
    }

    if (state.screen == Screen::Home) {
        DrawHomeScreen(font, library, menuStatus);
    } else if (state.screen == Screen::SongSelect) {
        locus::app::DrawSongSelectMenu(font, library, libraryUi);
    } else if (state.screen == Screen::Skin) {
        locus::app::DrawSkinMenu(font, draftSkin, skinUi);
    } else if (state.screen == Screen::Settings) {
        locus::app::DrawSettingsMenu(font, settings, settingsUi);
    } else if (state.screen == Screen::Listen) {
        DrawLocalMusicScreen(font, localSong, now, musicError);
    } else if (state.screen == Screen::Play) {
        DrawPlayGrid(font, gameplay, now);
        if (gameplay.paused) {
            DrawRectangle(0, 0, kWindowWidth, kWindowHeight,
                          Color{12, 14, 22, 204});
            DrawTextLabel(font, "PAUSED", Vector2{355.0f, 198.0f},
                          59.0f, Color{240, 246, 247, 255});
            const std::string resume = locus::app::KeyboardKeyName(
                gameplay.bindings.Key(locus::app::GameAction::Pause)) + "  -  RESUME";
            DrawTextLabel(font, resume.c_str(),
                          Vector2{325.0f, 287.0f}, 28.0f,
                          Color{239, 219, 164, 255});
            const std::string controls = locus::app::KeyboardKeyName(
                gameplay.bindings.Key(locus::app::GameAction::Retry)) +
                "  RETRY      " + locus::app::KeyboardKeyName(
                gameplay.bindings.Key(locus::app::GameAction::Home)) + "  HOME";
            DrawTextLabel(font, controls.c_str(),
                          Vector2{277.0f, 335.0f}, 21.0f,
                          Color{204, 213, 232, 255});
        }
    } else if (state.screen == Screen::Results) {
        DrawResultsScreen(font, gameplay);
    } else if (state.screen == Screen::Editor) {
        const double selectedBeat = editorTransport.Playing()
            ? editorTransport.Beat(now) : static_cast<double>(editor.SelectedBeat());
        locus::app::RenderChartEditor(font, editor, editorUi,
                                     localSong.DisplayName(), localSong.Ready(),
                                     editorTransport.Playing(), editorTransport.Paused(),
                                     selectedBeat, editorTransport.Speed(), gameplay.bindings);
        const std::string statusLine = editorStatus.substr(0, 68);
        DrawTextLabel(font, statusLine.c_str(), Vector2{50.0f, 525.0f},
                      12.0f, Color{175, 212, 213, 255});
        if (editorPending != EditorPending::None) {
            DrawRectangle(0, 0, kWindowWidth, kWindowHeight, Color{10, 13, 21, 218});
            DrawTextLabel(font, "UNSAVED CHART CHANGES", {193.0f, 187.0f},
                          32.0f, Color{245, 216, 165, 255});
            DrawTextLabel(font, "Y  DISCARD CHANGES", {245.0f, 268.0f},
                          23.0f, Color{235, 156, 156, 255});
            DrawTextLabel(font, "N  CANCEL AND KEEP EDITING", {206.0f, 315.0f},
                          21.0f, Color{201, 226, 226, 255});
            DrawTextLabel(font, "CTRL+S AFTER CANCEL TO SAVE", {218.0f, 364.0f},
                          18.0f, Color{192, 203, 223, 255});
        }
    } else {
        const char* screen = ScreenName(state.screen);
        constexpr float kScreenFontSize = 54.0f;
        const Vector2 textSize = MeasureTextEx(font, screen, kScreenFontSize, 1.0f);
        DrawTextLabel(font, screen,
                      Vector2{(static_cast<float>(kWindowWidth) - textSize.x) / 2.0f,
                              static_cast<float>(kWindowHeight) / 2.0f - 38.0f},
                      kScreenFontSize, Color{211, 239, 241, 255});
        DrawTextLabel(font, "Unexpected screen state - return Home or restart LOCUS",
                      Vector2{50.0f, static_cast<float>(kWindowHeight - 112)},
                      22.0f, Color{208, 211, 225, 255});
    }
    // F11 still works; the fullscreen hint no longer floats across menu
    // buttons or the editor. Debug overlays only belong to explicit diagnostics.
    if (debugScreens && state.screen != Screen::Editor &&
        state.screen != Screen::Settings && state.screen != Screen::Play) {
        DrawTextLabel(font, "DEBUG: F1 Home   F2 Play   F3 Editor   F4 Results",
                      Vector2{50.0f, static_cast<float>(kWindowHeight - 65)},
                      22.0f, Color{232, 213, 160, 255});
    }
    DrawQuitConfirmation(font, state, editor, editorUi, skinUi);
    EndMode2D();
    EndDrawing();
}

// Keep rendering-only layout regressions in the headless screen check.
// This uses the same rectangles as the visible controls and hit testing.
bool CheckPresentationLayout() {
    // Every edge of the gameplay window must form a fully visible,
    // centered 540x540 square; the marker sits at its exact midpoint.
    if (GameplayCamera::kCenterX != kWindowWidth * 0.5f ||
        GameplayCamera::kCenterY != kWindowHeight * 0.5f ||
        kGameplayWorldViewport.x != 210.0f ||
        kGameplayWorldViewport.y != 0.0f ||
        kGameplayWorldViewport.width != static_cast<float>(kWindowHeight) ||
        kGameplayWorldViewport.height != static_cast<float>(kWindowHeight) ||
        kGameplayWorldViewport.x + kGameplayWorldViewport.width != 750.0f ||
        kGameplayWorldViewport.x + kGameplayWorldViewport.width * 0.5f !=
            GameplayCamera::kCenterX ||
        kGameplayWorldViewport.y + kGameplayWorldViewport.height * 0.5f !=
            GameplayCamera::kCenterY)
        return false;
    for (const auto& size : {
             std::pair<int, int>{960, 540}, {1920, 1080},
             {1280, 800}, {800, 600}, {600, 800}}) {
        // In landscape the square's side equals the height of the actual
        // window; portrait windows cannot contain a height-wide square.
        const float scale = GameplayViewportScale(size.first, size.second);
        const float side = kGameplayWorldViewport.width * scale;
        const float expected = static_cast<float>(
            std::min(size.first, size.second));
        if (std::abs(side - expected) > 0.001f)
            return false;
        // The right-side statistics must never extend into invisible
        // letterbox/cropped space, even in a 4:3 window.
        const float rightWidth = GameplayRightSidebarWidth(
            size.first, size.second);
        const float projectedRight =
            (kGameplayWorldViewport.x +
             kGameplayWorldViewport.width + rightWidth -
             GameplayCamera::kCenterX) * scale +
            size.first * 0.5f;
        if (std::abs(projectedRight - size.first) > 0.01f ||
            rightWidth < 0.0f)
            return false;
    }
    const auto back = locus::app::detail::kEditorBack;
    const auto tab = locus::app::detail::kTabField;
    const auto exit = locus::app::HomeMenuButton(4);
    // Dedicated Back and Exit actions must stay visible and not overlap
    // the editor FIELD tab or the Home SETTINGS button.
    if (back.x < tab.x + tab.width ||
        back.x + back.width > kWindowWidth ||
        back.y + back.height > kWindowHeight ||
        exit.y < locus::app::HomeMenuButton(3).y +
                     locus::app::HomeMenuButton(3).height ||
        exit.y + exit.height > kWindowHeight)
        return false;
    const Rectangle left{52, 149, 408, 325};
    const Rectangle right{486, 149, 432, 325};
    const auto inside = [](Rectangle frame, Rectangle r) {
        return r.x >= frame.x && r.y >= frame.y &&
               r.x + r.width <= frame.x + frame.width &&
               r.y + r.height <= frame.y + frame.height;
    };
    const Rectangle leftControls[] = {
        locus::app::SettingsVolumeMinus(), locus::app::SettingsVolumePlus(),
        locus::app::SettingsFlashButton(),
        locus::app::SettingsChordMinus(), locus::app::SettingsChordPlus(),
        locus::app::SettingsFullscreenButton()
    };
    for (const auto control : leftControls)
        if (!inside(left, control)) return false;
    for (int i = 0; i < locus::app::kGameActionCount; ++i) {
        const auto button = locus::app::SettingsBindingButton(i);
        if (!inside(right, button)) return false;
        if (button.y + button.height >= 474.0f) return false;
        if (i > 0) {
            const auto previous = locus::app::SettingsBindingButton(i - 1);
            if (button.y < previous.y + previous.height) return false;
        }
    }
    // Player remains centered and speed must not depend on chart grid size.
    for (int dimensions : {2, 4, 8, 16}) {
        auto chart = locus::core::MakeDemoChart();
        chart.gridWidth = dimensions;
        chart.gridHeight = dimensions;
        for (double beat : {0.0, 1.5, 4.75, 15.125, 30.5}) {
            const GameplayCamera camera(chart, beat);
            const Vector2 projected = camera.Canvas().Position(camera.world);
            const float expected = locus::app::StepForFixedSpeed(
                locus::app::kDefaultBaseSpeed, chart.bpm, chart.ticksPerBeat);
            if (std::abs(projected.x - GameplayCamera::kCenterX) > 0.001f ||
                std::abs(projected.y - GameplayCamera::kCenterY) > 0.001f ||
                std::abs(camera.step - expected) > 0.001f)
                return false;
        }
    }
    auto longRoute = locus::core::MakeDemoChart();
    longRoute.lengthBeats = 65536;
    longRoute.turns.clear();
    longRoute.initialHeading = locus::core::Heading::UpRight;
    const GameplayCamera farCamera(longRoute, 65321.625);
    const auto farSample = farCamera.Canvas().Position(farCamera.world);
    if (std::abs(farSample.x - GameplayCamera::kCenterX) > 0.001f ||
        std::abs(farSample.y - GameplayCamera::kCenterY) > 0.001f)
        return false;
    return true;
}

// Headless checks exercise the same update function without starting raylib.
bool CheckLoopState() {
    AppState state;
    for (int i = 0; i < kSmokeFrames; ++i) {
        UpdateState(state, FrameInput{}, true);
        if (!state.running || state.frames != i + 1 || state.screen != Screen::Home) {
            return false;
        }
    }
    UpdateState(state, FrameInput{}, true);
    if (state.running || state.frames != kSmokeFrames) {
        return false;
    }

    AppState closedByUser;
    UpdateState(closedByUser, FrameInput{true, std::nullopt}, false);
    if (!closedByUser.running || !closedByUser.quitConfirmation ||
        closedByUser.frames != 1) return false;
    FrameInput cancel;
    cancel.quitCanceled = true;
    UpdateState(closedByUser, cancel, false);
    if (!closedByUser.running || closedByUser.quitConfirmation ||
        closedByUser.frames != 2) return false;
    UpdateState(closedByUser, FrameInput{true, std::nullopt}, false);
    FrameInput approve;
    approve.quitConfirmed = true;
    UpdateState(closedByUser, approve, false);
    return !closedByUser.running && closedByUser.frames == 3;
}

bool CheckScreenTransitions() {
    AppState state;
    if (state.screen != Screen::Home || std::strcmp(ScreenName(state.screen), "Home") != 0) {
        return false;
    }

    const Screen transitions[] = {Screen::SongSelect, Screen::Play,
                                  Screen::Editor, Screen::Skin,
                                  Screen::Settings, Screen::Results, Screen::Home};
    int expectedFrames = 0;
    for (Screen next : transitions) {
        UpdateState(state, FrameInput{false, next}, false);
        if (!state.running || state.screen != next || ++expectedFrames != state.frames) {
            return false;
        }
    }

    // No key press means no screen transition.
    UpdateState(state, FrameInput{}, false);
    if (state.screen != Screen::Home || state.frames != ++expectedFrames) {
        return false;
    }

    // An X/Alt+F4 or Home Esc requires approval, taking priority over
    // any simultaneous navigation. No wrong screen should open underneath.
    UpdateState(state, FrameInput{true, Screen::Editor}, false);
    if (!state.running || !state.quitConfirmation ||
        state.screen != Screen::Home || state.frames != ++expectedFrames)
        return false;
    UpdateState(state, FrameInput{false, Screen::Play}, false);
    if (state.screen != Screen::Home || !state.quitConfirmation ||
        state.frames != ++expectedFrames) return false;
    FrameInput cancel;
    cancel.quitCanceled = true;
    UpdateState(state, cancel, false);
    if (!state.running || state.quitConfirmation ||
        state.screen != Screen::Home || state.frames != ++expectedFrames)
        return false;
    // Back from Editor is an ordinary screen navigation, not a close.
    state.screen = Screen::Editor;
    FrameInput back;
    back.backPressed = true;
    UpdateState(state, back, false);
    if (!state.running || state.screen != Screen::Home ||
        state.quitConfirmation || state.frames != ++expectedFrames)
        return false;
    // The confirmation can reopen repeatedly; only Y terminates.
    UpdateState(state, FrameInput{true, std::nullopt}, false);
    if (!state.running || !state.quitConfirmation ||
        state.frames != ++expectedFrames) return false;
    FrameInput approve;
    approve.quitConfirmed = true;
    UpdateState(state, approve, false);
    return CheckPresentationLayout() &&
           !state.running && state.screen == Screen::Home &&
           state.frames == expectedFrames &&
           std::strcmp(ScreenName(Screen::Play), "Play") == 0 &&
           std::strcmp(ScreenName(Screen::Editor), "Editor") == 0 &&
           std::strcmp(ScreenName(Screen::Results), "Results") == 0;
}
// Headless session check: start, return Home, retry, resolve the demo,
// and retry from Results. Desktop and real-audio behavior need manual testing.
bool CheckSessionFlow() {
    AppState state;
    const locus::core::Chart chart = locus::core::MakeDemoChart();
    locus::core::TurnJudge judge(chart);

    FrameInput start;
    start.startPressed = true;
    UpdateState(state, start, false);
    if (state.screen != Screen::SongSelect) return false;
    FrameInput chooseDemo;
    chooseDemo.requestedScreen = Screen::Play;
    UpdateState(state, chooseDemo, false);
    if (state.screen != Screen::Play) return false;

    // A turn can be judged without ever changing authored movement.
    const auto baseline = locus::core::SampleRouteStep(chart, 8);
    judge.Press(locus::core::Turn::Right, 1.0);
    if (judge.Counts().perfect != 1) return false;

    FrameInput goHome;
    goHome.homePressed = true;
    UpdateState(state, goHome, false);
    if (state.screen != Screen::Home) return false;
    UpdateState(state, start, false);
    if (state.screen != Screen::SongSelect) return false;
    UpdateState(state, chooseDemo, false);
    if (state.screen != Screen::Play) return false;
    judge.Reset();
    if (judge.Counts().Resolved() != 0) return false;

    // Audio finishing is no longer the primary completion condition.
    // Even a very long song finishes at the instant the final note is
    // judged (both correct hits and late-window misses).
    if (ChartReadyForResults(judge, false)) return false;
    judge.Advance(14.819); // The last demo note is at 15 seconds.
    if (ChartReadyForResults(judge, false)) return false;
    // Beat 30 is the last turn (LEFT) at 15.0 seconds. A hit here
    // ends immediately; the synthesized demo click track lasts 16 s.
    const auto finalHit = judge.Press(locus::core::Turn::Left, 15.0);
    if (!finalHit || finalHit->grade != locus::core::Grade::Perfect ||
        !ChartReadyForResults(judge, false)) return false;
    FinishSession(state, judge);
    if (state.screen != Screen::Results ||
        judge.Counts().perfect != 1 ||
        judge.Counts().miss != static_cast<int>(chart.turns.size()) - 1 ||
        judge.Counts().Resolved() != static_cast<int>(chart.turns.size())) {
        return false;
    }
    const auto unchanged = locus::core::SampleRouteStep(chart, 8);
    if (baseline.from.x != unchanged.from.x ||
        baseline.from.y != unchanged.from.y ||
        baseline.to.x != unchanged.to.x ||
        baseline.to.y != unchanged.to.y ||
        baseline.heading != unchanged.heading) return false;

    FrameInput retry;
    retry.restartPressed = true;
    UpdateState(state, retry, false);
    if (state.screen != Screen::Play) return false;
    judge.Reset();
    if (judge.Counts().Resolved() != 0) return false;

    // Even when audio has just ended, a pending last note must keep its
    // full late-hit window; completion requires a real final judgement.
    judge.Advance(15.17);
    if (ChartReadyForResults(judge, true)) return false;
    judge.Advance(15.181);
    if (!ChartReadyForResults(judge, true) ||
        judge.Counts().miss != static_cast<int>(chart.turns.size()) ||
        judge.Counts().AccuracyPercent() != 0.0)
        return false;

    // An empty chart (or a TEST beginning after its final note) has no
    // scorable notes, so only there may the song end trigger Results.
    locus::core::Chart emptyChart;
    locus::core::TurnJudge noNotes(emptyChart);
    if (ChartReadyForResults(noNotes, false) ||
        !ChartReadyForResults(noNotes, true))
        return false;
    locus::core::TurnJudge noTestNotes(chart);
    noTestNotes.ResetFromBeat(31);
    if (noTestNotes.ScorableCount() != 0 ||
        ChartReadyForResults(noTestNotes, false) ||
        !ChartReadyForResults(noTestNotes, true))
        return false;

    // Closing the window blocks other navigation, and only a confirmed
    // exit may terminate the current game session.
    FrameInput close;
    close.closeRequested = true;
    close.requestedScreen = Screen::Home;
    UpdateState(state, close, false);
    if (!state.running || !state.quitConfirmation ||
        state.screen != Screen::Play) return false;
    FrameInput canceled;
    canceled.quitCanceled = true;
    UpdateState(state, canceled, false);
    if (!state.running || state.quitConfirmation ||
        state.screen != Screen::Play) return false;
    UpdateState(state, close, false);
    FrameInput confirmed;
    confirmed.quitConfirmed = true;
    UpdateState(state, confirmed, false);
    return !state.running && state.screen == Screen::Play;
}


} // namespace

int main(int argc, char** argv) {
    if (HasArgument(argc, argv, "--loop-check")) {
        if (!CheckLoopState()) {
            std::cerr << "LOOP_STATE_FAIL\n";
            return 1;
        }
        std::cout << "LOOP_STATE_PASS frames=" << kSmokeFrames << '\n';
        return 0;
    }
    if (HasArgument(argc, argv, "--screen-check")) {
        if (!CheckScreenTransitions()) {
            std::cerr << "SCREEN_STATE_FAIL\n";
            return 1;
        }
        std::cout << "SCREEN_STATE_PASS transitions=7\n";
        return 0;
    }
    if (HasArgument(argc, argv, "--session-check")) {
        if (!CheckSessionFlow()) {
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

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
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
    SetTargetFPS(60); // Rendering cadence never determines musical time.
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

    locus::app::QuitMediaCoordinator quitMedia;

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
        if (state.screen == Screen::Skin) {
            const auto applySkin = [&]() {
                std::string error;
                if (!locus::app::SaveSkin(activeSkinFile, draftSkin, error)) {
                    skinUi.notice = "SAVE FAILED: " + error;
                    skinUi.noticeError = true;
                    return;
                }
                activeSkin = draftSkin;
                skinUi.saved = activeSkin;
                skinUi.dirty = false;
                skinUi.notice = "SKIN SAVED AND APPLIED TO ALL CHARTS";
                skinUi.noticeError = false;
            };
            switch (menuEvents.skinCommand) {
                case locus::app::SkinCommand::Load: {
                    const auto picked = locus::app::ChooseSkinFile();
                    if (!picked.path.empty()) {
                        locus::app::Skin loaded;
                        std::string error;
                        if (locus::app::LoadSkin(picked.path, loaded, error)) {
                            draftSkin = loaded;
                            skinUi.dirty = true;
                            skinUi.notice = "SKIN LOADED - SAVE TO ACTIVATE";
                            skinUi.noticeError = false;
                        } else {
                            skinUi.notice = "LOAD FAILED: " + error;
                            skinUi.noticeError = true;
                        }
                    } else if (!picked.canceled) {
                        skinUi.notice = picked.error;
                        skinUi.noticeError = true;
                    }
                    break;
                }
                case locus::app::SkinCommand::Save:
                    applySkin();
                    break;
                case locus::app::SkinCommand::SaveAs: {
                    const auto picked = locus::app::ChooseSkinSaveFile(L"My-Skin.locus-skin.json");
                    if (!picked.path.empty()) {
                        std::string error;
                        if (locus::app::SaveSkin(picked.path, draftSkin, error))
                            applySkin();
                        else {
                            skinUi.notice = "EXPORT FAILED: " + error;
                            skinUi.noticeError = true;
                        }
                    } else if (!picked.canceled) {
                        skinUi.notice = picked.error;
                        skinUi.noticeError = true;
                    }
                    break;
                }
                case locus::app::SkinCommand::Reset:
                    draftSkin = locus::app::Skin{};
                    skinUi.dirty = true;
                    skinUi.notice = "DEFAULT SKIN - SAVE TO ACTIVATE";
                    skinUi.noticeError = false;
                    break;
                case locus::app::SkinCommand::None: break;
            }
        }
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
        if (previousScreen == Screen::Skin && state.screen != Screen::Skin) {
            // Back/Esc cancels all unsaved skin edits.
            draftSkin = activeSkin;
            skinUi.dirty = false;
            skinUi.editingName = false;
            skinUi.editingNumberRow = -1;
            skinUi.numberBuffer.clear();
            skinUi.draggingRow = -1;
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
            // File dialog is modal. Freeze playback while Windows owns focus,
            // and preserve the current song if selection fails or is canceled.
            const bool resume = state.screen == Screen::Listen &&
                                localSong.Playing() && !localSong.Paused();
            if (resume) localSong.Pause(now);
            const auto picked = locus::app::ChooseLocalMusicFile();
            now = MonotonicSeconds();
            if (!picked.path.empty()) {
                if (localSong.LoadFromFile(picked.path)) {
                    musicError.clear();
                    loadedAudioPath = picked.path;
                    state.screen = Screen::Listen;
                    loadedNewSong = true;
                } else {
                    musicError = localSong.Error();
                }
            } else if (!picked.canceled) {
                musicError = picked.error;
            }
            if (resume && !loadedNewSong) localSong.Resume(now);
        }

        bool loadedNewChart = false;
        if (state.screen == Screen::SongSelect) {
            if (menuEvents.refresh)
                locus::app::RefreshLibraryBrowser(library, libraryUi, menuStatus);
            if (menuEvents.import || importFromHome) {
                const auto selected = locus::app::ChooseChartFile();
                now = MonotonicSeconds();
                if (!selected.path.empty()) {
                    locus::app::ImportLibraryChart(
                        library, libraryUi, std::filesystem::path(selected.path),
                        menuStatus);
                } else if (!selected.canceled) {
                    libraryUi.notice = "IMPORT FAILED: " + selected.error;
                }
            }
            if (menuEvents.editSelected) {
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
        } else if (state.screen == Screen::Listen) {
            if (loadedNewSong || input.restartPressed ||
                (input.startPressed && localSong.Finished())) {
                localSong.Start(now);
            } else if (input.pausePressed) {
                if (localSong.Paused()) localSong.Resume(now);
                else localSong.Pause(now);
            }
        }

        if (state.screen == Screen::Listen) {
            localSong.Update(now);
        }
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
                    editorStatus, editorDiscard.Pending(), musicError, renderNow,
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
