#pragma once

#include "core/utf8.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace locus::app {

// Menus/editor only. Fit glyphs to real pixel width, not an estimated
// UTF-8 character count. This does not alter the stored text or its cursor.
// Suffix mode keeps the most recent characters visible in an active input.
enum class UiTextOverflow { EllipsisAtEnd, EllipsisAtStart };

template<class Measure>
inline std::string UiFitText(std::string_view source, float maxWidth,
                             Measure measure,
                             UiTextOverflow overflow = UiTextOverflow::EllipsisAtEnd) {
    if (!std::isfinite(maxWidth) || maxWidth <= 0.0f ||
        !core::utf8::Valid(source)) return {};

    const std::string full(source);
    if (measure(full) <= maxWidth) return full;
    const std::string dots = "...";
    if (measure(dots) > maxWidth) return {};

    std::vector<std::size_t> boundaries{0};
    std::size_t offset = 0;
    while (offset < source.size()) {
        std::uint32_t point = 0;
        if (!core::utf8::Next(source, offset, point)) return {};
        boundaries.push_back(offset);
    }
    const std::size_t count = boundaries.size() - 1;
    const auto clipped = [&](std::size_t kept) {
        return overflow == UiTextOverflow::EllipsisAtEnd
            ? full.substr(0, boundaries[kept]) + dots
            : dots + full.substr(boundaries[count - kept]);
    };

    // Binary search by Unicode scalar boundaries; each candidate is measured
    // with the actual font and spacing that will be used to draw it.
    std::size_t low = 0, high = count;
    while (low < high) {
        const std::size_t mid = low + (high - low + 1) / 2;
        if (measure(clipped(mid)) <= maxWidth) low = mid;
        else high = mid - 1;
    }
    return clipped(low);
}

inline std::string UiFitText(Font font, std::string_view source,
                             float maxWidth, float fontSize,
                             float spacing = 1.0f,
                             UiTextOverflow overflow =
                                 UiTextOverflow::EllipsisAtEnd) {
    return UiFitText(source, maxWidth,
        [&](const std::string& text) {
            return MeasureTextEx(font, text.c_str(), fontSize, spacing).x;
        }, overflow);
}

} // namespace locus::app
