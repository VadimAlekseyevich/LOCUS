#pragma once

#include "app/ui_button.h"
#include "app/ui_focus.h"
#include "app/ui_theme.h"
#include "core/utf8.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace locus::app {

// Shared drawing primitives deliberately contain no input polling or
// persistence. A screen controller updates models; the same layout
// rectangles are passed to drawing and UiPointerHit / UiFocusChain.

inline void DrawUiPanel(Rectangle bounds, bool elevated = false,
                        const UiTheme& theme = ApprovedUiTheme()) {
    const float radius = UiRoundness(bounds, theme.panelRadiusPx);
    DrawRectangleRounded(bounds, radius, 12,
                         elevated ? theme.raised : theme.surface);
    DrawRectangleRoundedLinesEx(bounds, radius, 12, 1.0f, theme.divider);
}

inline void DrawUiLabel(Font font, const char* caption, Vector2 position,
                        bool secondary = false, float size = 0.0f,
                        const UiTheme& theme = ApprovedUiTheme()) {
    DrawTextEx(font, caption, position, size > 0 ? size : theme.fontSize,
               1.0f, secondary ? theme.secondaryText : theme.foreground);
}

inline void DrawUiToggle(Rectangle bounds, bool checked,
                         UiInteraction state = {},
                         UiMotionFrame animation = {},
                         const UiTheme& theme = ApprovedUiTheme()) {
    const float t = state.disabled ? (checked ? 1.0f : 0.0f)
                                  : animation.selected;
    const Color active = UiMix(theme.raised, theme.softBlue, t);
    const Color base = state.disabled ? UiOpacity(active, 0.45f) : active;
    DrawRectangleRounded(bounds, 1.0f, 14, base);
    const float radius = std::max(0.0f, bounds.height * 0.5f - 5.0f);
    const float x = bounds.x + bounds.height * 0.5f +
                    (bounds.width - bounds.height) * t;
    DrawCircleV({x, bounds.y + bounds.height * 0.5f}, radius,
                UiOpacity(checked ? theme.onAccent : theme.foreground,
                          state.disabled ? 0.45f : 1.0f));
    if (state.focused && !state.disabled)
        DrawRectangleRoundedLinesEx(
            {bounds.x - 3, bounds.y - 3,
             bounds.width + 6, bounds.height + 6}, 1.0f, 14,
            1.5f, theme.rose);
}

struct UiTextBuffer {
    std::string text;
    std::size_t maxCodepoints = 64;
    std::size_t maxBytes = 256;

    bool Append(std::string_view utf8) {
        return core::utf8::AppendLimited(
            text, utf8, maxCodepoints, maxBytes);
    }
    void Backspace() noexcept { core::utf8::PopBack(text); }
};

inline void DrawUiTextField(
    Font font, Rectangle bounds, std::string_view value,
    const char* placeholder, bool focused = false,
    bool invalid = false, bool disabled = false,
    const UiTheme& theme = ApprovedUiTheme()) {
    const float radius = UiRoundness(bounds, theme.buttonRadiusPx);
    DrawRectangleRounded(bounds, radius, 10,
                         disabled ? UiOpacity(theme.surface, 0.5f)
                                  : theme.surface);
    DrawRectangleRoundedLinesEx(bounds, radius, 10,
                                focused ? 1.7f : 1.0f,
                                invalid ? theme.danger
                                  : focused ? theme.softBlue
                                            : theme.divider);
    const bool empty = value.empty();
    const std::string displayed = empty ? placeholder
        : core::utf8::Truncate(value,
             static_cast<std::size_t>(std::max(1.0f, bounds.width / 11.0f)));
    const float size = theme.fontSize;
    DrawTextEx(font, displayed.c_str(),
        {bounds.x + 14.0f, bounds.y + (bounds.height - size) * 0.5f - 1.0f},
        size, 1.0f,
        UiOpacity(empty || disabled ? theme.secondaryText : theme.foreground,
                  disabled ? 0.5f : 1.0f));
    if (focused && !disabled) {
        const float advance = MeasureTextEx(
            font, displayed.c_str(), size, 1.0f).x;
        const float caret = std::min(bounds.x + bounds.width - 11.0f,
                                     bounds.x + 16.0f + advance);
        DrawLineEx({caret, bounds.y + 12.0f},
                   {caret, bounds.y + bounds.height - 12.0f},
                   1.4f, theme.rose);
    }
}

inline float UiSliderFraction(float value, float low, float high) noexcept {
    if (!std::isfinite(value) || !std::isfinite(low) ||
        !std::isfinite(high) || high <= low) return 0.0f;
    return std::clamp((value - low) / (high - low), 0.0f, 1.0f);
}

