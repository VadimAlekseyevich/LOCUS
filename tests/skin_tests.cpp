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
    original.lookaheadMs = 1150.0f;
    original.baseSpeedPxPerSecond = 185.0f;
    original.trajectory = Color{12, 99, 180, 222};
    original.fieldBorder = Color{39, 207, 255, 196};
    original.junctionGlow = false;
    original.reverseCapScale = 4.75f;
    original.showHud = false;
    const std::string json = SerializeSkin(original);
    Skin restored;
    std::string error;
    check(ParseSkin(json, restored, error), "round trip parse");
    check(restored.name == original.name &&
          restored.lookaheadMs == 1150.0f &&
          restored.baseSpeedPxPerSecond == 185.0f &&
          restored.trajectory.b == 180 &&
          restored.trajectory.a == 222 &&
          restored.fieldBorder.r == 39 &&
          restored.fieldBorder.g == 207 &&
          restored.fieldBorder.b == 255 &&
          restored.fieldBorder.a == 196 &&
          restored.reverseCapScale == 4.75f &&
          !restored.junctionGlow && !restored.showHud,
          "all values round trip");
    check(kSkinColorCount == 10 &&
          std::string(SkinColorLabel(9)) == "FIELD BORDER" &&
          SkinColor(original, 9).g == 207,
          "square border is available as tenth editable RGBA colour");
    auto withoutCap = nlohmann::json::parse(json);
    withoutCap["path"].erase("reverseCapScale");
    Skin olderProfile;
    check(ParseSkin(withoutCap.dump(), olderProfile, error) &&
          olderProfile.reverseCapScale == kDefaultReverseCapScale &&
          kDefaultReverseCapScale == 3.2f,
          "preexisting skin JSON adopts the doubled default cap size");
    auto newFile = nlohmann::json::parse(json);
    check(newFile["path"].contains("lookaheadMs") &&
          !newFile["path"].contains("lookaheadBeats") &&
          newFile["path"]["lookaheadMs"] == 1150.0f,
          "new skin saves only millisecond preview duration");
    auto oldFile = nlohmann::json::parse(json);
    oldFile["version"] = 1;
    oldFile["path"].erase("lookaheadMs");
    oldFile["path"]["lookaheadBeats"] = 4.0f;
    oldFile["player"].erase("speedPxPerSecond");
    oldFile["player"]["cameraStep"] = 70.0;
    oldFile["colors"].erase("fieldBorder");
    Skin migrated;
    check(ParseSkin(oldFile.dump(), migrated, error) &&
          migrated.lookaheadMs == 800.0f &&
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
    auto brokenCap = nlohmann::json::parse(json);
    brokenCap["path"]["reverseCapScale"] = 13.0;
    Skin intact;
    intact.name = "Retain";
    check(!ParseSkin(brokenCap.dump(), intact, error) &&
          intact.name == "Retain",
          "unsafe cap size is rejected atomically when parsing JSON");
    brokenCap["path"]["reverseCapScale"] = -0.5;
    check(!ParseSkin(brokenCap.dump(), intact, error) &&
          intact.name == "Retain",
          "negative cap size is invalid");
    check(Skin{}.lookaheadMs == 800.0f, "default 800 ms preview");
    auto invalid = original;
    invalid.lookaheadMs = 0.0f;
    check(!invalid.Valid(error), "reject invalid lookahead");
    invalid = original;
    invalid.baseSpeedPxPerSecond = 701.0f;
    check(!invalid.Valid(error), "reject invalid base speed");
    // Draggers have modest limits, but exact input can go further.
    SkinNumberRange limits;
    check(GetSkinNumberRange(0, 0, limits) &&
          limits.sliderMax == 2000.0 && limits.inputMax == 10000.0,
          "lookahead slider range and extended typed limit differ");
    check(FormatSkinNumber(1.8200001) == "1.82" &&
          FormatSkinNumber(2.00000001) == "2" &&
          FormatSkinNumber(800.0f) == "800" &&
          FormatSkinNumber(-0.000001) == "0",
          "skin UI hides floating-point residue and trailing zeroes");
    check(GetSkinNumberRange(0, 7, limits) &&
          limits.sliderMin == 0.0 && limits.sliderMax == 6.0 &&
          limits.inputMin == 0.0 && limits.inputMax == 12.0,
          "reverse cap size has an editable slider and an extended typed range");
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
    check(SetSkinNumber(edited, 0, 0, 1, 4800.25, false, error) &&
          edited.lookaheadMs == 4800.0f,
          "typed path horizon may exceed the slider's maximum");
    check(edited.Valid(error), "extended input still passes skin serialization validation");
    check(SetSkinNumber(edited, 0, 2, 1, 1.8200001, false, error) &&
          std::abs(edited.outlineScale - 1.82f) < 0.00001f &&
          FormatSkinNumber(edited.outlineScale) == "1.82",
          "typed outline scale uses hundredths and renders without noise");
    check(SetSkinNumber(edited, 0, 0, 1, 845.67, true, error) &&
          edited.lookaheadMs == 846.0f,
          "preview slider always rounds to whole milliseconds");
    check(GetSkinNumber(edited, 0, 7, 1) == kDefaultReverseCapScale &&
          SetSkinNumber(edited, 0, 7, 1, 5.5, false, error) &&
          edited.reverseCapScale == 5.5f &&
          SetSkinNumber(edited, 0, 7, 1, 0.0, false, error) &&
          edited.reverseCapScale == 0.0f && edited.Valid(error),
          "skin controls change cap size and zero hides the cap");
    check(!SetSkinNumber(edited, 0, 7, 1, 12.5, false, error) &&
          edited.reverseCapScale == 0.0f,
          "out-of-range cap size cannot change the draft");
    check(SetSkinNumber(edited, 0, 7, 1, 99.0, true, error) &&
          edited.reverseCapScale == 6.0f,
          "cap slider clamps to its quick-edit range");
    const auto editedCapJson = SerializeSkin(edited);
    Skin savedCap;
    check(ParseSkin(editedCapJson, savedCap, error) &&
          savedCap.reverseCapScale == 6.0f,
          "selected reverse cap size survives skin serialization");
    const float before = edited.lookaheadMs;
    check(!SetSkinNumber(edited, 0, 0, 1, 10001.0, false, error) &&
          edited.lookaheadMs == before,
          "unsafe typed value rejected without mutation");
    check(SetSkinNumber(edited, 0, 0, 1, 100.0, true, error) &&
          edited.lookaheadMs == 200.0f,
          "preview slider clamps low values to 200 ms");
    check(SetSkinNumber(edited, 0, 0, 1, 10001.0, true, error) &&
          edited.lookaheadMs == 2000.0f,
          "preview slider clamps high values to 2000 ms");
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
    Color hexResult{7, 8, 9, 77};
    check(ParseSkinHex("#12AAbb", hexResult, hexResult) &&
          hexResult.r == 0x12 && hexResult.g == 0xAA &&
          hexResult.b == 0xBB && hexResult.a == 77,
          "six digit hex updates RGB and keeps alpha");
    check(ParseSkinHex("#FF800040", hexResult, hexResult) &&
          FormatSkinHex(hexResult) == "#FF800040",
          "eight digit hex updates alpha and round trips");
    const auto beforeHex = hexResult;
    check(!ParseSkinHex("#GGGGGG", hexResult, hexResult) &&
          hexResult.r == beforeHex.r && hexResult.a == beforeHex.a,
          "invalid hex does not mutate color");
    const auto wheelColor = SkinColorFromHsv({0.5f, 0.75f, 0.8f}, 120);
    const auto wheelHsv = SkinColorToHsv(wheelColor);
    const auto converted = SkinColorFromHsv(wheelHsv, 120);
    check(std::abs(static_cast<int>(converted.r) - wheelColor.r) <= 1 &&
          std::abs(static_cast<int>(converted.g) - wheelColor.g) <= 1 &&
          std::abs(static_cast<int>(converted.b) - wheelColor.b) <= 1 &&
          converted.a == 120,
          "HSV wheel round trips colored pixels without losing alpha");
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
          loaded.lookaheadMs == original.lookaheadMs,
          "load from independent file");
    std::error_code ec;
    std::filesystem::remove_all(file.parent_path(), ec);
    if (ok) std::cout << "SKIN_PASS independent_json=1\n";
    return ok ? 0 : 1;
}
