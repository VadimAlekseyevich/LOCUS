#pragma once

#include "raylib.h"
#include "app/ui_theme.h"
#include "app/ui_motion.h"
#include "app/ui_text.h"

#include <algorithm>

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
    const auto& theme = ApprovedUiTheme();
    return {theme.surface, theme.raised, theme.rose, theme.softBlue,
            theme.foreground, 0.20f, 8, 1.0f, 1.5f, 15.0f, 1.0f};
}

inline UiButtonStyle MenuButtonStyle(Color tint) noexcept {
    const auto& theme = ApprovedUiTheme();
    return {theme.surface, UiMix(theme.surface, tint, 0.28f),
            UiOpacity(tint, 0.67f), tint, theme.foreground,
            0.23f, 10, 1.1f, 1.6f, 17.0f, 1.0f};
}

inline UiButtonStyle EditorButtonStyle(Color tint) noexcept {
    const auto& theme = ApprovedUiTheme();
    return {theme.surface, UiMix(theme.surface, tint, 0.25f),
            UiOpacity(tint, 0.65f), tint, theme.foreground,
            0.22f, 10, 1.1f, 1.6f, 17.0f, 1.0f};
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


enum class UiButtonVariant { Rose, Blue, Neutral, Cool, Warm, Quiet, Danger };

// New semantic styles reuse the existing UiButtonStyle and chrome renderer.
// Legacy Menu/Skin/Editor styles stay untouched until each screen is approved.
inline UiButtonStyle ApprovedButtonStyle(
    UiButtonVariant variant,
    const UiTheme& theme = ApprovedUiTheme()) noexcept {
    UiButtonStyle style{};
    style.fontSize = theme.fontSize;
    style.fontSpacing = 1.0f;
    style.roundness = 0.25f; // Overridden using actual pixel radius at draw.
    style.segments = 12;
    style.strokeWidth = 1.0f;
    style.selectedStrokeWidth = 1.0f;
    style.text = theme.foreground;
    style.fill = theme.surface;
    style.selectedFill = theme.raised;
    style.stroke = theme.divider;
    style.selectedStroke = theme.softBlue;
    switch (variant) {
        case UiButtonVariant::Rose:
            style.fill = theme.rose;
            style.selectedFill = UiMix(theme.rose, theme.foreground, 0.17f);
            style.stroke = theme.rose;
            style.selectedStroke = style.selectedFill;
            style.text = theme.onAccent;
            break;
        case UiButtonVariant::Blue:
            style.fill = theme.softBlue;
            style.selectedFill = UiMix(theme.softBlue, theme.foreground, 0.22f);
            style.stroke = theme.softBlue;
            style.selectedStroke = style.selectedFill;
            style.text = theme.onAccent;
            break;
        case UiButtonVariant::Neutral:
            break;
        case UiButtonVariant::Cool:
            // Stronger secondary presence without competing with Play/Edit.
            style.fill = UiMix(theme.surface, theme.softBlue, 0.35f);
            style.selectedFill = UiMix(theme.surface, theme.softBlue, 0.51f);
            style.stroke = UiMix(theme.surface, theme.softBlue, 0.67f);
            style.selectedStroke = theme.softBlue;
            break;
        case UiButtonVariant::Warm:
            style.fill = UiMix(theme.surface, theme.rose, 0.25f);
            style.selectedFill = UiMix(theme.surface, theme.rose, 0.40f);
            style.stroke = UiMix(theme.surface, theme.rose, 0.63f);
            style.selectedStroke = theme.rose;
            break;
        case UiButtonVariant::Quiet:
            // Exit is a real button with a deliberate hit area and clear
            // border, not an almost invisible line of text.
            style.fill = UiMix(theme.background, theme.surface, 0.80f);
            style.selectedFill = UiMix(theme.surface, theme.rose, 0.18f);
            style.stroke = UiOpacity(theme.secondaryText, 0.46f);
            style.selectedStroke = UiOpacity(theme.rose, 0.86f);
            style.text = theme.foreground;
            break;
        case UiButtonVariant::Danger:
            style.fill = theme.surface;
            style.selectedFill = UiMix(theme.surface, theme.danger, 0.18f);
            style.stroke = theme.danger;
            style.selectedStroke = theme.danger;
            break;
    }
    return style;
}

// Visual-only animation. The label may move at most 1 pixel; the actual
// layout/hit rectangle NEVER changes, including under hover or press.
inline void DrawUiThemedButton(
    Font font, Rectangle bounds, const char* caption,
    UiButtonVariant variant, UiInteraction state = {},
    UiMotionFrame animation = {},
    const UiTheme& theme = ApprovedUiTheme(),
    float fontSizeOverride = 0.0f) {
    auto style = ApprovedButtonStyle(variant, theme);
    if (fontSizeOverride > 0.0f) style.fontSize = fontSizeOverride;
    const float interaction = std::max(animation.hover, animation.selected);
    style.fill = UiMix(style.fill, style.selectedFill, interaction);
    style.stroke = UiMix(style.stroke, style.selectedStroke, interaction);
    style.roundness = UiRoundness(bounds, theme.buttonRadiusPx);
    if (state.disabled) {
        style.fill = UiOpacity(style.fill, 0.45f);
        style.stroke = UiOpacity(style.stroke, 0.35f);
        style.text = UiOpacity(style.text, 0.45f);
    }
    DrawUiButtonChrome(bounds, style, false);
    if (!state.disabled && animation.focus > 0.0f) {
        const Rectangle ring{bounds.x - 3.0f, bounds.y - 3.0f,
                             bounds.width + 6.0f, bounds.height + 6.0f};
        DrawRectangleRoundedLinesEx(ring,
            UiRoundness(ring, theme.buttonRadiusPx + 3.0f), 12, 1.5f,
            UiOpacity(theme.rose, animation.focus));
    }
    const std::string label = UiFitText(
        font, caption ? caption : "", std::max(0.0f, bounds.width - 20.0f),
        style.fontSize, style.fontSpacing);
    if (label.empty()) return;
    const Vector2 measured = MeasureTextEx(
        font, label.c_str(), style.fontSize, style.fontSpacing);
    DrawTextEx(font, label.c_str(),
        {bounds.x + (bounds.width - measured.x) * 0.5f,
         bounds.y + (bounds.height - measured.y) * 0.5f +
              (state.disabled ? 0.0f : animation.press)},
        style.fontSize, style.fontSpacing, style.text);
}

} // namespace locus::app