inline float UiSliderFromPointer(Rectangle bounds, Vector2 position,
                                 float low, float high,
                                 float step = 0.0f) noexcept {
    if (!std::isfinite(low) || !std::isfinite(high) ||
        high <= low || bounds.width <= 0.0f) return low;
    const float progress = std::clamp(
        (position.x - bounds.x) / bounds.width, 0.0f, 1.0f);
    float value = low + (high - low) * progress;
    if (step > 0.0f && std::isfinite(step))
        value = low + std::round((value - low) / step) * step;
    return std::clamp(value, low, high);
}

inline void DrawUiSlider(Rectangle bounds, float value,
                         float low, float high, bool disabled = false,
                         bool focused = false,
                         const UiTheme& theme = ApprovedUiTheme()) {
    const float progress = UiSliderFraction(value, low, high);
    const Rectangle track{bounds.x, bounds.y + bounds.height * 0.5f - 3.0f,
                          bounds.width, 6.0f};
    DrawRectangleRounded(track, 1.0f, 12, theme.raised);
    const Rectangle filled{track.x, track.y,
                           track.width * progress, track.height};
    if (filled.width > 0.0f)
        DrawRectangleRounded(filled, 1.0f, 12,
                             UiOpacity(theme.softBlue, disabled ? 0.45f : 1.0f));
    const Vector2 knob{bounds.x + bounds.width * progress,
                        bounds.y + bounds.height * 0.5f};
    DrawCircleV(knob, 9.0f,
                UiOpacity(theme.foreground, disabled ? 0.45f : 1.0f));
    if (focused && !disabled)
        DrawCircleLines(static_cast<int>(knob.x),
                        static_cast<int>(knob.y), 12.0f, theme.rose);
}

inline void DrawUiTab(Font font, Rectangle bounds, const char* caption,
                      bool selected, bool focused = false,
                      const UiTheme& theme = ApprovedUiTheme()) {
    UiInteraction interaction{};
    interaction.selected = selected;
    interaction.focused = focused;
    UiMotionFrame frame{};
    frame.selected = selected ? 1.0f : 0.0f;
    frame.focus = focused ? 1.0f : 0.0f;
    DrawUiThemedButton(font, bounds, caption, UiButtonVariant::Neutral,
                       interaction, frame, theme);
}

inline void DrawUiDialogSurface(
    Rectangle bounds, const char* title, Font font,
    float opacity = 1.0f, float translateY = 0.0f,
    const UiTheme& theme = ApprovedUiTheme()) {
    if (opacity <= 0.0f) return;
    // Scrim and panel are visual only. Modal input exclusivity belongs
    // to the screen/controller (as with existing quit confirmation).
    DrawRectangle(0, 0, 960, 540,
                  UiOpacity(theme.background, opacity * 0.82f));
    bounds.y += translateY;
    const float radius = UiRoundness(bounds, theme.panelRadiusPx);
    DrawRectangleRounded(bounds, radius, 12,
                         UiOpacity(theme.surface, opacity));
    DrawRectangleRoundedLinesEx(bounds, radius, 12, 1.0f,
                                UiOpacity(theme.divider, opacity));
    DrawTextEx(font, title, {bounds.x + 24.0f, bounds.y + 24.0f},
               23.0f, 1.0f, UiOpacity(theme.foreground, opacity));
}

inline void DrawUiListRow(Font font, Rectangle bounds,
                          const char* text, bool selected,
                          bool focused = false, bool disabled = false,
                          const UiTheme& theme = ApprovedUiTheme()) {
    DrawRectangleRounded(bounds,
        UiRoundness(bounds, theme.buttonRadiusPx * 0.7f), 10,
        selected ? theme.raised : UiOpacity(theme.surface, 0.0f));
    if (focused && !disabled)
        DrawRectangleRoundedLinesEx(bounds,
            UiRoundness(bounds, theme.buttonRadiusPx * 0.7f),
            10, 1.0f, theme.rose);
    DrawTextEx(font, text,
        {bounds.x + 14.0f,
         bounds.y + (bounds.height - theme.fontSize) * 0.5f - 1.0f},
        theme.fontSize, 1.0f,
        UiOpacity(selected ? theme.foreground : theme.secondaryText,
                  disabled ? 0.45f : 1.0f));
}

inline void DrawUiToast(Font font, Rectangle bounds,
                        const char* message, bool error = false,
                        float opacity = 1.0f,
                        const UiTheme& theme = ApprovedUiTheme()) {
    if (opacity <= 0.0f) return;
    DrawRectangleRounded(bounds,
        UiRoundness(bounds, theme.buttonRadiusPx), 10,
        UiOpacity(theme.raised, opacity));
    DrawRectangleRoundedLinesEx(bounds,
        UiRoundness(bounds, theme.buttonRadiusPx), 10, 1.0f,
        UiOpacity(error ? theme.danger : theme.divider, opacity));
    DrawTextEx(font, message, {bounds.x + 16.0f, bounds.y + 12.0f},
               theme.smallFontSize, 1.0f,
               UiOpacity(error ? theme.danger : theme.foreground, opacity));
}

} // namespace locus::app
