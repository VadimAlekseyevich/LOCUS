#pragma once

#include "raylib.h"

namespace locus::app {

// Presentation-only button component. Geometry/hitboxes stay with their
// screen layout; all buttons use the same drawing implementation. Skin,
// menu and editor remain distinct presets so no visual behaviour changes.
struct UiButtonStyle {
    Color fill{};
    Color selectedFill{};
    Color stroke{};
    Color selectedStroke{};
    Color text{};
    float roundness = 0.12f;
    int segments = 5;
    float strokeWidth = 1.5f;
    float selectedStrokeWidth = 1.5f;
    float fontSize = 17.0f;
    float fontSpacing = 1.0f;
};

inline UiButtonStyle SkinButtonStyle() noexcept {
    return {{35, 41, 56, 255}, {49, 69, 82, 255},
            {183, 166, 243, 255}, {244, 210, 137, 255},
            {225, 232, 240, 255}, 0.12f, 4, 1.3f, 1.3f, 15.0f, 1.0f};
}

inline UiButtonStyle MenuButtonStyle(Color tint) noexcept {
    return {{36, 42, 58, 255}, {42, 75, 83, 255},
            tint, tint, {225, 232, 240, 255},
            0.10f, 5, 1.6f, 2.5f, 17.0f, 1.0f};
}

inline UiButtonStyle EditorButtonStyle(Color tint) noexcept {
    return {{36, 43, 58, 255}, {45, 81, 88, 255},
            tint, tint, {232, 238, 245, 255},
            0.16f, 5, 1.7f, 1.7f, 17.0f, 1.0f};
}

inline void DrawUiButtonChrome(Rectangle rect, const UiButtonStyle& style,
                               bool selected = false) {
    DrawRectangleRounded(rect, style.roundness, style.segments,
                         selected ? style.selectedFill : style.fill);
    DrawRectangleRoundedLinesEx(rect, style.roundness, style.segments,
                                selected ? style.selectedStrokeWidth
                                         : style.strokeWidth,
                                selected ? style.selectedStroke : style.stroke);
}

inline void DrawUiButton(Font font, Rectangle rect, const char* caption,
                         const UiButtonStyle& style, bool selected = false) {
    DrawUiButtonChrome(rect, style, selected);
    const Vector2 measured = MeasureTextEx(
        font, caption, style.fontSize, style.fontSpacing);
    DrawTextEx(font, caption,
               {rect.x + (rect.width - measured.x) * 0.5f,
                rect.y + (rect.height - measured.y) * 0.5f},
               style.fontSize, style.fontSpacing, style.text);
}

} // namespace locus::app
