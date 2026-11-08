#pragma once

#include "app/route_renderer.h"

#include <vector>

namespace locus::app {

// A width/opacity sample along the future route, already projected to
// screen coordinates. Adjacent segments share this exact row at their join;
// independent DrawLineEx quads cannot provide that continuity.
struct PathRibbonRow {
    Vector2 center{};
    Vector2 offset{};          // normal * half-width (before outline scale)
    float opacity = 0.0f;
    double tick = 0.0;
    bool startsRun = false;    // true at start and at 180-degree reversals
};

// Build one continuous tapered ribbon over [playhead, playhead + lookahead].
// Direction changes of up to 135 degrees use mitered joins. Exact 180-degree
// reversals split into two runs at the same point, avoiding an inverted join.
// All route/clock calculations stay outside the renderer and are unmodified.
std::vector<PathRibbonRow> BuildScoredPathRibbon(
    const core::Chart& chart, double fractionalTick,
    RouteCanvas canvas, const Skin& skin);

} // namespace locus::app
