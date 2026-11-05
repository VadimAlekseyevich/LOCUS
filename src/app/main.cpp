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

#include "raylib.h"

#include <algorithm>
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
    Listen
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
    bool editorSave = false;
    bool editorSaveAs = false;
    bool editorOpen = false;
    bool editorNew = false;
    bool editorTest = false;
    int practiceSpeedIndex = -1;
    bool leftPressed = false;
    bool rightPressed = false;
    double leftCapturedAt = 0.0;
    double rightCapturedAt = 0.0;
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
    }
    return "Unknown";
}

// 1. Input: raylib already pumps native events at the end of each drawn frame.
// F1-F4 are diagnostic shortcuts ONLY when --debug-screens is explicitly enabled.
FrameInput PollInput(bool debugScreens) {
    FrameInput input;
    input.closeRequested = WindowShouldClose() || IsKeyPressed(KEY_ESCAPE);
    if (debugScreens) {
        if (IsKeyPressed(KEY_F1)) input.requestedScreen = Screen::Home;
        else if (IsKeyPressed(KEY_F2)) input.requestedScreen = Screen::Play;
        else if (IsKeyPressed(KEY_F3)) input.requestedScreen = Screen::Editor;
        else if (IsKeyPressed(KEY_F4)) input.requestedScreen = Screen::Results;
    }
    input.startPressed = IsKeyPressed(KEY_ENTER);
    input.pausePressed = IsKeyPressed(KEY_SPACE);
    input.restartPressed = IsKeyPressed(KEY_R);
    input.homePressed = IsKeyPressed(KEY_H);
    input.fullscreenPressed = IsKeyPressed(KEY_F11);
    input.chooseMusicPressed = IsKeyPressed(KEY_O);
    input.chooseChartPressed = IsKeyPressed(KEY_C);
    input.openEditorPressed = IsKeyPressed(KEY_E);
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
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
    input.leftPressed = IsKeyPressed(KEY_F);
    if (input.leftPressed) input.leftCapturedAt = MonotonicSeconds();
    input.rightPressed = IsKeyPressed(KEY_J);
    if (input.rightPressed) input.rightCapturedAt = MonotonicSeconds();
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
    } else if (input.homePressed && state.screen != Screen::Home) {
        state.screen = Screen::Home;
    } else if (state.screen == Screen::Home && input.openEditorPressed) {
        state.screen = Screen::Editor;
    } else if ((state.screen == Screen::Home && input.startPressed) ||
               (state.screen == Screen::Results &&
                (input.startPressed || input.restartPressed))) {
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
    const std::string& title;
    double songSeconds;
    double beat;
    double previewLeadBeats;
    bool paused;
    bool completed;
    bool custom;
    double practiceSpeed;
    const char* status;
};

// G-04: render the charted route and marker using one fractional song beat.
// The route is fully independent of keyboard input and rendered frame count.
struct GameplayGrid {
    float step;
    float left;
    float top;

    explicit GameplayGrid(const locus::core::Chart& chart) {
        const int span = std::max(chart.gridWidth - 1, chart.gridHeight - 1);
        step = std::min(76.0f, 240.0f / static_cast<float>(std::max(1, span)));
        left = kWindowWidth / 2.0f -
               static_cast<float>(chart.gridWidth - 1) * step / 2.0f;
        top = 278.0f - static_cast<float>(chart.gridHeight - 1) * step / 2.0f;
    }

    Vector2 Point(locus::core::Cell cell) const {
        return {left + static_cast<float>(cell.x) * step,
                top + static_cast<float>(cell.y) * step};
    }
    Vector2 Position(locus::core::GridPoint point) const {
        return {left + static_cast<float>(point.x) * step,
                top + static_cast<float>(point.y) * step};
    }
};

