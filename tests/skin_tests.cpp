#include "app/skin.h"
#include "app/skin_controls.h"

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
    invalid.cameraStep = 251.0f;
    check(!invalid.Valid(error), "reject invalid camera zoom");
    // Draggers have modest limits, but exact input can go further.
    SkinNumberRange limits;
    check(GetSkinNumberRange(0, 0, limits) &&
          limits.sliderMax == 12.0 && limits.inputMax == 64.0,
          "lookahead slider range and extended typed limit differ");
    check(GetSkinNumberRange(1, 1, limits) &&
          limits.sliderMax == 80.0 && limits.inputMax == 250.0,
          "camera zoom has extended typed limit");
    Skin edited;
    double parsed = 0.0;
    check(ParseSkinNumber("24,75", parsed) && parsed == 24.75,
          "typed decimal comma accepted");
    check(ParseSkinNumber("1.25e1", parsed) && parsed == 12.5,
          "typed decimals and exponent accepted");
    check(!ParseSkinNumber("12px", parsed) &&
          !ParseSkinNumber("nan", parsed) &&
          !ParseSkinNumber("1e999", parsed),
          "invalid, non-finite, and overflowing numbers rejected");
    check(SetSkinNumber(edited, 0, 0, 1, 48.25, false, error) &&
          edited.lookaheadBeats == 48.25f,
          "typed path horizon may exceed the slider's maximum");
    check(edited.Valid(error), "extended input still passes skin serialization validation");
    const float before = edited.lookaheadBeats;
    check(!SetSkinNumber(edited, 0, 0, 1, 65.0, false, error) &&
          edited.lookaheadBeats == before,
          "unsafe typed value rejected without mutation");
    check(SetSkinNumber(edited, 0, 0, 1, 99.0, true, error) &&
          edited.lookaheadBeats == 12.0f,
          "slider clamps to its friendly range");
    check(SetSkinNumber(edited, 0, 4, 1, 0.0, false, error) &&
          edited.pathOpacity == 0.0f && edited.Valid(error),
          "zero opacity is valid through text field and JSON model");
    check(SetSkinNumber(edited, 2, 2, 1, 243.0, false, error) &&
          edited.trajectory.b == 243,
          "color channels support exact typed integers");
    const int savedBlue = edited.trajectory.b;
    check(!SetSkinNumber(edited, 2, 2, 1, 243.5, false, error) &&
          edited.trajectory.b == savedBlue,
          "fractional color channel does not change the value");
    check(SetSkinNumber(edited, 2, 2, 1, 300.0, true, error) &&
          edited.trajectory.b == 255,
          "color slider clamps to eight-bit channel range");
    check(!GetSkinNumberRange(0, 6, limits) &&
          !GetSkinNumberRange(1, 2, limits),
          "booleans are switches, not numeric sliders");
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
