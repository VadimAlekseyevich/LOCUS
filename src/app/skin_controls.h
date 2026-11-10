#pragma once

#include "app/skin.h"

#include <string>
#include <string_view>

namespace locus::app {

// The slider is a convenient everyday range. Typed values may reach the
// broader safety limits without being snapped into the slider range.
struct SkinNumberRange {
    double sliderMin = 0.0;
    double sliderMax = 1.0;
    double inputMin = 0.0;
    double inputMax = 1.0;
};

// Each PATH/PLAYER row is described exactly once here: its label,
// field binding, widget type, numeric limits, and editing precision.
// COLORS uses independent RGBA-channel controls.
struct SkinSetting {
    const char* label = "";
    SkinNumberRange numberRange{};
    float Skin::*number = nullptr;
    bool Skin::*toggle = nullptr;
    bool wholeMilliseconds = false;
};
const SkinSetting* GetSkinSetting(int page, int row) noexcept;
int SkinSettingCount(int page) noexcept;
bool GetSkinToggle(const Skin& skin, int page, int row) noexcept;
bool FlipSkinToggle(Skin& skin, int page, int row) noexcept;

// Page 0/1 use the setting catalog; page 2 is four RGBA channels.
bool GetSkinNumberRange(int page, int row, SkinNumberRange& range) noexcept;
double GetSkinNumber(const Skin& skin, int page, int row,
                     int selectedColor) noexcept;
bool SetSkinNumber(Skin& skin, int page, int row, int selectedColor,
                   double value, bool fromSlider, std::string& error);
bool ParseSkinNumber(std::string_view text, double& result) noexcept;
std::string FormatSkinNumber(double value);

// HSV wheel + saturation/value square are used by COLORS instead of sliders.
// Hex is accepted as #RRGGBB or #RRGGBBAA; six digits preserve old alpha.
struct SkinHsv { float h = 0.0f, s = 0.0f, v = 0.0f; };
SkinHsv SkinColorToHsv(Color color) noexcept;
Color SkinColorFromHsv(SkinHsv hsv, unsigned char alpha = 255) noexcept;
std::string FormatSkinHex(Color color);
bool ParseSkinHex(std::string_view value, Color previous,
                  Color& result) noexcept;

} // namespace locus::app
