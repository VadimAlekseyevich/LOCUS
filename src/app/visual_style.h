#pragma once

#include "raylib.h"

namespace locus::app {

// Rendering-only values: chart geometry, audio timing and scoring never read
// these. A future player skin can load a palette and layout into the same
// structs without changing the route sampler or chart JSON format.
// No user-editable skins, textures or drag-and-drop layout exist yet.
struct RouteVisualStyle {
    Color nextStep{179, 224, 255, 255};  // High-contrast future trajectory
    Color furtherStep{152, 207, 252, 235};
    Color routeOutline{13, 29, 49, 245};
    Color wholeStep{112, 199, 239, 222};
    Color traveledStep{112, 240, 220, 255};
    Color currentStep{113, 237, 221, 255};
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
inline constexpr EditorRouteLayout kDefaultEditorRouteLayout{};

} // namespace locus::app
