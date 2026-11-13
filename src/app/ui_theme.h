#pragma once

#include "raylib.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

// Approved interface palette. Intentionally independent of gameplay SKIN:
// changing chart colors and path speed must not recolor application chrome.
struct UiTheme {
    // Nightbloom: soft dark violet-blue, sky blue and lilac; no warm yellow.
    // This is menu chrome only; gameplay SKIN colors are independent.
    Color background{28, 33, 50, 255};      // #1C2132
    Color surface{41, 46, 69, 255};         // #292E45
    Color raised{54, 59, 83, 255};
    Color foreground{238, 241, 251, 255};  // #EEF1FB
    Color secondaryText{172, 182, 203, 255};
    Color softBlue{162, 216, 238, 255};    // #A2D8EE
    Color rose{181, 164, 243, 255};        // Lilac #B5A4F3
    Color onAccent{36, 38, 58, 255};
    Color divider{188, 188, 234, 48};
    Color danger{236, 163, 182, 255};
    float buttonRadiusPx = 14.0f;
    float panelRadiusPx = 14.0f;
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
