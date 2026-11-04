#include "core/foundation.h"

#include "raylib.h"

#include <cstring>
#include <iostream>

// F-06: one loop with explicit input, update, and render phases.
// Rendering is a blank placeholder; screens and musical timing arrive later.
namespace {
constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
constexpr int kSmokeFrames = 60;
constexpr Color kBackground = {21, 23, 33, 255};

struct FrameInput {
    bool closeRequested = false;
};

struct AppState {
    bool running = true;
    int frames = 0;
};

bool HasArgument(int argc, char** argv, const char* argument) {
    return argc == 2 && std::strcmp(argv[1], argument) == 0;
}

// 1. Input: read events raylib collected in the previous EndDrawing().
// Do not call PollInputEvents() here: raylib already pumps them per frame.
FrameInput PollInput() {
    return {WindowShouldClose()};
}

// 2. Update: change app state only. No raylib drawing calls or music timing.
void UpdateState(AppState& state, const FrameInput& input, bool smokeWindow) {
    if (input.closeRequested || (smokeWindow && state.frames >= kSmokeFrames)) {
        state.running = false;
        return;
    }
    ++state.frames;
}

// 3. Render: draw the current blank view without changing app state.
void RenderFrame() {
    BeginDrawing();
    ClearBackground(kBackground);
    EndDrawing();
}

// Headless unit-sized diagnostic for the same update function used in the loop.
bool CheckLoopState() {
    AppState state;
    for (int i = 0; i < kSmokeFrames; ++i) {
        UpdateState(state, FrameInput{}, true);
        if (!state.running || state.frames != i + 1) {
            return false;
        }
    }
    UpdateState(state, FrameInput{}, true);
    if (state.running || state.frames != kSmokeFrames) {
        return false;
    }

    AppState closedByUser;
    UpdateState(closedByUser, FrameInput{true}, false);
    return !closedByUser.running && closedByUser.frames == 0;
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

    const bool smokeWindow = HasArgument(argc, argv, "--window-smoke");
    if (argc != 1 && !smokeWindow) {
        std::cerr << "Usage: locus.exe [--foundation-check | --loop-check | --window-smoke]\n";
        return 2;
    }

    InitWindow(kWindowWidth, kWindowHeight, "LOCUS");
    if (!IsWindowReady()) {
        std::cerr << "WINDOW_INIT_FAILED\n";
        return 1;
    }
    SetTargetFPS(60); // Visual smoke baseline only, never a musical clock.
    std::cout << "LOCUS_WINDOW_OPEN\n";

    AppState state;
    while (state.running) {
        const FrameInput input = PollInput();
        UpdateState(state, input, smokeWindow);
        if (!state.running) {
            break;
        }
        RenderFrame();
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
