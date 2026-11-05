#pragma once

#include "raylib.h"

#include <algorithm>
#include <cmath>

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

// Editor-only preview: a four-beat bar, still used while authoring ROUTE.
// PLAY/TEST uses ScoredPathOpacity below, not this whole-beat preview.
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

// PLAY/TEST displays exactly ONE continuous beat of future trajectory.
// Anything behind the moving marker disappears immediately. The red route
// starts opaque and wide directly ahead; both opacity and width taper to
// zero at the next beat. Evaluate fractional song beats every frame.
inline constexpr double kScoredPathLengthBeats = 1.0;
inline constexpr int kScoredPathSlices = 32;

inline float ScoredPathOpacity(double ahead) noexcept {
    if (!std::isfinite(ahead) || ahead < 0.0 ||
        ahead >= kScoredPathLengthBeats) return 0.0f;
    const float t = std::clamp(static_cast<float>(ahead), 0.0f, 1.0f);
    const float smooth = t * t * (3.0f - 2.0f * t);
    return 1.0f - smooth;
}

// Clip an authored beat segment against the one-beat window ahead of
// the player. Never draw any time earlier than 'beat'.
struct ScoredPathInterval {
    double begin = 0.0;
    double end = 0.0;
    bool Visible() const noexcept { return end > begin; }
};

inline ScoredPathInterval ScoredPathVisibleInterval(double beat,
                                                   int segmentBeat) noexcept {
    if (!std::isfinite(beat)) return {};
    const double begin = std::max(beat, static_cast<double>(segmentBeat));
    const double end = std::min(beat + kScoredPathLengthBeats,
                                 static_cast<double>(segmentBeat + 1));
    return {begin, std::max(begin, end)};
}

inline float ScoredPathWidth(double ahead) noexcept {
    const float opacity = ScoredPathOpacity(ahead);
    return opacity <= 0.0f ? 0.0f : 0.7f + 5.3f * opacity;
}

inline Color WithOpacity(Color color, float opacity) noexcept {
    if (opacity <= 0.0f) color.a = 0;
    else if (opacity < 1.0f) color.a = static_cast<unsigned char>(
        static_cast<float>(color.a) * opacity);
    return color;
}
inline constexpr EditorRouteLayout kDefaultEditorRouteLayout{};

} // namespace locus::app
