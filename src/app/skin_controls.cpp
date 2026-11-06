#include "app/skin_controls.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace locus::app {

bool GetSkinNumberRange(int page, int row, SkinNumberRange& range) noexcept {
    if (page == 0) {
        switch (row) {
            case 0: range = {0.5, 12.0, 0.25, 64.0}; return true;
            case 1: range = {1.0, 12.0, 0.25, 40.0}; return true;
            case 2: range = {1.0, 3.0, 1.0, 8.0}; return true;
            case 3: range = {0.5, 2.5, 0.1, 8.0}; return true;
            case 4: range = {0.0, 1.0, 0.0, 1.0}; return true;
            case 5: range = {3.0, 18.0, 1.0, 64.0}; return true;
            default: return false;
        }
    }
    if (page == 1) {
        switch (row) {
            case 0: range = {6.0, 22.0, 1.0, 80.0}; return true;
            case 1: range = {60.0, 320.0, kMinBaseSpeed, kMaxBaseSpeed}; return true;
            case 3: range = {0.5, 2.0, 0.1, 8.0}; return true;
            default: return false;
        }
    }
    if (page == 2 && row >= 0 && row < 4) {
        // RGBA has a hard representational limit: 8-bit integer channels.
        range = {0.0, 255.0, 0.0, 255.0};
        return true;
    }
    return false;
}

double GetSkinNumber(const Skin& skin, int page, int row,
                     int selectedColor) noexcept {
    if (page == 0) {
        switch (row) {
            case 0: return skin.lookaheadBeats;
            case 1: return skin.pathWidth;
            case 2: return skin.outlineScale;
            case 3: return skin.fadePower;
            case 4: return skin.pathOpacity;
            case 5: return skin.junctionRadius;
        }
    } else if (page == 1) {
        switch (row) {
            case 0: return skin.playerRadius;
            case 1: return skin.baseSpeedPxPerSecond;
            case 3: return skin.hitFlashScale;
        }
    } else if (page == 2 && selectedColor >= 0 &&
               selectedColor < kSkinColorCount) {
        const Color color = SkinColor(skin, selectedColor);
        switch (row) {
            case 0: return color.r;
            case 1: return color.g;
            case 2: return color.b;
            case 3: return color.a;
        }
    }
    return 0.0;
}

bool SetSkinNumber(Skin& skin, int page, int row, int selectedColor,
                   double value, bool fromSlider, std::string& error) {
    SkinNumberRange range;
    if (!GetSkinNumberRange(page, row, range) || !std::isfinite(value)) {
        error = "Invalid numeric skin value.";
        return false;
    }
    if (fromSlider)
        value = std::clamp(value, range.sliderMin, range.sliderMax);
    else if (value < range.inputMin || value > range.inputMax) {
        error = "INPUT RANGE: " + FormatSkinNumber(range.inputMin) + " .. " +
                FormatSkinNumber(range.inputMax);
        return false;
    }
    if (page == 2) {
        if (selectedColor < 0 || selectedColor >= kSkinColorCount ||
            (!fromSlider && std::floor(value) != value)) {
            error = "RGBA channels must be whole numbers from 0 to 255.";
            return false;
        }
        const auto byte = static_cast<unsigned char>(std::lround(value));
        Color& color = SkinColor(skin, selectedColor);
        switch (row) {
            case 0: color.r = byte; break;
            case 1: color.g = byte; break;
            case 2: color.b = byte; break;
            case 3: color.a = byte; break;
            default: return false;
        }
    } else {
        // Sliders are rounded to two decimals; typing retains more precision.
        if (fromSlider) value = std::round(value * 100.0) / 100.0;
        const float number = static_cast<float>(value);
        if (page == 0) {
            switch (row) {
                case 0: skin.lookaheadBeats = number; break;
                case 1: skin.pathWidth = number; break;
                case 2: skin.outlineScale = number; break;
                case 3: skin.fadePower = number; break;
                case 4: skin.pathOpacity = number; break;
                case 5: skin.junctionRadius = number; break;
                default: return false;
            }
        } else if (page == 1) {
            switch (row) {
                case 0: skin.playerRadius = number; break;
                case 1: skin.baseSpeedPxPerSecond = number; break;
                case 3: skin.hitFlashScale = number; break;
                default: return false;
            }
        } else return false;
    }
    error.clear();
    return true;
}

bool ParseSkinNumber(std::string_view text, double& result) noexcept {
    if (text.empty() || text.size() > 32) return false;
    std::string normalized(text);
    for (char& c : normalized)
        if (c == ',') c = '.';
    errno = 0;
    char* tail = nullptr;
    const double parsed = std::strtod(normalized.c_str(), &tail);
    if (errno == ERANGE || tail == normalized.c_str() ||
        *tail != '\0' || !std::isfinite(parsed)) return false;
    result = parsed;
    return true;
}

std::string FormatSkinNumber(double value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(8) << value;
    return stream.str();
}

} // namespace locus::app
