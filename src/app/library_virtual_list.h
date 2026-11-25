#pragma once

#include "app/menu_ui.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace locus::app {

// RmlUi scroll positions and measured element bounds are physical pixels;
// CSS heights, gaps and generated spacer markup are density-independent.
// The virtual list uses dp exclusively, including its prefix sums.
inline float LibraryScrollDp(float pixels, float density) noexcept {
    return pixels / (std::isfinite(density) && density > 0.0f ? density : 1.0f);
}

inline float LibraryScrollPixels(float dp, float density) noexcept {
    return dp * (std::isfinite(density) && density > 0.0f ? density : 1.0f);
}

inline float LibraryResultRowStepDp(float measuredHeightPixels, float density) noexcept {
    return LibraryScrollDp(measuredHeightPixels,density) + 5.0f;
}

// Pure layout model shared by RmlUi and headless tests. Each row retains its
// exact original Rml height; no arbitrary uniform-height approximation.
inline std::vector<float> LibraryRowOffsets(
    const std::vector<LibraryTreeEntry>& entries) {
    std::vector<float> offsets;
    offsets.reserve(entries.size() + 1);
    offsets.push_back(0.0f);
    for (const auto& item : entries)
        offsets.push_back(offsets.back() +
                          (item.chart >= 0 ? 42.0f : 45.0f) + 6.0f);
    return offsets;
}

struct LibraryRowWindow {
    std::size_t first = 0;
    std::size_t end = 0;
    float before = 0.0f;
    float after = 0.0f;
};

// Keep the live RML element count bounded, independent of library size.
// Add overscan rows so ordinary scrolling does not rebuild on every pixel.
// "gap:6dp" in #tree is included in each prefix sum; spacer heights subtract
// the extra gap that RmlUi inserts between spacer and first/last real row.
inline LibraryRowWindow VisibleLibraryRows(
    const std::vector<float>& offsets, float scrollTop,
    std::size_t maxRows = 40, std::size_t overscan = 6) noexcept {
    LibraryRowWindow view;
    if (offsets.size() < 2) return view;
    const auto count = offsets.size() - 1;
    const float position = std::isfinite(scrollTop)
        ? std::max(0.0f, scrollTop) : 0.0f;
    auto found = std::upper_bound(offsets.begin(), offsets.end() - 1, position);
    const auto at = found == offsets.begin()
        ? std::size_t(0)
        : static_cast<std::size_t>(found - offsets.begin() - 1);
    view.first = at > overscan ? at - overscan : 0;
    view.end = std::min(count, view.first + std::max(std::size_t(1), maxRows));
    view.before = view.first == 0 ? 0.0f : offsets[view.first] - 6.0f;
    view.after = view.end == count ? 0.0f
        : offsets.back() - offsets[view.end] - 6.0f;
    return view;
}

// Completed-run rows have uniform height (date labels never wrap).
// The row step is measured in dp from a live RmlUi element after layout, not
// guessed from a CSS min-height that may include padding and borders.
inline LibraryRowWindow VisibleResultRows(
    std::size_t count, float scrollTop, float measuredRowStep,
    std::size_t maxRows = 32, std::size_t overscan = 5,
    float gap = 5.0f) noexcept {
    LibraryRowWindow view;
    if (count == 0) return view;
    const float step = std::isfinite(measuredRowStep) &&
        measuredRowStep >= 5.0f ? measuredRowStep : 43.0f;
    const float top = std::isfinite(scrollTop) ?
        std::max(0.0f, scrollTop) : 0.0f;
    const auto firstVisible = std::min(count - 1, static_cast<std::size_t>(
        std::floor(top / step)));
    view.first = firstVisible > overscan ? firstVisible - overscan : 0;
    view.end = std::min(count, view.first + std::max(std::size_t(1), maxRows));
    view.before = view.first == 0 ? 0.0f : view.first * step - gap;
    view.after = view.end == count ? 0.0f : (count - view.end) * step - gap;
    return view;
}

// Size the working set from the actual viewport, not a hard row cap.
inline std::size_t VirtualRowsForViewport(float heightDp, float stepDp,
                                          std::size_t overscan = 5) noexcept {
    if (!std::isfinite(heightDp) || heightDp <= 0.0f ||
        !std::isfinite(stepDp) || stepDp <= 0.0f) return 32;
    return static_cast<std::size_t>(std::ceil(heightDp / stepDp)) +
        1 + 2 * overscan;
}

} // namespace locus::app