// The active line grows with the marker, not in jumps on each beat.
// Show three future steps; their farthest segment grows gradually as
// the look-ahead horizon moves. A Wrap never connects opposite edges.
Vector2 DrawRoutePreview(const locus::core::Chart& chart, double fractionalBeat,
                         double previewLeadBeats) {
    const GameplayGrid grid(chart);
    if (chart.lengthBeats <= 0) {
        return grid.Point(chart.start);
    }
    const double beat = std::clamp(fractionalBeat, 0.0,
                                   static_cast<double>(chart.lengthBeats));
    const int currentBeat = std::min(static_cast<int>(std::floor(beat)),
                                     chart.lengthBeats - 1);
    const double currentProgress = std::clamp(beat - currentBeat, 0.0, 1.0);
    const double lead = std::clamp(previewLeadBeats, 0.01, 8.0);
    const int previewSteps = std::clamp(static_cast<int>(std::ceil(lead)), 1, 8);

    for (int ahead = previewSteps - 1; ahead >= 0; --ahead) {
        const Color color = ahead == 0
            ? Color{107, 230, 222, 255}
            : Color{106, 187, 224,
                    static_cast<unsigned char>(std::max(75, 255 - 35 * ahead))};
        const int routeBeat = currentBeat + ahead;
        if (routeBeat >= chart.lengthBeats) continue;
        const locus::core::RouteStep step =
            locus::core::SampleRouteStep(chart, routeBeat);
        // The current step is revealed up to the moving marker. The
        // newest (third future) step is also revealed over a full beat,
        // so no complete new segment pops into view on a click.
        const double reveal = (ahead == 0)
                                  ? currentProgress
                              : (ahead == previewSteps - 1)
                                  ? std::clamp(currentProgress + lead - previewSteps,
                                               0.0, 1.0)
                                  : 1.0;
        const auto view = locus::core::TraceRouteStep(step, reveal);
        for (int segment = 0; segment < view.segmentCount; ++segment) {
            const auto& line = view.segments[segment];
            const float thickness = std::clamp(grid.step / 76.0f, 0.4f, 1.0f);
            DrawLineEx(grid.Position(line.from), grid.Position(line.to),
                       (ahead == 0 ? 6.0f : 3.5f) * thickness, color);
        }
        if (ahead > 0 && reveal >= 1.0) {
            DrawCircleV(grid.Point(step.to), std::min(4.0f, grid.step * 0.16f), color);
        }
    }

    const locus::core::RouteStep live = locus::core::SampleRouteStep(chart, currentBeat);
    return grid.Position(locus::core::ViewRouteStep(live, currentProgress).marker);
}

