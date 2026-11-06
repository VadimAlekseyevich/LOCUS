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

// PLAY/TEST previews an entire four-beat measure, not a single beat.
// Anything behind the moving marker disappears immediately. Ahead, the
// trajectory stays readable for upcoming turns while thinning and fading
// continuously to zero at the end of the fourth beat.
inline constexpr double kScoredPathLengthBeats = 4.0;
inline constexpr int kScoredPathSlices = 32;

inline float ScoredPathOpacity(double ahead,
                               double length = kScoredPathLengthBeats,
                               float fadePower = 1.0f) noexcept {
    if (!std::isfinite(ahead) || !std::isfinite(length) ||
        !std::isfinite(fadePower) || length <= 0.0 ||
        ahead < 0.0 || ahead >= length) return 0.0f;
    const float t = std::clamp(static_cast<float>(ahead / length), 0.0f, 1.0f);
    // Same smooth four-beat default as before; user skins can change
    // the distance and curve without altering chart beats or timing.
    const float u = t * t;
    const float smooth = u * u * (3.0f - 2.0f * u);
    return std::pow(1.0f - smooth, fadePower);
}

// Clip each authored beat segment against the four-beat lookahead
// measured from the player's current fractional beat. Nothing behind
// the player is drawn, and no integer-beat boundaries cause popping.
struct ScoredPathInterval {
    double begin = 0.0;
    double end = 0.0;
    bool Visible() const noexcept { return end > begin; }
};

inline ScoredPathInterval ScoredPathVisibleInterval(
    double beat, int segmentBeat,
    double length = kScoredPathLengthBeats) noexcept {
    if (!std::isfinite(beat) || !std::isfinite(length) || length <= 0.0) return {};
    const double begin = std::max(beat, static_cast<double>(segmentBeat));
    const double end = std::min(beat + length, static_cast<double>(segmentBeat + 1));
    return {begin, std::max(begin, end)};
}

inline float ScoredPathWidth(double ahead,
                             double length = kScoredPathLengthBeats,
                             float thickness = 6.0f,
                             float fadePower = 1.0f) noexcept {
    const float opacity = ScoredPathOpacity(ahead, length, fadePower);
    return opacity <= 0.0f ? 0.0f
        : thickness * (0.7f / 6.0f + (5.3f / 6.0f) * opacity);
}

inline Color WithOpacity(Color color, float opacity) noexcept {
    if (opacity <= 0.0f) color.a = 0;
    else if (opacity < 1.0f) color.a = static_cast<unsigned char>(
        static_cast<float>(color.a) * opacity);
    return color;
}
inline constexpr EditorRouteLayout kDefaultEditorRouteLayout{};

} // namespace locus::app
