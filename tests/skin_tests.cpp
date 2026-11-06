#include "app/skin.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

using namespace locus::app;

int main() {
    bool ok = true;
    const auto check = [&](bool pass, const char* what) {
        if (!pass) std::cerr << "SKIN_FAIL " << what << '\n';
        ok &= pass;
    };
    Skin original;
    original.name = "Blue practice";
    original.lookaheadBeats = 11.5f;
    original.cameraStep = 70.0f;
    original.trajectory = Color{12, 99, 180, 222};
    original.junctionGlow = false;
    original.showHud = false;
    const std::string json = SerializeSkin(original);
    Skin restored;
    std::string error;
    check(ParseSkin(json, restored, error), "round trip parse");
    check(restored.name == original.name &&
          restored.lookaheadBeats == 11.5f &&
          restored.cameraStep == 70.0f &&
          restored.trajectory.b == 180 &&
          restored.trajectory.a == 222 &&
          !restored.junctionGlow && !restored.showHud,
          "all values round trip");
    Skin sentinel;
    sentinel.name = "Unchanged";
    check(!ParseSkin("{\"version\":1}", sentinel, error) &&
          sentinel.name == "Unchanged", "incomplete JSON is atomic");
    auto invalid = original;
    invalid.lookaheadBeats = 0.0f;
    check(!invalid.Valid(error), "reject invalid lookahead");
    invalid = original;
    invalid.cameraStep = 150.0f;
    check(!invalid.Valid(error), "reject invalid camera zoom");
    const auto file = std::filesystem::temp_directory_path() /
        "locus_skin_test" / "practice.locus-skin.json";
    check(SaveSkin(file, original, error), "save to independent file");
    Skin loaded;
    check(LoadSkin(file, loaded, error) &&
          loaded.lookaheadBeats == original.lookaheadBeats,
          "load from independent file");
    std::error_code ec;
    std::filesystem::remove_all(file.parent_path(), ec);
    if (ok) std::cout << "SKIN_PASS independent_json=1\n";
    return ok ? 0 : 1;
}
