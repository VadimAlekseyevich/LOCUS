#include "core/foundation.h"

#include "raylib.h"

#include <cstring>
#include <iostream>

// F-05: create and close a blank production window; gameplay and app phases
// arrive in later roadmap tasks. A fixed FPS here is only a UI smoke baseline.
namespace {
constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 540;
constexpr int kSmokeFrames = 60;
constexpr Color kBackground = {21, 23, 33, 255};

bool HasArgument(int argc, char** argv, const char* argument) {
    return argc == 2 && std::strcmp(argv[1], argument) == 0;
}
} // namespace

int main(int argc, char** argv) {
    if (HasArgument(argc, argv, "--foundation-check")) {
        // Headless linkage verification for CI machines with no display.
        std::cout << locus::core::FoundationStatus() << '\n';
        return 0;
    }

    const bool smokeWindow = HasArgument(argc, argv, "--window-smoke");
    if (argc != 1 && !smokeWindow) {
        std::cerr << "Usage: locus.exe [--foundation-check | --window-smoke]\n";
        return 2;
    }

    InitWindow(kWindowWidth, kWindowHeight, "LOCUS");
    if (!IsWindowReady()) {
        std::cerr << "WINDOW_INIT_FAILED\n";
        return 1;
    }
    SetTargetFPS(60);
    std::cout << "LOCUS_WINDOW_OPEN\n";

    int renderedFrames = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(kBackground);
        EndDrawing();

        ++renderedFrames;
        if (smokeWindow && renderedFrames >= kSmokeFrames) {
            break; // F-05 CI: exercise draw and exit without a person.
        }
    }

    CloseWindow();
    if (smokeWindow) {
        if (IsWindowReady() || renderedFrames < kSmokeFrames) {
            std::cerr << "WINDOW_SMOKE_FAIL\n";
            return 1;
        }
        std::cout << "WINDOW_SMOKE_PASS frames=" << renderedFrames << '\n';
    }
    return 0;
}