void DrawPlayGrid(Font font, const GameplayView& gameplay) {
    const auto& chart = gameplay.chart;
    const auto& judge = gameplay.judge;
    const GameplayGrid grid(chart);
    DrawTextLabel(font, gameplay.custom ? "PLAY  /  YOUR CHART" : "PLAY  /  DEMO CHART",
                  Vector2{50.0f, 98.0f}, 27.0f,
                  Color{211, 239, 241, 255});

    // Grid guides are quieter than the authored route overlay.
    const Color guides{43, 50, 64, 255};
    for (int row = 0; row < chart.gridHeight; ++row) {
        DrawLineEx(grid.Point({0, row}),
                   grid.Point({chart.gridWidth - 1, row}), 1.5f, guides);
    }
    for (int column = 0; column < chart.gridWidth; ++column) {
        DrawLineEx(grid.Point({column, 0}),
                   grid.Point({column, chart.gridHeight - 1}), 1.5f, guides);
    }

    const Vector2 marker = DrawRoutePreview(chart, gameplay.beat,
                                             gameplay.previewLeadBeats);

    const Color pointOuter{83, 101, 122, 255};
    const Color pointInner{169, 191, 204, 255};
    const float dotRadius = std::min(9.0f, grid.step * 0.30f);
    for (int y = 0; y < chart.gridHeight; ++y) {
        for (int x = 0; x < chart.gridWidth; ++x) {
            const Vector2 point = grid.Point({x, y});
            DrawCircleV(point, dotRadius, pointOuter);
            DrawCircleV(point, dotRadius * 0.45f, pointInner);
        }
    }

    // One song-time snapshot controls both the marker and preview.
    // The marker keeps moving along the authored path without key presses.
    const float markerRadius = std::min(17.0f, grid.step * 0.47f);
    DrawCircleV(marker, markerRadius * 1.5f, Color{112, 211, 216, 45});
    DrawCircleV(marker, markerRadius, Color{125, 224, 229, 255});
    DrawCircleV(marker, markerRadius * 0.33f, Color{245, 253, 255, 255});

    DrawTextLabel(font, "CYAN   CURRENT STEP", Vector2{665.0f, 216.0f}, 17.0f,
                  Color{131, 232, 227, 255});
    DrawTextLabel(font, "BLUE   ROUTE PREVIEW", Vector2{665.0f, 244.0f}, 17.0f,
                  Color{128, 174, 222, 255});
    DrawTextLabel(font, "WRAP   EDGE SPLIT", Vector2{665.0f, 272.0f}, 17.0f,
                  Color{184, 189, 204, 255});

    // F/J only affect judgement feedback. The charted route never changes.
    DrawTextLabel(font, "F  =  LEFT", Vector2{50.0f, 204.0f}, 21.0f,
                  Color{236, 216, 160, 255});
    DrawTextLabel(font, "J  =  RIGHT", Vector2{50.0f, 237.0f}, 21.0f,
                  Color{236, 216, 160, 255});
    DrawTextLabel(font, "HIT ON THE BEAT", Vector2{50.0f, 274.0f}, 18.0f,
                  Color{171, 187, 210, 255});

    if (const locus::core::TurnEvent* next = judge.NextEvent()) {
        DrawTextLabel(font,
                      TextFormat("NEXT: %s  /  BEAT %d",
                                 locus::core::TurnName(next->direction), next->beat),
                      Vector2{50.0f, 318.0f}, 17.0f,
                      Color{228, 231, 241, 255});
    } else {
        DrawTextLabel(font, "ALL TURNS RESOLVED",
                      Vector2{50.0f, 318.0f}, 17.0f,
                      Color{228, 231, 241, 255});
    }

    const auto& counts = judge.Counts();
    DrawTextLabel(font,
                  TextFormat("PERFECT %d  GOOD %d", counts.perfect, counts.good),
                  Vector2{665.0f, 322.0f}, 17.0f,
                  Color{194, 225, 232, 255});
    DrawTextLabel(font,
                  TextFormat("BAD %d  MISS %d", counts.bad, counts.miss),
                  Vector2{665.0f, 350.0f}, 17.0f,
                  Color{202, 207, 218, 255});
    if (const auto& last = judge.Last()) {
        const char* label = locus::core::GradeName(last->grade);
        if (last->grade == locus::core::Grade::Miss) {
            label = last->wrongDirection ? "MISS (WRONG KEY)" : "MISS (NO INPUT)";
        }
        const Color feedback = last->grade == locus::core::Grade::Perfect
                                   ? Color{131, 240, 207, 255}
                               : last->grade == locus::core::Grade::Good
                                   ? Color{131, 204, 235, 255}
                               : last->grade == locus::core::Grade::Bad
                                   ? Color{239, 195, 123, 255}
                                   : Color{239, 136, 139, 255};
        DrawTextLabel(font, label, Vector2{665.0f, 379.0f},
                      17.0f, feedback);
    }

    DrawTextLabel(font, TextFormat("%.1f BPM  |  %d BEATS  |  %d x %d GRID",
                                     chart.bpm, chart.lengthBeats,
                                     chart.gridWidth, chart.gridHeight),
                  Vector2{50.0f, 140.0f}, 20.0f,
                  Color{204, 208, 222, 255});
    DrawTextLabel(font, "SPACE PAUSE  |  R RETRY  |  H HOME",
                  Vector2{50.0f, 380.0f}, 17.0f,
                  Color{229, 214, 170, 255});
    if (gameplay.custom) {
        DrawTextLabel(font,
                      TextFormat("SPEED %.0f%%  |  1-5 TO PRACTICE SLOWLY",
                                 gameplay.practiceSpeed * 100.0),
                      Vector2{665.0f, 413.0f}, 15.0f,
                      Color{164, 210, 222, 255});
    }
    DrawTextLabel(font, gameplay.status, Vector2{50.0f, 412.0f}, 20.0f,
                  Color{211, 239, 241, 255});
    DrawTextLabel(font,
                  TextFormat("SONG TIME  %.2f s     BEAT  %.2f",
                             gameplay.songSeconds, gameplay.beat),
                  Vector2{50.0f, 444.0f}, 20.0f,
                  Color{204, 208, 222, 255});
}

