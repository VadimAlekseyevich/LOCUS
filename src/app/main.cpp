#include "core/foundation.h"
#include "core/route.h"

#include "raylib.h"

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

// F-07: placeholder screens share one window and one input/update/render loop.
// Real screen controllers, routing, and musical timing arrive in later tasks.
namespace {
constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
constexpr int kSmokeFrames = 60;
constexpr Color kBackground = {21, 23, 33, 255};

enum class Screen {
    Home,
    Play,
    Editor,
    Results
};

struct FrameInput {
    bool closeRequested = false;
    std::optional<Screen> requestedScreen;
};

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
    }
    return "Unknown";
}

// 1. Input: raylib already pumps native events at the end of each drawn frame.
// F1-F4 are diagnostic shortcuts ONLY when --debug-screens is explicitly enabled.
FrameInput PollInput(bool debugScreens) {
    FrameInput input;
    input.closeRequested = WindowShouldClose();
    if (debugScreens) {
        if (IsKeyPressed(KEY_F1)) input.requestedScreen = Screen::Home;
        else if (IsKeyPressed(KEY_F2)) input.requestedScreen = Screen::Play;
        else if (IsKeyPressed(KEY_F3)) input.requestedScreen = Screen::Editor;
        else if (IsKeyPressed(KEY_F4)) input.requestedScreen = Screen::Results;
    }
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
    }
    ++state.frames;
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
    const char* fontFiles[] = {"segoeui.ttf", "arial.ttf"};
    for (const char* filename : fontFiles) {
        const std::string path = fontsDirectory + filename;
        if (!FileExists(path.c_str())) {
            continue;
        }
        Font candidate = LoadFontEx(path.c_str(), 64, nullptr, 0);
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

// G-02: show the actual 4x4 board, using the start position from the demo
// chart. Audio-driven motion and future-route lines are added in G-03/G-04.
constexpr float kGridSpacing = 76.0f;
constexpr float kGridOriginX = kWindowWidth / 2.0f - 1.5f * kGridSpacing;
constexpr float kGridOriginY = 278.0f - 1.5f * kGridSpacing;

Vector2 GridPoint(locus::core::Cell cell) {
    return Vector2{kGridOriginX + static_cast<float>(cell.x) * kGridSpacing,
                   kGridOriginY + static_cast<float>(cell.y) * kGridSpacing};
}

void DrawPlayGrid(Font font, const locus::core::Chart& chart) {
    DrawTextLabel(font, "PLAY  /  DEMO CHART", Vector2{50.0f, 98.0f}, 27.0f,
                  Color{211, 239, 241, 255});

    // Quiet guides help the player read all sixteen points without implying
    // that these guide edges are the chart's actual upcoming route.
    const Color guides{58, 68, 85, 255};
    for (int index = 0; index < 4; ++index) {
        DrawLineEx(GridPoint({0, index}), GridPoint({3, index}), 2.0f, guides);
        DrawLineEx(GridPoint({index, 0}), GridPoint({index, 3}), 2.0f, guides);
    }

    const Color pointOuter{83, 101, 122, 255};
    const Color pointInner{169, 191, 204, 255};
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const Vector2 point = GridPoint({x, y});
            DrawCircleV(point, 9.0f, pointOuter);
            DrawCircleV(point, 4.0f, pointInner);
        }
    }

    // The marker is deliberately stationary at beat zero until the audio
    // clock drives its progress. It is not controlled by Left/Right input.
    const Vector2 start = GridPoint(chart.start);
    DrawCircleV(start, 26.0f, Color{112, 211, 216, 45});
    DrawCircleV(start, 17.0f, Color{125, 224, 229, 255});
    DrawCircleV(start, 5.5f, Color{245, 253, 255, 255});

    DrawTextLabel(font, "120 BPM  |  32 BEATS  |  4 x 4 GRID",
                  Vector2{50.0f, 439.0f}, 20.0f,
                  Color{204, 208, 222, 255});
    DrawTextLabel(font, "Marker movement and music are coming next",
                  Vector2{50.0f, 474.0f}, 19.0f,
                  Color{159, 164, 184, 255});
}

// 3. Render: display the selected state; never choose or modify it here.
void RenderFrame(const AppState& state, bool debugScreens, Font font,
                 const locus::core::Chart& demo) {
    BeginDrawing();
    ClearBackground(kBackground);

    DrawTextLabel(font, "LOCUS", Vector2{50.0f, 38.0f}, 30.0f,
                  Color{223, 225, 238, 255});

    if (state.screen == Screen::Play) {
        DrawPlayGrid(font, demo);
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
    if (debugScreens) {
        DrawTextLabel(font, "DEBUG: F1 Home   F2 Play   F3 Editor   F4 Results",
                      Vector2{50.0f, static_cast<float>(kWindowHeight - 65)},
                      22.0f, Color{232, 213, 160, 255});
    }
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

    const bool smokeWindow = HasArgument(argc, argv, "--window-smoke");
    const bool debugScreens = HasArgument(argc, argv, "--debug-screens");
    if (argc != 1 && !smokeWindow && !debugScreens) {
        std::cerr << "Usage: locus.exe [--foundation-check | --loop-check | --screen-check | "
                     "--window-smoke | --debug-screens]\n";
        return 2;
    }

    InitWindow(kWindowWidth, kWindowHeight, "LOCUS");
    if (!IsWindowReady()) {
        std::cerr << "WINDOW_INIT_FAILED\n";
        return 1;
    }
    UiFont uiFont = LoadUiFont();
    SetTargetFPS(60); // Visual smoke baseline only, never a musical clock.
    std::cout << "LOCUS_WINDOW_OPEN\n";

    AppState state;
    const locus::core::Chart demo = locus::core::MakeDemoChart();
    while (state.running) {
        const FrameInput input = PollInput(debugScreens);
        UpdateState(state, input, smokeWindow);
        if (!state.running) {
            break;
        }
        RenderFrame(state, debugScreens, uiFont.value, demo);
    }

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
