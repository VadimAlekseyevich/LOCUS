#pragma once

#include "raylib.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

// Approved interface palette. Intentionally independent of gameplay SKIN:
// changing chart colors and path speed must not recolor application chrome.
struct UiTheme {
    Color background{32, 33, 41, 255};      // #202129
    Color surface{40, 42, 52, 255};         // #282A34
    Color raised{49, 51, 62, 255};
    Color foreground{240, 236, 237, 255};  // #F0ECED
    Color secondaryText{178, 176, 184, 255};
    Color softBlue{187, 210, 227, 255};    // #BBD2E3
    Color rose{233, 90, 153, 255};         // #E95A99
    Color onAccent{32, 27, 36, 255};
    Color divider{240, 236, 237, 23};
    Color danger{239, 139, 149, 255};
    float buttonRadiusPx = 17.0f;
    float panelRadiusPx = 22.0f;
    float controlHeightPx = 44.0f;
    float gapPx = 12.0f;
    float fontSize = 17.0f;
    float smallFontSize = 13.0f;
};

// One immutable source of semantic colors and geometry for future screens.
inline const UiTheme& ApprovedUiTheme() noexcept {
    static const UiTheme theme{};
    return theme;
}

inline Color UiOpacity(Color color, float opacity) noexcept {
    color.a = static_cast<unsigned char>(std::lround(
        std::clamp(opacity, 0.0f, 1.0f) * color.a));
    return color;
}

inline Color UiMix(Color from, Color to, float progress) noexcept {
    const float t = std::clamp(progress, 0.0f, 1.0f);
    const auto mix = [t](unsigned char a, unsigned char b) {
        return static_cast<unsigned char>(std::lround(
            a + (static_cast<float>(b) - a) * t));
    };
    return {mix(from.r, to.r), mix(from.g, to.g),
            mix(from.b, to.b), mix(from.a, to.a)};
}

// raylib's roundness is relative to rectangle height, not a pixel radius.
// Keep radii consistent across tall and short controls.
inline float UiRoundness(Rectangle bounds, float radiusPx) noexcept {
    return bounds.height <= 0.0f ? 0.0f
        : std::clamp(radiusPx * 2.0f / bounds.height, 0.0f, 1.0f);
}

} // namespace locus::app
