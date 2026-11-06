#include "app/skin.h"
#include "app/skin_controls.h"
#include "app/movement_speed.h"
#include <nlohmann/json.hpp>

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
    original.baseSpeedPxPerSecond = 185.0f;
    original.trajectory = Color{12, 99, 180, 222};
    original.fieldBorder = Color{39, 207, 255, 196};
    original.junctionGlow = false;
    original.showHud = false;
    const std::string json = SerializeSkin(original);
    Skin restored;
    std::string error;
    check(ParseSkin(json, restored, error), "round trip parse");
    check(restored.name == original.name &&
          restored.lookaheadBeats == 11.5f &&
          restored.baseSpeedPxPerSecond == 185.0f &&
          restored.trajectory.b == 180 &&
          restored.trajectory.a == 222 &&
          restored.fieldBorder.r == 39 &&
          restored.fieldBorder.g == 207 &&
          restored.fieldBorder.b == 255 &&
          restored.fieldBorder.a == 196 &&
          !restored.junctionGlow && !restored.showHud,
          "all values round trip");
    check(kSkinColorCount == 10 &&
          std::string(SkinColorLabel(9)) == "FIELD BORDER" &&
          SkinColor(original, 9).g == 207,
          "square border is available as tenth editable RGBA colour");
    auto oldFile = nlohmann::json::parse(json);
    oldFile["version"] = 1;
    oldFile["player"].erase("speedPxPerSecond");
    oldFile["player"]["cameraStep"] = 70.0;
    oldFile["colors"].erase("fieldBorder");
    Skin migrated;
    check(ParseSkin(oldFile.dump(), migrated, error) &&
          migrated.fieldBorder.r == Skin{}.fieldBorder.r &&
          migrated.fieldBorder.a == 255 &&
          std::abs(migrated.baseSpeedPxPerSecond -
                   70.0f * kDefaultBaseSpeed / 51.0f) < 0.001f,
          "v1 skin zoom migrates to fixed speed and frame colour");
    oldFile["colors"]["fieldBorder"] = nlohmann::json::array({256, 0, 0, 255});
    Skin unmodified;
    unmodified.name = "Keep";
    check(!ParseSkin(oldFile.dump(), unmodified, error) &&
          unmodified.name == "Keep",
          "invalid border channel rejected atomically");
    auto invalidLegacy = oldFile;
    invalidLegacy["colors"]["fieldBorder"] = nlohmann::json::array({255,255,255,255});
    invalidLegacy["player"]["cameraStep"] = 251;
    Skin legacyGuard;
    check(!ParseSkin(invalidLegacy.dump(), legacyGuard, error),
          "reject invalid legacy zoom without mutation");
    Skin sentinel;
    sentinel.name = "Unchanged";
    check(!ParseSkin("{\"version\":1}", sentinel, error) &&
          sentinel.name == "Unchanged", "incomplete JSON is atomic");
    auto invalid = original;
    invalid.lookaheadBeats = 0.0f;
    check(!invalid.Valid(error), "reject invalid lookahead");
    invalid = original;
    invalid.baseSpeedPxPerSecond = 701.0f;
    check(!invalid.Valid(error), "reject invalid base speed");
    // Draggers have modest limits, but exact input can go further.
    SkinNumberRange limits;
    check(GetSkinNumberRange(0, 0, limits) &&
          limits.sliderMax == 12.0 && limits.inputMax == 64.0,
          "lookahead slider range and extended typed limit differ");
    check(GetSkinNumberRange(1, 1, limits) &&
          limits.sliderMax == 320.0 && limits.inputMax == 700.0,
          "fixed speed has extended typed limit");
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
    check(SetSkinNumber(edited, 1, 1, 1, 235.25, false, error) &&
          std::abs(edited.baseSpeedPxPerSecond - 235.25f) < 0.001f,
          "typed speed is stored in skin");
    check(!SetSkinNumber(edited, 1, 1, 1, 701.0, false, error) &&
          std::abs(edited.baseSpeedPxPerSecond - 235.25f) < 0.001f,
          "invalid speed is not applied");
    check(AdjustBaseSpeed(135.0f, 1) == 145.0f &&
          AdjustBaseSpeed(135.0f, -1) == 125.0f &&
          AdjustBaseSpeed(kMaxBaseSpeed, 1) == kMaxBaseSpeed,
          "hotkeys change and clamp the persisted base speed");
    for (double bpm : {60.0, 120.0, 250.0})
        for (int ticks : {1, 6, 24})
            for (double practice : {1.0, 0.75, 0.5, 0.1}) {
                const double step = StepForFixedSpeed(135.0f, bpm, ticks, practice);
                const double rate = bpm * ticks * practice / 60.0;
                check(std::abs(step * rate - 135.0) < 0.001,
                      "cardinal speed fixed across BPM/ticks/practice");
                check(std::abs(std::hypot(step / std::sqrt(2.0),
                                       step / std::sqrt(2.0)) * rate -
                               135.0) < 0.001,
                      "normalized diagonal speed equals cardinal");
            }
    check(StepForFixedSpeed(135.0f, 0.0, 1) == 0.0f &&
          StepForFixedSpeed(135.0f, 120.0, 0) == 0.0f,
          "invalid chart timing does not generate invalid camera step");
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
    check(SetSkinNumber(edited, 2, 0, 9, 111.0, false, error) &&
          edited.fieldBorder.r == 111,
          "frame border R channel supports exact RGBA editing");
    check(SetSkinNumber(edited, 2, 3, 9, 156.0, false, error) &&
          edited.fieldBorder.a == 156,
          "frame border transparency adjustable from skin editor");
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
