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

// Raylib's 2D projected Y axis points down. For its quad indices
// (0,1,2 and 0,2,3), vertices must trace the same front-facing outline
// as DrawRectangle/DrawLineEx. Reverse winding makes the GPU cull the
// ENTIRE path even though all mesh positions are mathematically valid.
struct PathRibbonQuad {
    Vector2 vertices[4]{};
};
PathRibbonQuad MakePathRibbonQuad(
    const PathRibbonRow& from, const PathRibbonRow& to, float scale) noexcept;

// Build one continuous tapered ribbon over [playhead, playhead + lookahead].
// Direction changes of up to 135 degrees use mitered joins. Exact 180-degree
// reversals split into two runs at the same point, avoiding an inverted join.
// All route/clock calculations stay outside the renderer and are unmodified.
std::vector<PathRibbonRow> BuildScoredPathRibbon(
    const core::Chart& chart, double fractionalTick,
    RouteCanvas canvas, const Skin& skin);

} // namespace locus::app
