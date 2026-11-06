#pragma once

#include "app/hit_flash.h"
#include "app/skin.h"
#include "core/route.h"
#include "core/route_view.h"
#include "raylib.h"

namespace locus::app {

// Screen-space projection used by both the editor and scored gameplay.
// Keep the chart and route sampler independent of raylib coordinates.
struct RouteCanvas {
    float left = 0.0f;
    float top = 0.0f;
    float step = 1.0f;
    core::GridPoint origin{}; // World position that maps to (left, top).

    Vector2 Position(core::GridPoint point) const noexcept {
        return {left + static_cast<float>((point.x - origin.x) * step),
                top + static_cast<float>((point.y - origin.y) * step)};
    }
    Vector2 Point(core::Cell cell) const noexcept {
        return Position({static_cast<double>(cell.x),
                         static_cast<double>(cell.y)});
    }
};

enum class RouteRenderMode { Editor, Gameplay };

// The same geometry, one-bar fade and style rules are used in both screens.
// Draw lines before grid nodes; draw junctions afterwards so they remain visible.
// Returns the interpolated live marker position in screen coordinates.
Vector2 DrawRouteTrajectory(const core::Chart& chart, double fractionalBeat,
                            RouteCanvas canvas, RouteRenderMode mode,
                            const Skin& skin = Skin{});
void DrawRouteJunctions(const core::Chart& chart, double fractionalBeat,
                        RouteCanvas canvas, RouteRenderMode mode,
                        const Skin& skin = Skin{});
// A short radial feedback burst attached to the moving marker, never
// the stationary junction dot. Only called for scored successful hits.
void DrawPlayerHitFlash(Vector2 marker, float markerRadius, HitFlashFrame flash,
                        const Skin& skin = Skin{});

} // namespace locus::app