void DrawHomeScreen(Font font, const locus::core::Chart& demo,
                    const std::string& musicError) {
    DrawTextLabel(font, "THE FIRST PLAYABLE DEMO", Vector2{110.0f, 119.0f},
                  25.0f, Color{136, 221, 218, 255});
    DrawTextLabel(font, "FOLLOW THE RHYTHM", Vector2{110.0f, 172.0f},
                  47.0f, Color{232, 240, 246, 255});
    DrawTextLabel(font, "ENTER   PLAY BUILT-IN DEMO", Vector2{110.0f, 260.0f},
                  27.0f, Color{237, 218, 157, 255});
    DrawTextLabel(font, "O   LISTEN TO WAV / MP3", Vector2{110.0f, 298.0f},
                  26.0f, Color{144, 224, 225, 255});
    DrawTextLabel(font, "C   OPEN JSON CHART + SONG", Vector2{110.0f, 337.0f},
                  25.0f, Color{177, 229, 206, 255});
    DrawTextLabel(font, "E   EDIT BEATS + TURNS", Vector2{110.0f, 375.0f},
                  22.0f, Color{184, 200, 239, 255});
    DrawTextLabel(font,
                  TextFormat("%d BEATS  |  %.0f BPM  |  %d TURNS  |  NO FAIL",
                             demo.lengthBeats, demo.bpm,
                             static_cast<int>(demo.turns.size())),
                  Vector2{110.0f, 412.0f}, 19.0f,
                  Color{182, 192, 209, 255});
    if (!musicError.empty()) {
        const std::string shortError = musicError.substr(0, 92);
        DrawTextLabel(font, shortError.c_str(), Vector2{110.0f, 443.0f},
                      16.0f, Color{241, 150, 150, 255});
    } else {
        DrawTextLabel(font, "F LEFT / J RIGHT  |  NO FAIL",
                      Vector2{110.0f, 443.0f}, 17.0f,
                      Color{165, 182, 203, 255});
    }
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

// 3. Render: display the selected state; never choose or modify it here.
void RenderFrame(const AppState& state, bool debugScreens, Font font,
                 const locus::core::Chart& demo,
                 const GameplayView& gameplay,
                 const locus::app::LocalMusic& localSong,
                 const locus::core::ChartEditor& editor,
                 const locus::app::EditorUiState& editorUi,
                 const locus::app::ChartPlayback& editorTransport,
                 const std::string& editorStatus, EditorPending editorPending,
                 const std::string& musicError, double now) {
    BeginDrawing();
    ClearBackground(kBackground);

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

    DrawTextLabel(font, "LOCUS", Vector2{50.0f, 38.0f}, 30.0f,
                  Color{223, 225, 238, 255});

    if (state.screen == Screen::Home) {
        DrawHomeScreen(font, demo, musicError);
    } else if (state.screen == Screen::Listen) {
        DrawLocalMusicScreen(font, localSong, now, musicError);
    } else if (state.screen == Screen::Play) {
        DrawPlayGrid(font, gameplay);
        if (gameplay.paused) {
            DrawRectangle(0, 0, kWindowWidth, kWindowHeight,
                          Color{12, 14, 22, 204});
            DrawTextLabel(font, "PAUSED", Vector2{355.0f, 198.0f},
                          59.0f, Color{240, 246, 247, 255});
            DrawTextLabel(font, "SPACE  -  RESUME",
                          Vector2{325.0f, 287.0f}, 28.0f,
                          Color{239, 219, 164, 255});
            DrawTextLabel(font, "R  -  RETRY      H  -  HOME",
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
    DrawTextLabel(font, "F11  FULLSCREEN", Vector2{665.0f, 61.0f}, 16.0f,
                  Color{174, 191, 208, 255});
    if (debugScreens && state.screen != Screen::Editor) {
        DrawTextLabel(font, "DEBUG: F1 Home   F2 Play   F3 Editor   F4 Results",
                      Vector2{50.0f, static_cast<float>(kWindowHeight - 65)},
                      22.0f, Color{232, 213, 160, 255});
    }
    EndMode2D();
    EndDrawing();
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

    const Screen transitions[] = {Screen::Play, Screen::Editor, Screen::Results, Screen::Home};
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
    return !state.running && state.screen == Screen::Home &&
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
        const std::wstring suggestion = destination.empty()
            ? (std::filesystem::path(audioPath).parent_path() /
               std::filesystem::path(L"Untitled.locus.json")).wstring()
            : destination;
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
        std::cout << "SCREEN_STATE_PASS transitions=4\n";
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

    AppState state;
    const locus::core::Chart demo = locus::core::MakeDemoChart();
    locus::core::Chart activeChart = demo;
    locus::core::ChartEditor editor;
    locus::app::EditorUiState editorUi;
    locus::core::TurnJudge judge(activeChart); // Stable reference across chart loads.
    locus::app::DemoTransport transport;
    locus::app::LocalMusic localSong;
    locus::app::ChartPlayback chartPlayback(localSong);
    bool customChart = false;
    std::wstring editorChartPath;
    std::wstring editorAudioPath;
    EditorPending editorPending = EditorPending::None;
    std::string editorStatus = "O ATTACH AUDIO  |  CTRL+S SAVE  |  T TEST PLAY";
    double previewLead = 4.0;
    int editorPreviewStartBeat = 0; // R returns to this preview origin.
    std::string chartTitle = "BUILT-IN DEMO";
    std::string musicError;
    transport.Initialize(demo); // Owns the shared raylib audio output device.
    while (state.running) {
        FrameInput input = PollInput(debugScreens);
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
                ? std::max(0.0, chartPlayback.Beat(now))
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
                // Preview is for authoring, not scoring: allow a partial
                // chart even when its full timeline exceeds song duration.
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
                        editorAudioPath = picked.path;
                        const std::string name =
                            std::filesystem::path(picked.path).filename().u8string();
                        editor.SetAudioPath(name);
                        editorStatus = "SONG LOADED - CLICK PLAY / SPACE TO PREVIEW";
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
                } else if (!failure.empty()) {
                    editorStatus = "SAVE FAILED: " + failure;
                }
            } else if (input.editorTest) {
                chartPlayback.Stop();
                locus::core::LoadedChart checked = editor.Data();
                if (editorAudioPath.empty() || !localSong.Ready()) {
                    editorStatus = "TEST NEEDS AUDIO - CLICK LOAD SONG";
                } else if (!locus::core::PrepareChartForSong(
                               checked, localSong.DurationSeconds(), musicError)) {
                    editorStatus = "TEST FAILED: " + musicError;
                } else {
                    activeChart = std::move(checked.route);
                    chartTitle = std::move(checked.title);
                    previewLead = checked.previewLeadBeats;
                    chartPlayback.Configure(activeChart.bpm, checked.offsetSeconds);
                    judge.SetOffsetSeconds(checked.offsetSeconds);
                    customChart = true;
                    state.screen = Screen::Play;
                    editorStatus = "TEST IN PROGRESS - H HOME / E EDIT AGAIN";
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
        if (input.chooseChartPressed && state.screen == Screen::Home) {
            // The same Play/Results route and F/J judge handle imported charts.
            // Only commit a new chart after JSON + relative audio + duration
            // have been validated. A canceled dialog keeps Home unchanged.
            const auto selected = locus::app::ChooseChartFile();
            now = MonotonicSeconds();
            if (!selected.path.empty()) {
                locus::core::LoadedChart candidate;
                std::wstring songPath;
                if (!locus::app::OpenChartWithAudioPath(
                        selected.path, candidate, songPath, musicError)) {
                    std::cerr << "CHART_LOAD_FAIL " << musicError << '\\n';
                } else if (!localSong.LoadFromFile(songPath)) {
                    musicError = "audioPath: " + localSong.Error();
                } else if (!locus::core::PrepareChartForSong(
                               candidate, localSong.DurationSeconds(), musicError)) {
                    std::cerr << "CHART_TIMING_FAIL " << musicError << '\\n';
                } else {
                    editor.Load(candidate); // M-04 can reopen/edit/save imported routes.
                    editorChartPath = selected.path;
                    editorAudioPath = songPath;
                    editorStatus = "CHART LOADED - E EDIT / CTRL+S SAVE";
                    activeChart = std::move(candidate.route);
                    chartTitle = std::move(candidate.title);
                    previewLead = candidate.previewLeadBeats;
                    chartPlayback.Configure(activeChart.bpm, candidate.offsetSeconds);
                    chartPlayback.SetSpeed(1.0, now); // Fresh Home charts start at normal speed.
                    judge.SetOffsetSeconds(candidate.offsetSeconds);
                    customChart = true;
                    loadedNewChart = true;
                    state.screen = Screen::Play;
                    musicError.clear();
                    std::cout << "CHART_V1_READY turns=" << activeChart.turns.size()
                              << " bpm=" << activeChart.bpm << '\\n';
                }
            } else if (!selected.canceled) {
                musicError = selected.error;
            }
        }

        if (state.screen != previousScreen) {
            if (previousScreen == Screen::Play) {
                if (customChart) chartPlayback.Stop();
                else transport.Stop();
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
                    previewLead = 4.0;
                    judge.SetOffsetSeconds(0.0);
                } else {
                    judge.Reset();
                }
                if (customChart) chartPlayback.Start(now);
                else transport.Start(now);
            } else if (state.screen == Screen::Listen) {
                localSong.SetSpeed(1.0, now); // Preview slowdown must not leak to Listen.
                localSong.Start(now);
            }
        } else if (state.screen == Screen::Play) {
            if (customChart && input.practiceSpeedIndex >= 0) {
                const double speeds[] = {1.0, 0.75, 0.5, 0.25, 0.1};
                chartPlayback.SetSpeed(speeds[input.practiceSpeedIndex], now);
            }
            if (input.restartPressed ||
                (input.requestedScreen && *input.requestedScreen == Screen::Play)) {
                judge.Reset();
                if (customChart) chartPlayback.Start(now);
                else transport.Start(now);
            } else if (input.pausePressed) {
                if (customChart) {
                    if (chartPlayback.Paused()) chartPlayback.Resume(now);
                    else chartPlayback.Pause(now);
                } else {
                    if (transport.Paused()) transport.Resume(now);
                    else transport.Pause(now);
                }
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
                        static_cast<int>(std::floor(currentBeat)), 0, lastBeat);
                    editor.SelectBeat(beat); // Follow audio with bottom squares.
                }
            }
        }
        if (state.screen == Screen::Play) {
            const bool playing = customChart ? chartPlayback.Playing() : transport.Playing();
            const bool paused = customChart ? chartPlayback.Paused() : transport.Paused();
            if (playing && !paused) {
                // Fresh F/J key-downs are timestamped in music-time coordinates
                // for either audio source. Player input never changes the route.
                if (input.leftPressed) {
                    judge.Press(locus::core::Turn::Left,
                                customChart
                                    ? chartPlayback.AudioSeconds(input.leftCapturedAt)
                                    : transport.SongSeconds(input.leftCapturedAt));
                }
                if (input.rightPressed) {
                    judge.Press(locus::core::Turn::Right,
                                customChart
                                    ? chartPlayback.AudioSeconds(input.rightCapturedAt)
                                    : transport.SongSeconds(input.rightCapturedAt));
                }
            }
            if (customChart) chartPlayback.Update(now);
            else transport.Update(now);
            const double songTime = customChart ? chartPlayback.AudioSeconds(now)
                                                 : transport.SongSeconds(now);
            if (customChart || transport.Ready()) judge.Advance(songTime);
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
               : songTime < 0.0 ? "SILENT PREROLL  |  F/J TO HIT"
                                : "CUSTOM SONG  |  F/J TO HIT")
            : transport.Status();
        const GameplayView gameplay{activeChart, judge, chartTitle, songTime,
                                    customChart ? chartPlayback.Beat(renderNow)
                                                : transport.Beat(renderNow),
                                    previewLead, paused, finished, customChart,
                                    customChart ? chartPlayback.Speed() : 1.0, status};
        RenderFrame(state, debugScreens, uiFont.value, demo,
                    gameplay, localSong, editor, editorUi, chartPlayback,
                    editorStatus, editorPending, musicError, renderNow);
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
