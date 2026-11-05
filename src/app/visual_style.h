#pragma once

#include "raylib.h"

namespace locus::app {

// Rendering-only values: chart geometry, audio timing and scoring never read
// these. A future player skin can load a palette and layout into the same
// structs without changing the route sampler or chart JSON format.
// No user-editable skins, textures or drag-and-drop layout exist yet.
struct RouteVisualStyle {
    // These form the future skin's fully-colored / semi-colored / overlaid path.
    Color fullTrajectory{255, 70, 84, 255};
    Color semiTrajectory{255, 70, 84, 130};
    Color combinedTrajectory{255, 117, 125, 255};
    Color routeOutline{49, 13, 24, 245};
    Color reachedTurnPoint{255, 63, 72, 255};
    static constexpr int lookaheadBeats = 4; // One bar of 4/4.
    static constexpr int fadeBeats = 4; // Linearly fade one bar into view.
    float editorNextStepWidth = 6.0f;
    float gameNextStepWidth = 5.0f;
    float outlineScale = 1.8f;
};

struct EditorRouteLayout {
    float practiceSpeedX = 618.0f;
    float practiceSpeedStartY = 208.0f;
    float practiceSpeedWidth = 116.0f;
    float practiceSpeedHeight = 30.0f;
    float practiceSpeedGap = 7.0f;
};

inline constexpr RouteVisualStyle kDefaultRouteVisualStyle{};

// One bar in total. Opacity rises linearly from zero at four beats ahead
// to fully red at the current beat. Measured in beats, not frames.
inline float TrajectoryOpacity(double beatsAhead) noexcept {
    if (beatsAhead <= RouteVisualStyle::lookaheadBeats -
                          RouteVisualStyle::fadeBeats) return 1.0f;
    if (beatsAhead >= RouteVisualStyle::lookaheadBeats) return 0.0f;
    return static_cast<float>(
        (RouteVisualStyle::lookaheadBeats - beatsAhead) /
        RouteVisualStyle::fadeBeats);
}

inline Color WithOpacity(Color color, float opacity) noexcept {
    if (opacity <= 0.0f) color.a = 0;
    else if (opacity < 1.0f) color.a = static_cast<unsigned char>(
        static_cast<float>(color.a) * opacity);
    return color;
}
inline constexpr EditorRouteLayout kDefaultEditorRouteLayout{};

} // namespace locus::app
