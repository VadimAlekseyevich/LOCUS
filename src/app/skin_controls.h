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

// Page 0: path rows 0..5; page 1: player rows 0,1,3;
// page 2: four RGBA channels (rows 0..3) of selectedColor.
bool GetSkinNumberRange(int page, int row, SkinNumberRange& range) noexcept;
double GetSkinNumber(const Skin& skin, int page, int row,
                     int selectedColor) noexcept;
bool SetSkinNumber(Skin& skin, int page, int row, int selectedColor,
                   double value, bool fromSlider, std::string& error);
bool ParseSkinNumber(std::string_view text, double& result) noexcept;
std::string FormatSkinNumber(double value);

} // namespace locus::app
