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
    const char* status;
};

// G-04: render the charted route and marker using one fractional song beat.
// The route is fully independent of keyboard input and rendered frame count.
constexpr float kGridSpacing = 76.0f;
constexpr float kGridOriginX = kWindowWidth / 2.0f - 1.5f * kGridSpacing;
constexpr float kGridOriginY = 278.0f - 1.5f * kGridSpacing;

Vector2 GridPoint(locus::core::Cell cell) {
    return Vector2{kGridOriginX + static_cast<float>(cell.x) * kGridSpacing,
                   kGridOriginY + static_cast<float>(cell.y) * kGridSpacing};
}

Vector2 GridPosition(locus::core::GridPoint point) {
    return Vector2{kGridOriginX + static_cast<float>(point.x) * kGridSpacing,
                   kGridOriginY + static_cast<float>(point.y) * kGridSpacing};
}

// The active line grows with the marker, not in jumps on each beat.
// Show three future steps; their farthest segment grows gradually as
// the look-ahead horizon moves. A Wrap never connects opposite edges.
Vector2 DrawRoutePreview(const locus::core::Chart& chart, double fractionalBeat,
                         double previewLeadBeats) {
    if (chart.lengthBeats <= 0) {
        return GridPoint(chart.start);
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
            DrawLineEx(GridPosition(line.from), GridPosition(line.to),
                       (ahead == 0) ? 6.0f : 3.5f, color);
        }
        if (ahead > 0 && reveal >= 1.0) {
            DrawCircleV(GridPoint(step.to), 4.0f, color);
        }
    }

    const locus::core::RouteStep live = locus::core::SampleRouteStep(chart, currentBeat);
    return GridPosition(locus::core::ViewRouteStep(live, currentProgress).marker);
}

void DrawPlayGrid(Font font, const GameplayView& gameplay) {
    const auto& chart = gameplay.chart;
    const auto& judge = gameplay.judge;
    DrawTextLabel(font, gameplay.custom ? "PLAY  /  YOUR CHART" : "PLAY  /  DEMO CHART",
                  Vector2{50.0f, 98.0f}, 27.0f,
                  Color{211, 239, 241, 255});

    // Grid guides are quieter than the authored route overlay.
    const Color guides{43, 50, 64, 255};
    for (int index = 0; index < 4; ++index) {
        DrawLineEx(GridPoint({0, index}), GridPoint({3, index}), 2.0f, guides);
        DrawLineEx(GridPoint({index, 0}), GridPoint({index, 3}), 2.0f, guides);
    }

    const Vector2 marker = DrawRoutePreview(chart, gameplay.beat,
                                             gameplay.previewLeadBeats);

    const Color pointOuter{83, 101, 122, 255};
    const Color pointInner{169, 191, 204, 255};
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const Vector2 point = GridPoint({x, y});
            DrawCircleV(point, 9.0f, pointOuter);
            DrawCircleV(point, 4.0f, pointInner);
        }
    }

    // One song-time snapshot controls both the marker and preview.
    // The marker keeps moving along the authored path without key presses.
    DrawCircleV(marker, 26.0f, Color{112, 211, 216, 45});
    DrawCircleV(marker, 17.0f, Color{125, 224, 229, 255});
    DrawCircleV(marker, 5.5f, Color{245, 253, 255, 255});

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

    DrawTextLabel(font, TextFormat("%.1f BPM  |  %d BEATS  |  4 x 4 GRID",
                                     chart.bpm, chart.lengthBeats),
                  Vector2{50.0f, 140.0f}, 20.0f,
                  Color{204, 208, 222, 255});
    DrawTextLabel(font, "SPACE PAUSE  |  R RETRY  |  H HOME",
                  Vector2{50.0f, 380.0f}, 17.0f,
                  Color{229, 214, 170, 255});
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
                 const std::string& editorStatus, EditorPending editorPending,
                 const std::string& musicError, double now) {
    BeginDrawing();
    ClearBackground(kBackground);

    // Keep all UI and the 4x4 grid at the original 16:9 proportions.
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
        locus::app::RenderChartEditor(font, editor);
        DrawTextLabel(font, editorStatus.c_str(), Vector2{416.0f, 129.0f},
                      14.0f, Color{175, 212, 213, 255});
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
    std::string chartTitle = "BUILT-IN DEMO";
    std::string musicError;
    transport.Initialize(demo); // Owns the shared raylib audio output device.
    while (state.running) {
        const FrameInput input = PollInput(debugScreens);
        if (input.fullscreenPressed) {
            ToggleBorderlessWindowed(); // F11: fullscreen desktop / resizable window
        }
        const Screen previousScreen = state.screen;
        UpdateState(state, input, smokeWindow);
        if (!state.running) {
            break;
        }
        double now = MonotonicSeconds();
        bool loadedNewSong = false;
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
                    editor.Load(candidate); // M-03 can edit the imported route.
                    activeChart = std::move(candidate.route);
                    chartTitle = std::move(candidate.title);
                    previewLead = candidate.previewLeadBeats;
                    chartPlayback.Configure(activeChart.bpm, candidate.offsetSeconds);
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
            if (state.screen == Screen::Play) {
                if (previousScreen != Screen::Results && !loadedNewChart) {
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
                localSong.Start(now);
            }
        } else if (state.screen == Screen::Play) {
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

        if (state.screen == Screen::Editor && previousScreen == Screen::Editor) {
            locus::app::ApplyEditorControls(editor, input.editor);
        }
        if (state.screen == Screen::Listen) {
            localSong.Update(now);
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
                                    previewLead, paused, finished, customChart, status};
        RenderFrame(state, debugScreens, uiFont.value, demo,
                    gameplay, localSong, editor, editorStatus, editorPending,
                    musicError, renderNow);
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
