// F-02 only: verify pinned third-party dependencies compile and link together.
// This is not a game window and does not require an audio or display device.
#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <nlohmann/json.hpp>

#include <iostream>

static_assert(RAYLIB_VERSION_MAJOR == 6 && RAYLIB_VERSION_MINOR == 0,
              "The verified renderer dependency must remain raylib 6.0.");
static_assert(RAYGUI_VERSION_MAJOR == 5 && RAYGUI_VERSION_MINOR == 0,
              "The verified GUI dependency must remain raygui 5.0.");

int main() {
    const auto parsed = nlohmann::json::parse(R"({"formatVersion":1})");
    const Color pixel = GetColor(0xff0000ffU);
    if (parsed.value("formatVersion", 0) != 1 || pixel.r != 255 ||
        pixel.g != 0 || pixel.b != 0 || pixel.a != 255) {
        std::cerr << "DEPENDENCY_SMOKE_FAIL\n";
        return 1;
    }

    // RAYGUI_IMPLEMENTATION above compiles the real raygui functions against
    // the pinned raylib headers. No UI calls are needed during headless checks.
    std::cout << "DEPENDENCY_SMOKE_PASS\n"
              << "raylib=" << RAYLIB_VERSION << '\n'
              << "raygui=" << RAYGUI_VERSION << '\n'
              << "json=" << NLOHMANN_JSON_VERSION_MAJOR << '.'
              << NLOHMANN_JSON_VERSION_MINOR << '.'
              << NLOHMANN_JSON_VERSION_PATCH << '\n';
    return 0;
}
