#pragma once

#include "app/menu_ui.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace locus::app {

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

} // namespace locus::app
