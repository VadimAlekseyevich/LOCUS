#pragma once

#include <algorithm>
#include <cmath>

namespace locus::app {

// Shared presentation scale for all RmlUi contexts and custom UI canvases.
// The reference dimensions express the preferred visual size of desktop
// controls, not a coordinate system: the real window still owns layout.
// Additional width on an ultrawide window becomes usable workspace.
inline float NativeUiDensity(int width, int height, float monitorScale) noexcept {
    constexpr float minScale = 0.75f, maxScale = 3.0f;
    constexpr float comfortableWidth = 960.0f, comfortableHeight = 540.0f;
    constexpr float minimumContentWidth = 640.0f, minimumContentHeight = 400.0f;
    if (!std::isfinite(monitorScale) || monitorScale <= 0.0f)
        monitorScale = 1.0f;
    if (width <= 0 || height <= 0)
        return std::clamp(monitorScale,minScale,maxScale);
    const float viewportScale = std::min(width / comfortableWidth,
                                         height / comfortableHeight);
    // DPI alone produces microscopic controls on a fullscreen 100%-DPI
    // display. Respect the OS preference AND a comfortable window-relative
    // scale, then cap it so the smallest supported workspace remains usable.
    const float desired = std::clamp(std::max(monitorScale,viewportScale),
                                     minScale,maxScale);
    const float usableScale = std::min(width / minimumContentWidth,
                                       height / minimumContentHeight);
    return std::max(0.05f,std::min(desired,usableScale));
}

} // namespace locus::app
