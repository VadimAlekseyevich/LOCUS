#include "core/foundation.h"
#include "core/route.h"
#include "core/route_view.h"
#include "core/judgement.h"
#include "core/chart_json.h"
#include "app/demo_transport.h"
#include "app/local_music.h"
#include "app/chart_file.h"
#include "app/chart_playback.h"
#include "app/music_picker.h"
#include "app/chart_editor_ui.h"
#include "app/chart_filename.h"
#include "app/visual_style.h"
#include "app/route_renderer.h"
#include "app/game_session.h"
#include "app/song_library.h"
#include "app/game_settings.h"
#include "app/skin.h"
#include "app/skin_menu.h"
#include "app/menu_ui.h"

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
#include <vector>

// F-07: placeholder screens share one window and one input/update/render loop.
// Real screen controllers, routing, and musical timing arrive in later tasks.
namespace {
constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
constexpr int kSmokeFrames = 60;
constexpr Color kBackground = {21, 23, 33, 255};

double MonotonicSeconds() {
    using Clock = std::chrono::steady_clock;
    static const auto epoch = Clock::now();
    return std::chrono::duration<double>(Clock::now() - epoch).count();
}

enum class Screen {
    Home,
    Play,
    Editor,
    Results,
    Listen,
    SongSelect,
    Skin,
    Settings
};

struct FrameInput {
    bool closeRequested = false;
    std::optional<Screen> requestedScreen;
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
    std::array<bool, 4> arrowPressed{};
    std::array<double, 4> arrowCapturedAt{};
    std::array<bool, 4> arrowHeld{};
    locus::app::EditorControls editor;
};

enum class EditorPending { None, Home, Exit, NewDraft, OpenChart };

struct AppState {
    bool running = true;
    int frames = 0;
    Screen screen = Screen::Home;
};

bool HasArgument(int argc, char** argv, const char* argument) {
    return argc == 2 && std::strcmp(argv[1], argument) == 0;
}

const char* ScreenName(Screen screen) {
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

// 1. Input: raylib already pumps native events at the end of each drawn frame.
// F1-F4 are diagnostic shortcuts ONLY when --debug-screens is explicitly enabled.
FrameInput PollInput(bool debugScreens, Screen currentScreen,
                     const locus::app::GameSettings& settings) {
    FrameInput input;
    const bool escape = IsKeyPressed(KEY_ESCAPE);
    input.closeRequested = WindowShouldClose() ||
        (escape && (currentScreen == Screen::Home || currentScreen == Screen::Editor));
    input.backPressed = escape && currentScreen != Screen::Home &&
                        currentScreen != Screen::Editor;
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
        ? settings.Key(locus::app::GameAction::Home) : KEY_H);
    input.fullscreenPressed = IsKeyPressed(KEY_F11);
    input.chooseMusicPressed = IsKeyPressed(KEY_O);
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
    input.editorTest = IsKeyPressed(KEY_T);
    input.practiceSpeedIndex = IsKeyPressed(KEY_ONE) ? 0
                              : IsKeyPressed(KEY_TWO) ? 1
                              : IsKeyPressed(KEY_THREE) ? 2
                              : IsKeyPressed(KEY_FOUR) ? 3
                              : IsKeyPressed(KEY_FIVE) ? 4 : -1;
    // IsKeyPressed() captures fresh key-downs only; held keys never re-score.
    // Use the same steady clock origin as DemoTransport, not frame duration.
    // Absolute direction input; a diagonal is a two-key stroke.
    // The original four enum indices still map to Up, Right, Down, Left.
    for (int i = 0; i < 4; ++i) {
        const int key = settings.keys[static_cast<std::size_t>(i)];
        input.arrowPressed[i] = IsKeyPressed(key);
        input.arrowHeld[i] = IsKeyDown(key);
        if (input.arrowPressed[i]) input.arrowCapturedAt[i] = MonotonicSeconds();
    }
    input.editor = locus::app::PollEditorControls();
    return input;
}

// 2. Update: only state transitions here. No drawing or gameplay logic.
void UpdateState(AppState& state, const FrameInput& input, bool smokeWindow) {
    if (input.closeRequested || (smokeWindow && state.frames >= kSmokeFrames)) {
        state.running = false;
        return;
    }
    if (input.requestedScreen.has_value()) {
        state.screen = *input.requestedScreen;
    } else if ((input.homePressed || input.backPressed) &&
               state.screen != Screen::Home) {
        state.screen = Screen::Home;
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

// Resolve all remaining events at the end of the authored track, even if the
// player never pressed a key. Final results cannot alter the route.
void FinishSession(AppState& state, locus::core::TurnJudge& judge,
                   const locus::core::Chart& chart) {
    (void)chart; // Completion is defined by music duration, not the next whole beat.
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

// One presentation snapshot for both generated and local-chart sessions.
struct GameplayView {
    const locus::core::Chart& chart;
    const locus::core::TurnJudge& judge;
    const locus::app::HitFlash& hitFlash;
    const std::string& title;
    double songSeconds;
    double beat;
    bool paused;
    bool completed;
    bool custom;
    double practiceSpeed;
    const char* status;
    bool showHitFlash = true;
    const locus::app::GameSettings& bindings;
    const locus::app::Skin& skin;
};

// Fixed screen-centre avatar: only the world translates. The gameplay
// viewport is grid-free and the entire HUD is isolated in a bottom strip.
constexpr float kGameplayHudTop = 478.0f;
constexpr Rectangle kGameplayWorldViewport{0.0f, 0.0f, 960.0f, kGameplayHudTop};

struct GameplayCamera {
    static constexpr float kCenterX = kWindowWidth * 0.5f;
    static constexpr float kCenterY = kWindowHeight * 0.5f;
    static constexpr float kStep = 51.0f; // Less zoomed than the old 75px/beat.
    locus::core::GridPoint world{};
    float step = kStep;

    GameplayCamera(const locus::core::Chart& chart, double beat,
                   const locus::app::Skin& skin = locus::app::Skin{})
        : world(locus::core::RoutePositionAt(chart, beat)), step(skin.cameraStep) {}

    locus::app::RouteCanvas Canvas() const noexcept {
        return {kCenterX, kCenterY, step, world};
    }
};

void DrawPlayGrid(Font font, const GameplayView& gameplay, double now) {
    const GameplayCamera camera(gameplay.chart, gameplay.beat, gameplay.skin);
    const auto canvas = camera.Canvas();

    // Scissor coordinates are physical pixels while all world points remain
    // projected into the virtual 960x540 resolution.
    const float scale = std::min(static_cast<float>(GetScreenWidth()) / kWindowWidth,
                                 static_cast<float>(GetScreenHeight()) / kWindowHeight);
    const float offsetX = (GetScreenWidth() - kWindowWidth * scale) * 0.5f;
    const float offsetY = (GetScreenHeight() - kWindowHeight * scale) * 0.5f;
    BeginScissorMode(
        static_cast<int>(std::floor(offsetX + kGameplayWorldViewport.x * scale)),
        static_cast<int>(std::floor(offsetY + kGameplayWorldViewport.y * scale)),
        static_cast<int>(std::ceil(kGameplayWorldViewport.width * scale)),
        static_cast<int>(std::ceil(kGameplayWorldViewport.height * scale)));

    // PLAY and editor TEST: only a gradually thinning/fading red route
    // *ahead* of the marker, red turn targets and the centred avatar.
    // No past-route trail, grid or debug overlay.
    const Vector2 marker = locus::app::DrawRouteTrajectory(
        gameplay.chart, gameplay.beat, canvas,
        locus::app::RouteRenderMode::Gameplay, gameplay.skin);
    locus::app::DrawRouteJunctions(
        gameplay.chart, gameplay.beat, canvas,
        locus::app::RouteRenderMode::Gameplay, gameplay.skin);
    const float markerRadius = gameplay.skin.playerRadius;
    if (gameplay.skin.playerGlow)
        DrawCircleV(marker, markerRadius * 1.65f,
                    locus::app::WithOpacity(gameplay.skin.player, 0.22f));
    DrawCircleV(marker, markerRadius, gameplay.skin.player);
    DrawCircleV(marker, markerRadius * 0.33f, gameplay.skin.playerCore);
    if (gameplay.showHitFlash) {
        locus::app::DrawPlayerHitFlash(
            marker, markerRadius, gameplay.hitFlash.Sample(now), gameplay.skin);
    }
    EndScissorMode();

    // The HUD stays separate from the playing field and can be hidden.
    if (!gameplay.skin.showHud) return;
    DrawRectangle(0, static_cast<int>(kGameplayHudTop), kWindowWidth,
                  kWindowHeight - static_cast<int>(kGameplayHudTop),
                  gameplay.skin.hudBackground);
    DrawLine(0, static_cast<int>(kGameplayHudTop),
             kWindowWidth, static_cast<int>(kGameplayHudTop),
             Color{47, 57, 73, 255});

    const auto& counts = gameplay.judge.Counts();
    const struct {
        const char* label;
        int count;
        float x;
        Color color;
    } stats[] = {
        {"PERFECT", counts.perfect, 42.0f, {133, 236, 201, 255}},
        {"GOOD", counts.good, 224.0f, {143, 208, 239, 255}},
        {"BAD", counts.bad, 401.0f, {242, 198, 135, 255}},
        {"MISS", counts.miss, 558.0f, {237, 142, 149, 255}}
    };
    for (const auto& stat : stats) {
        DrawTextLabel(font, stat.label, {stat.x, 488.0f}, 15.0f, stat.color);
        DrawTextLabel(font, TextFormat("%d", stat.count),
                      {stat.x, 508.0f}, 22.0f,
                      gameplay.skin.hudText);
    }
    DrawLine(750, 488, 750, 531, Color{47, 57, 73, 255});
    DrawTextLabel(font, "LAST", {775.0f, 488.0f}, 14.0f,
                  Color{164, 188, 207, 255});
    if (const auto& last = gameplay.judge.Last()) {
        const Color color = last->grade == locus::core::Grade::Perfect
                                ? Color{133, 236, 201, 255}
                            : last->grade == locus::core::Grade::Good
                                ? Color{143, 208, 239, 255}
                            : last->grade == locus::core::Grade::Bad
                                ? Color{242, 198, 135, 255}
                                : Color{237, 142, 149, 255};
        DrawTextLabel(font, locus::core::GradeName(last->grade),
                      {775.0f, 509.0f}, 20.0f, color);
    }
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

void DrawResultsScreen(Font font, const GameplayView& gameplay) {
    const auto& judge = gameplay.judge;
    const auto& demo = gameplay.chart;
    const bool completed = gameplay.completed;
    DrawTextLabel(font, completed
                            ? (gameplay.custom ? "CHART COMPLETE" : "DEMO COMPLETE")
                            : "RESULTS PREVIEW",
                  Vector2{100.0f, 111.0f}, 41.0f,
                  Color{195, 237, 232, 255});
    DrawTextLabel(font, gameplay.title.c_str(), Vector2{570.0f, 180.0f},
                  18.0f, Color{170, 215, 222, 255});
    const auto& counts = judge.Counts();
    DrawTextLabel(font, TextFormat("PERFECT   %d", counts.perfect),
                  Vector2{110.0f, 206.0f}, 27.0f,
                  Color{128, 238, 204, 255});
    DrawTextLabel(font, TextFormat("GOOD      %d", counts.good),
                  Vector2{110.0f, 250.0f}, 27.0f,
                  Color{147, 215, 245, 255});
    DrawTextLabel(font, TextFormat("BAD       %d", counts.bad),
                  Vector2{110.0f, 294.0f}, 27.0f,
                  Color{240, 197, 138, 255});
    DrawTextLabel(font, TextFormat("MISS      %d", counts.miss),
                  Vector2{110.0f, 338.0f}, 27.0f,
                  Color{243, 150, 151, 255});
    DrawTextLabel(font,
                  TextFormat("TURNS RESOLVED  %d / %d", counts.Resolved(),
                             static_cast<int>(demo.turns.size())),
                  Vector2{570.0f, 227.0f}, 22.0f,
                  Color{204, 225, 228, 255});
    DrawTextLabel(font, "NO FAIL  -  ROUTE UNCHANGED",
                  Vector2{570.0f, 269.0f}, 17.0f,
                  Color{158, 193, 204, 255});
    DrawTextLabel(font, "ENTER / R  -  PLAY AGAIN",
                  Vector2{110.0f, 409.0f}, 24.0f,
                  Color{236, 219, 160, 255});
    DrawTextLabel(font, "H  -  HOME",
                  Vector2{110.0f, 447.0f}, 22.0f,
                  Color{187, 205, 220, 255});
}

// Menu controls use the same centered 960x540 coordinate space as drawing.
// Keep Home/Play/Skin/Settings interactions separate from the editor handlers.
struct MenuEvents {
    bool import = false;
    bool refresh = false;
    bool playSelected = false;
    bool editSelected = false;
    bool saveSettings = false;
    bool toggleFullscreen = false;
    locus::app::SkinCommand skinCommand = locus::app::SkinCommand::None;
};

MenuEvents HandleMenuInput(const AppState& state, FrameInput& input,
                           const locus::app::SongLibrary& library,
                           locus::app::SongSelectUi& libraryUi,
                           locus::app::GameSettings& settings,
                           locus::app::SettingsUi& settingsUi,
                           locus::app::Skin& draftSkin,
                           locus::app::SkinUi& skinUi) {
    MenuEvents events;
    const bool mouseClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const float scale = std::min(static_cast<float>(GetScreenWidth()) / 960.0f,
                                 static_cast<float>(GetScreenHeight()) / 540.0f);
    const bool validMouse = scale > 0.0f;
    const Vector2 physical = GetMousePosition();
    const Vector2 mouse = validMouse
        ? Vector2{(physical.x - (GetScreenWidth() - 960.0f * scale) / 2.0f) / scale,
                  (physical.y - (GetScreenHeight() - 540.0f * scale) / 2.0f) / scale}
        : Vector2{-1000.0f, -1000.0f};

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

    if (state.screen == Screen::SongSelect) {
        if (IsKeyPressed(KEY_UP)) libraryUi.MoveSong(-1, library);
        if (IsKeyPressed(KEY_DOWN)) libraryUi.MoveSong(1, library);
        if (IsKeyPressed(KEY_LEFT)) libraryUi.MoveChart(-1, library);
        if (IsKeyPressed(KEY_RIGHT)) libraryUi.MoveChart(1, library);
        if (IsKeyPressed(KEY_F5)) events.refresh = true;
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
            libraryUi.MoveSong(wheel > 0.0f ? -1 : 1, library);
        if (input.chooseChartPressed) events.import = true;
        if (input.startPressed) events.playSelected = true;
        if (IsKeyPressed(KEY_E)) events.editSelected = true;
    }

    if (!mouseClick || !validMouse) return events;
    const auto hit = [&](Rectangle area) {
        return CheckCollisionPointRec(mouse, area);
    };

    if (state.screen == Screen::Home) {
        for (int i = 0; i < 4; ++i) {
            if (!hit(locus::app::HomeMenuButton(i))) continue;
            switch (i) {
                case 0: input.requestedScreen = Screen::SongSelect; break;
                case 1: input.requestedScreen = Screen::Editor; break;
                case 2: input.requestedScreen = Screen::Skin; break;
                case 3: input.requestedScreen = Screen::Settings; break;
            }
            return events;
        }
    } else if (state.screen == Screen::SongSelect) {
        if (hit(locus::app::MenuBackButton())) input.backPressed = true;
        if (hit(locus::app::LibraryImportButton())) events.import = true;
        if (hit(locus::app::LibraryRefreshButton())) events.refresh = true;
        if (hit(locus::app::LibraryPlayButton())) events.playSelected = true;
        if (hit(locus::app::LibraryEditButton())) events.editSelected = true;
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
        const auto changeVolume = [&](int delta) {
            const int current = static_cast<int>(std::lround(settings.volume * 10.0f));
            settings.volume = std::clamp(current + delta, 0, 10) / 10.0f;
            events.saveSettings = true;
        };
        if (hit(locus::app::SettingsVolumeMinus())) changeVolume(-1);
        else if (hit(locus::app::SettingsVolumePlus())) changeVolume(1);
        else if (hit(locus::app::SettingsFlashButton())) {
            settings.hitFlash = !settings.hitFlash;
            events.saveSettings = true;
        } else if (hit(locus::app::SettingsChordMinus())) {
            settings.chordWindowMs = std::max(40, settings.chordWindowMs - 10);
            events.saveSettings = true;
        } else if (hit(locus::app::SettingsChordPlus())) {
            settings.chordWindowMs = std::min(160, settings.chordWindowMs + 10);
            events.saveSettings = true;
        } else if (hit(locus::app::SettingsFullscreenButton())) {
            settings.fullscreen = !settings.fullscreen;
            events.toggleFullscreen = true;
            events.saveSettings = true;
        }
    }
    return events;
}

// 3. Render: display the selected state; never choose or modify it here.
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

    // Keep UI and the authored game field at the original 16:9 proportions.
    // Resizing/maximizing/fullscreen simply fits and centers this canvas.
    const float scale = std::min(static_cast<float>(GetScreenWidth()) / kWindowWidth,
                                 static_cast<float>(GetScreenHeight()) / kWindowHeight);
    Camera2D camera{};
    camera.offset = Vector2{(GetScreenWidth() - kWindowWidth * scale) / 2.0f,
                            (GetScreenHeight() - kWindowHeight * scale) / 2.0f};
    camera.target = Vector2{0.0f, 0.0f};
    camera.zoom = scale;
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
                                     selectedBeat, editorTransport.Speed());
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
        DrawTextLabel(font, "Placeholder screen - game functionality is not implemented",
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
    EndMode2D();
    EndDrawing();
}

// Keep rendering-only layout regressions in the headless screen check.
// This uses the same rectangles as the visible controls and hit testing.
bool CheckPresentationLayout() {
    // World is truly screen-centred, and scoring is on a separate strip.
    if (GameplayCamera::kCenterX != kWindowWidth * 0.5f ||
        GameplayCamera::kCenterY != kWindowHeight * 0.5f ||
        GameplayCamera::kCenterY >= kGameplayHudTop ||
        kGameplayWorldViewport.height != kGameplayHudTop ||
        kGameplayWorldViewport.width != kWindowWidth)
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
    // A moving world MUST project the interpolated marker to exactly the
    // same screen point regardless of chart size, heading or beat.
    for (int dimensions : {2, 4, 8, 16}) {
        auto chart = locus::core::MakeDemoChart();
        chart.gridWidth = dimensions;
        chart.gridHeight = dimensions;
        for (double beat : {0.0, 1.5, 4.75, 15.125, 30.5}) {
            const GameplayCamera camera(chart, beat);
            const Vector2 projected = camera.Canvas().Position(camera.world);
            if (std::abs(projected.x - GameplayCamera::kCenterX) > 0.001f ||
                std::abs(projected.y - GameplayCamera::kCenterY) > 0.001f)
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
    return !closedByUser.running && closedByUser.frames == 0;
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

    // Closing takes precedence over a simultaneous screen-switch request.
    UpdateState(state, FrameInput{true, Screen::Editor}, false);
    return CheckPresentationLayout() &&
           !state.running && state.screen == Screen::Home &&
           state.frames == expectedFrames &&
           std::strcmp(ScreenName(Screen::Play), "Play") == 0 &&
           std::strcmp(ScreenName(Screen::Editor), "Editor") == 0 &&
           std::strcmp(ScreenName(Screen::Results), "Results") == 0;
}
// Headless G-06 check: start, return Home, retry, resolve the demo, retry
// from Results. The real music device/presentation is tested by owner in G-07.
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

    // A clean run resolves every missing turn at song end and shows Results.
    FinishSession(state, judge, chart);
    if (state.screen != Screen::Results ||
        judge.Counts().miss != static_cast<int>(chart.turns.size()) ||
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

    // Closing the window wins even if a screen change is requested.
    FrameInput close;
    close.closeRequested = true;
    close.requestedScreen = Screen::Home;
    UpdateState(state, close, false);
    return !state.running && state.screen == Screen::Play;
}

// M-04: reuse the same validation and local-music stream as external chart
// playback. Saving/opening never silently changes the active Play session.
bool LoadDraftFromDisk(const std::wstring& file, locus::core::ChartEditor& editor,
                       locus::app::LocalMusic& song, std::wstring& chartPath,
                       std::wstring& audioPath, std::string& error) {
    locus::core::LoadedChart candidate;
    std::wstring selectedAudio;
    if (!locus::app::OpenChartWithAudioPath(file, candidate, selectedAudio, error))
        return false;
    if (!song.LoadFromFile(selectedAudio)) {
        error = song.Error();
        return false;
    }
    if (!locus::core::PrepareChartForSong(candidate, song.DurationSeconds(), error))
        return false;
    editor.Load(candidate);
    editor.SetSongDuration(song.DurationSeconds());
    chartPath = file;
    audioPath = selectedAudio;
    error.clear();
    return true;
}

bool SaveDraftToDisk(locus::core::ChartEditor& editor,
                     const locus::app::LocalMusic& song,
                     std::wstring& chartPath, const std::wstring& audioPath,
                     bool saveAs, std::string& error) {
    if (audioPath.empty() || !song.Ready()) {
        error = "PRESS O TO ATTACH YOUR WAV / MP3 FIRST";
        return false;
    }
    locus::core::LoadedChart checked = editor.Data();
    if (!locus::core::PrepareChartForSong(checked, song.DurationSeconds(), error))
        return false;

    std::wstring destination = chartPath;
    if (destination.empty() || saveAs) {
        // Use the authored chart title as the filename suggestion, not
        // the unrelated audio filename or a perpetual "Untitled" default.
        // Ctrl+S for a previously saved chart still uses its existing path.
        const auto folder = destination.empty()
            ? std::filesystem::path(audioPath).parent_path()
            : std::filesystem::path(destination).parent_path();
        const std::wstring suggestion =
            (folder / locus::app::SuggestedChartFileName(checked.title)).wstring();
        const auto selected = locus::app::ChooseChartSaveFile(suggestion);
        if (selected.path.empty()) {
            error = selected.canceled ? "" : selected.error;
            return false;
        }
        destination = selected.path;
    }
    if (!locus::app::SaveChartWithAudioPath(destination, audioPath, checked, error))
        return false;

    // Reopen exactly the bytes that were saved, preserving editor semantics
    // including audioPath relative to the new location and authored length.
    locus::core::LoadedChart reopened;
    std::wstring resolvedSong;
    if (!locus::app::OpenChartWithAudioPath(
            destination, reopened, resolvedSong, error) ||
        !locus::core::PrepareChartForSong(reopened, song.DurationSeconds(), error))
        return false;
    editor.Load(reopened);
    editor.SetSongDuration(song.DurationSeconds());
    chartPath = destination;
    error.clear();
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (HasArgument(argc, argv, "--foundation-check")) {
        std::cout << locus::core::FoundationStatus() << '\n';
        return 0;
    }
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
        std::cerr << "Usage: locus.exe [--foundation-check | --loop-check | --screen-check | "
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
    SetExitKey(KEY_NULL); // Route Esc through unsaved-editor confirmation.
    UiFont uiFont = LoadUiFont();
    SetTargetFPS(60); // Visual smoke baseline only, never a musical clock.
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
    locus::app::DemoTransport transport;
    locus::app::LocalMusic localSong;
    locus::app::ChartPlayback chartPlayback(localSong);
    locus::app::GameSession gameSession(activeChart, judge, transport, chartPlayback);
    bool customChart = false;
    std::wstring editorChartPath;
    std::wstring editorAudioPath;
    std::wstring loadedAudioPath; // Reuse already-decoded audio for each difficulty.
    EditorPending editorPending = EditorPending::None;
    std::string editorStatus = "O ATTACH AUDIO  |  CTRL+S SAVE  |  T TEST PLAY";
    int editorPreviewStartBeat = 0; // R returns to this preview origin.
    std::string chartTitle = "BUILT-IN DEMO";
    std::string musicError;
    transport.Initialize(demo); // Owns the shared raylib audio output device.
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

    while (state.running) {
        FrameInput input = PollInput(debugScreens, state.screen, settings);
        const bool importFromHome =
            state.screen == Screen::Home && input.chooseChartPressed;
        const MenuEvents menuEvents = HandleMenuInput(
            state, input, library, libraryUi, settings, settingsUi,
            draftSkin, skinUi);
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
            input.closeRequested = false;
            input.requestedScreen.reset();
            input.editorNew = false;
            input.editorOpen = false;
            input.editorSave = false;
            input.editorSaveAs = false;
            input.chooseMusicPressed = false;
            input.editorTest = false;
        }
        if (input.fullscreenPressed) {
            ToggleBorderlessWindowed(); // F11: fullscreen desktop / resizable window
            settings.fullscreen = !settings.fullscreen;
            persistSettings();
        }
        const Screen previousScreen = state.screen;
        bool editorModalHandled = false;
        if (previousScreen == Screen::Editor) {
            if (editorPending != EditorPending::None) {
                editorModalHandled = true;
                const EditorPending requested = editorPending;
                if (IsKeyPressed(KEY_Y)) {
                    editorPending = EditorPending::None;
                    if (requested == EditorPending::Home) input.homePressed = true;
                    if (requested == EditorPending::Exit) input.closeRequested = true;
                    if (requested == EditorPending::NewDraft) input.editorNew = true;
                    if (requested == EditorPending::OpenChart) input.editorOpen = true;
                } else if (IsKeyPressed(KEY_N)) {
                    editorPending = EditorPending::None;
                    editorStatus = "CHANGES KEPT - CTRL+S TO SAVE";
                }
                if (editorPending != EditorPending::None ||
                    !IsKeyPressed(KEY_Y)) {
                    input.homePressed = false;
                    input.closeRequested = false;
                    input.editorNew = false;
                    input.editorOpen = false;
                    input.requestedScreen.reset();
                }
            } else if (editor.Dirty() &&
                       (input.closeRequested || input.homePressed ||
                        input.editorNew || input.editorOpen ||
                        (input.requestedScreen &&
                         *input.requestedScreen != Screen::Editor))) {
                editorPending = input.closeRequested ? EditorPending::Exit
                              : input.editorNew ? EditorPending::NewDraft
                              : input.editorOpen ? EditorPending::OpenChart
                                                 : EditorPending::Home;
                input.closeRequested = false;
                input.homePressed = false;
                input.editorNew = false;
                input.editorOpen = false;
                input.requestedScreen.reset();
                editorModalHandled = true;
            }
        }
        UpdateState(state, input, smokeWindow);
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
        bool loadedNewSong = false;
        // Home uses E to enter Editor and E also rotates its heading.
        // Ignore that initial key-down; start editing on the next frame.
        if (state.screen == Screen::Editor &&
            previousScreen == Screen::Editor &&
            editorPending == EditorPending::None) {
            const int beforeBeat = editor.SelectedBeat();
            // Use the fractional transport beat for timeline hit testing,
            // matching the animated ruler even between whole beat markers.
            input.editor.timelineViewBeat = chartPlayback.Playing()
                ? chartPlayback.Beat(now)
                : static_cast<double>(editor.SelectedBeat());
            const auto uiAction = locus::app::ApplyEditorControls(
                editor, editorUi, input.editor);
            if (uiAction.chooseAudio) input.chooseMusicPressed = true;
            if (uiAction.testPlay) input.editorTest = true;

            const double speeds[] = {1.0, 0.75, 0.5, 0.25, 0.1};
            if (uiAction.speedIndex >= 0 && uiAction.speedIndex < 5) {
                chartPlayback.SetSpeed(speeds[uiAction.speedIndex], now);
                editorStatus = "PREVIEW SPEED CHANGED - AUDIO PITCH WILL ALSO CHANGE";
            }
            const bool selectedAnotherBeat = editor.SelectedBeat() != beforeBeat;
            if (selectedAnotherBeat) editorPreviewStartBeat = editor.SelectedBeat();
            if ((uiAction.changedChart || selectedAnotherBeat) &&
                (chartPlayback.Playing() || chartPlayback.Finished())) {
                chartPlayback.Stop();
                editorStatus = "PREVIEW STOPPED - SPACE PLAYS FROM SELECTED BEAT";
            }

            const auto startPreview = [&](int beat) {
                if (!localSong.Ready() || editorAudioPath.empty()) {
                    editorStatus = "NO SONG - CLICK LOAD SONG / PRESS O";
                    return;
                }
                // Preserve virtual negative-time preroll for both the
                // original negative offset and a partially overlapping
                // first beat before physical song time zero.
                const double songTime = editor.AudioSecondsAtBeat(beat);
                if (!std::isfinite(songTime) ||
                    songTime >= localSong.DurationSeconds()) {
                    editorStatus = "PREVIEW FAILED: SELECTED BEAT / OFFSET IS AFTER SONG END";
                    return;
                }
                chartPlayback.Configure(editor.Data().route.bpm,
                                        editor.Data().offsetSeconds);
                if (!chartPlayback.StartAtAudioSeconds(songTime, now)) {
                    editorStatus = "PREVIEW FAILED: COULD NOT START AUDIO";
                    return;
                }
                editorPreviewStartBeat = beat;
                editorStatus = "PREVIEW FROM BEAT " + std::to_string(beat) +
                               " / SONG " + std::to_string(songTime) +
                               "S - SPACE PAUSE / R RESTART";
            };
            if (uiAction.previewRestart) {
                startPreview(editorPreviewStartBeat);
            } else if (uiAction.previewToggle) {
                if (!chartPlayback.Playing()) startPreview(editor.SelectedBeat());
                else if (chartPlayback.Paused()) chartPlayback.Resume(now);
                else chartPlayback.Pause(now);
            }

            if (input.editorNew) {
                chartPlayback.Stop();
                localSong.Shutdown();
                loadedAudioPath.clear();
                editor.NewDraft();
                editorUi = locus::app::EditorUiState{};
                editorChartPath.clear();
                editorAudioPath.clear();
                editorPreviewStartBeat = 0;
                editorStatus = "NEW CHART - CLICK LOAD SONG";
            } else if (input.editorOpen) {
                chartPlayback.Stop();
                const auto picked = locus::app::ChooseChartFile();
                now = MonotonicSeconds();
                if (!picked.path.empty()) {
                    if (LoadDraftFromDisk(picked.path, editor, localSong,
                                          editorChartPath, editorAudioPath, musicError)) {
                        editorUi = locus::app::EditorUiState{};
                        editorPreviewStartBeat = 0;
                        loadedAudioPath = editorAudioPath;
                        editorStatus = "CHART LOADED - SPACE PREVIEW / T TEST";
                    } else {
                        editorStatus = "OPEN FAILED: " + musicError;
                    }
                } else if (!picked.canceled) {
                    editorStatus = "OPEN FAILED: " + picked.error;
                }
            } else if (input.chooseMusicPressed) {
                chartPlayback.Stop();
                const auto picked = locus::app::ChooseLocalMusicFile();
                now = MonotonicSeconds();
                if (!picked.path.empty()) {
                    if (localSong.LoadFromFile(picked.path)) {
                        const bool timelineOk =
                            editor.SetSongDuration(localSong.DurationSeconds());
                        editorAudioPath = picked.path;
                        loadedAudioPath = picked.path;
                        const std::string name =
                            std::filesystem::path(picked.path).filename().u8string();
                        editor.SetAudioPath(name);
                        editorStatus = timelineOk
                            ? "SONG LOADED - TIMELINE FITS THE WHOLE TRACK"
                            : "SONG LOADED - BPM / OFFSET EXCEEDS TIMELINE LIMIT";
                    } else {
                        editorStatus = "SONG FAILED: " + localSong.Error();
                    }
                } else if (!picked.canceled) {
                    editorStatus = "SONG PICKER FAILED: " + picked.error;
                }
            } else if (input.editorSave || input.editorSaveAs) {
                if (chartPlayback.Playing()) chartPlayback.Pause(now);
                std::string failure;
                if (SaveDraftToDisk(editor, localSong, editorChartPath,
                                    editorAudioPath, input.editorSaveAs, failure)) {
                    editorStatus = "SAVED - SPACE PREVIEW / T TEST / CTRL+O OPEN";
                    // A save remains in the user's chosen folder, but also
                    // installs/updates the permanent Songs library copy.
                    std::filesystem::path installed;
                    std::string installError;
                    if (library.Import(std::filesystem::path(editorChartPath),
                                       installed, installError)) {
                        libraryUi.notice = "SAVED CHART INSTALLED IN SONG LIBRARY";
                        menuStatus = "LIBRARY: " + std::to_string(library.Songs().size()) +
                                     " SONGS / " + std::to_string(library.ChartCount()) +
                                     " CHARTS";
                    } else {
                        libraryUi.notice = "LIBRARY IMPORT FAILED: " + installError;
                        editorStatus = "SAVED (LIBRARY IMPORT FAILED - SEE PLAY)";
                    }
                } else if (!failure.empty()) {
                    editorStatus = "SAVE FAILED: " + failure;
                }
            } else if (input.editorTest) {
                // Practice starts from the exact paused/current audio position.
                // If not previewing, use the selected timeline beat.
                const double editorSongPosition = chartPlayback.Playing()
                    ? chartPlayback.AudioSeconds(now)
                    : editor.AudioSecondsAtBeat(editor.SelectedBeat());
                chartPlayback.Stop();
                locus::core::LoadedChart checked = editor.Data();
                if (editorAudioPath.empty() || !localSong.Ready()) {
                    editorStatus = "TEST NEEDS AUDIO - CLICK LOAD SONG";
                } else if (!locus::core::PrepareChartForSong(
                               checked, localSong.DurationSeconds(), musicError)) {
                    editorStatus = "TEST FAILED: " + musicError;
                } else {
                    if (!std::isfinite(editorSongPosition) ||
                        editorSongPosition >= localSong.DurationSeconds()) {
                        editorStatus = "TEST FAILED: SELECT A POSITION BEFORE SONG END";
                    } else {
                        const double exactBeat = (editorSongPosition - checked.offsetSeconds)
                            * checked.route.bpm / 60.0;
                        const int firstScored = std::clamp(
                            static_cast<int>(std::ceil(exactBeat - 1e-6)),
                            checked.route.firstBeat, checked.route.lengthBeats);
                        activeChart = std::move(checked.route);
                        chartTitle = std::move(checked.title);
                        chartPlayback.Configure(activeChart.bpm, checked.offsetSeconds);
                        judge.SetOffsetSeconds(checked.offsetSeconds);
                        gameSession.SetTestOrigin({editorSongPosition, firstScored});
                        customChart = true;
                        state.screen = Screen::Play;
                        editorStatus = "TEST FROM BEAT " + std::to_string(exactBeat) +
                                       " - R RETRIES FROM HERE";
                    }
                }
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
            if (menuEvents.refresh) {
                if (!library.Refresh(libraryUi.notice)) {
                    menuStatus = libraryUi.notice;
                } else {
                    menuStatus = libraryUi.notice;
                    libraryUi.SelectSong(libraryUi.selectedSong, library);
                    libraryUi.SelectChart(libraryUi.selectedChart, library);
                }
            }
            if (menuEvents.import || importFromHome) {
                const auto selected = locus::app::ChooseChartFile();
                now = MonotonicSeconds();
                if (!selected.path.empty()) {
                    std::filesystem::path installed;
                    std::string error;
                    if (library.Import(std::filesystem::path(selected.path),
                                       installed, error)) {
                        int song = -1, chart = -1;
                        if (library.FindChart(installed, song, chart)) {
                            libraryUi.SelectSong(song + 1, library);
                            libraryUi.SelectChart(chart, library);
                        }
                        libraryUi.notice = "IMPORTED - SONG AND CHART SAVED PERMANENTLY";
                        menuStatus = "LIBRARY: " +
                            std::to_string(library.Songs().size()) + " SONGS / " +
                            std::to_string(library.ChartCount()) + " CHARTS";
                    } else {
                        libraryUi.notice = "IMPORT FAILED: " + error;
                    }
                } else if (!selected.canceled) {
                    libraryUi.notice = "IMPORT FAILED: " + selected.error;
                }
            }
            if (menuEvents.editSelected) {
                const auto* selected = libraryUi.Selected(library);
                if (!selected) {
                    state.screen = Screen::Editor; // Demo uses the existing draft.
                } else {
                    chartPlayback.Stop();
                    if (LoadDraftFromDisk(selected->file.wstring(), editor, localSong,
                                          editorChartPath, editorAudioPath, musicError)) {
                        loadedAudioPath = editorAudioPath;
                        editorUi = locus::app::EditorUiState{};
                        editorPreviewStartBeat = editor.SelectedBeat();
                        editorStatus = "LIBRARY CHART READY - EDIT AND CTRL+S";
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
                    const auto& sourceSong =
                        library.Songs()[static_cast<std::size_t>(
                            libraryUi.selectedSong - 1)];
                    const std::wstring audioPath = sourceSong.audioFile.wstring();
                    locus::core::LoadedChart candidate = selected->data;
                    if ((!localSong.Ready() || loadedAudioPath != audioPath) &&
                        !localSong.LoadFromFile(audioPath)) {
                        libraryUi.notice = "PLAY FAILED: " + localSong.Error();
                    } else {
                        loadedAudioPath = audioPath;
                        if (!locus::core::PrepareChartForSong(
                                candidate, localSong.DurationSeconds(), musicError)) {
                            libraryUi.notice = "PLAY FAILED: " + musicError;
                        } else {
                            activeChart = std::move(candidate.route);
                            chartTitle = std::move(candidate.title);
                            chartPlayback.Configure(activeChart.bpm,
                                                    candidate.offsetSeconds);
                            chartPlayback.SetSpeed(1.0, now);
                            gameSession.ClearTestOrigin();
                            judge.SetOffsetSeconds(candidate.offsetSeconds);
                            customChart = true;
                            loadedNewChart = true;
                            state.screen = Screen::Play;
                            musicError.clear();
                        }
                    }
                }
            }
        }

        if (state.screen != previousScreen) {
            if (previousScreen == Screen::Play) {
                gameSession.Stop(customChart);
            }
            if (previousScreen == Screen::Listen) localSong.Stop();
            if (previousScreen == Screen::Editor && state.screen != Screen::Editor)
                chartPlayback.Stop();
            if (state.screen == Screen::Play) {
                if (previousScreen != Screen::Results && !loadedNewChart &&
                    previousScreen != Screen::Editor) {
                    // Enter on Home always selects the built-in original demo;
                    // replay from Results keeps the chart just completed.
                    customChart = false;
                    activeChart = demo;
                    chartTitle = "BUILT-IN DEMO";
                    gameSession.ClearTestOrigin();
                    judge.SetOffsetSeconds(0.0);
                } else {
                    judge.Reset();
                }
                gameSession.Start(customChart, now);
            } else if (state.screen == Screen::Listen) {
                localSong.SetSpeed(1.0, now); // Preview slowdown must not leak to Listen.
                localSong.Start(now);
            }
        } else if (state.screen == Screen::Play) {
            if (customChart && input.practiceSpeedIndex >= 0) {
                const double speeds[] = {1.0, 0.75, 0.5, 0.25, 0.1};
                gameSession.SetPracticeSpeed(customChart, speeds[input.practiceSpeedIndex], now);
            }
            if (input.restartPressed ||
                (input.requestedScreen && *input.requestedScreen == Screen::Play)) {
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
        if (state.screen == Screen::Editor && chartPlayback.Playing()) {
            chartPlayback.Update(now);
            if (chartPlayback.Finished()) {
                editorStatus = "PREVIEW FINISHED - SELECT BEAT OR R TO RESTART";
            } else if (!chartPlayback.Paused()) {
                const double currentBeat = chartPlayback.Beat(now);
                const int lastBeat = editor.Data().route.lengthBeats - 1;
                if (currentBeat >= editor.Data().route.lengthBeats) {
                    chartPlayback.Stop();
                    editor.SelectBeat(lastBeat);
                    editorStatus = "PREVIEW REACHED CHART END - SELECT ANOTHER BEAT";
                } else {
                    const int beat = std::clamp(
                        static_cast<int>(std::floor(currentBeat)),
                        editor.Data().route.firstBeat, lastBeat);
                    editor.SelectBeat(beat); // Follow audio with bottom squares.
                }
            }
        }
        if (state.screen == Screen::Play) {
            gameSession.Update(customChart, now,
                               {input.arrowPressed, input.arrowCapturedAt, input.arrowHeld});
            if (customChart ? chartPlayback.Finished() : transport.Finished()) {
                FinishSession(state, judge, activeChart);
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
                                    paused, finished, customChart,
                                    customChart ? chartPlayback.Speed() : 1.0,
                                    status, settings.hitFlash, settings, activeSkin};
        RenderFrame(state, debugScreens, uiFont.value, demo,
                    gameplay, localSong, editor, editorUi, chartPlayback,
                    editorStatus, editorPending, musicError, renderNow,
                    library, libraryUi, settings, settingsUi,
                    draftSkin, skinUi, menuStatus);
    }

    // Unload both music streams before the shared raylib audio device.
    chartPlayback.Stop();
    localSong.Shutdown();
    transport.Shutdown();
    if (uiFont.owned) {
        UnloadFont(uiFont.value);
    }
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
