#include "app/route_renderer.h"

#include "app/visual_style.h"

#include <algorithm>
#include <cmath>

namespace locus::app {
namespace {

void DrawSegments(const core::StepView& view, RouteCanvas canvas,
                  Color color, float width) {
    for (int index = 0; index < view.segmentCount; ++index) {
        const auto& segment = view.segments[index];
        DrawLineEx(canvas.Position(segment.from), canvas.Position(segment.to),
                   width, color);
    }
}

int CurrentBeat(const core::Chart& chart, double beat) {
    return std::clamp(static_cast<int>(std::floor(beat)),
                      chart.firstBeat, chart.lengthBeats - 1);
}

} // namespace

Vector2 DrawRouteTrajectory(const core::Chart& chart, double fractionalBeat,
                            RouteCanvas canvas, RouteRenderMode mode) {
    if (chart.lengthBeats <= chart.firstBeat || !std::isfinite(fractionalBeat))
        return canvas.Point(chart.start);

    const auto& style = kDefaultRouteVisualStyle;
    const double beat = std::clamp(fractionalBeat,
                                  static_cast<double>(chart.firstBeat),
                                  static_cast<double>(chart.lengthBeats));
    const int current = CurrentBeat(chart, beat);
    const double progress = std::clamp(beat - current, 0.0, 1.0);
    const bool editor = mode == RouteRenderMode::Editor;
    const float scale = std::clamp(canvas.step / (editor ? 80.0f : 76.0f),
                                   0.4f, 1.0f);
    const float width = editor ? style.editorNextStepWidth : style.gameNextStepWidth;

    // One four-beat bar including the current step, farthest step first.
    for (int ahead = RouteVisualStyle::lookaheadBeats - 1; ahead >= 1; --ahead) {
        const int target = current + ahead;
        if (target >= chart.lengthBeats) continue;
        const float opacity = TrajectoryOpacity(target - beat);
        if (opacity <= 0.0f) continue;
        const core::StepView future = core::ViewRouteStep(
            core::SampleRouteStep(chart, target), 1.0);
        DrawSegments(future, canvas, WithOpacity(style.routeOutline, opacity),
                     width * style.outlineScale * scale);
        DrawSegments(future, canvas, WithOpacity(style.fullTrajectory, opacity),
                     width * scale);
    }

    const core::StepView whole = core::ViewRouteStep(
        core::SampleRouteStep(chart, current), progress);
    const core::StepView trace = core::TraceRouteStep(
        core::SampleRouteStep(chart, current), progress);

    if (editor) {
        // The editor intentionally also shows the full current step for
        // authoring; both screens share exactly the same future segments.
        DrawSegments(whole, canvas, style.fullTrajectory, 4.0f * scale);
        DrawSegments(trace, canvas, style.combinedTrajectory, 5.5f * scale);
    } else {
        DrawSegments(trace, canvas, style.routeOutline, 7.0f * scale);
        DrawSegments(trace, canvas, style.combinedTrajectory, 5.5f * scale);
    }
    return canvas.Position(whole.marker);
}

void DrawRouteJunctions(const core::Chart& chart, double fractionalBeat,
                        RouteCanvas canvas, RouteRenderMode mode) {
    if (chart.lengthBeats <= chart.firstBeat || !std::isfinite(fractionalBeat))
        return;
    const double beat = std::clamp(fractionalBeat,
                                   static_cast<double>(chart.firstBeat),
                                   static_cast<double>(chart.lengthBeats));
    const int current = CurrentBeat(chart, beat);
    const bool editor = mode == RouteRenderMode::Editor;
    const auto& style = kDefaultRouteVisualStyle;
    for (const auto& turn : core::PreviewTurnPoints(
             chart, beat, RouteVisualStyle::lookaheadBeats)) {
        const float opacity = turn.beat <= current
            ? 1.0f : TrajectoryOpacity(turn.beat - 1 - beat);
        if (opacity <= 0.0f) continue;
        const Vector2 point = canvas.Point(turn.cell);
        DrawCircleV(point, std::min(11.0f, canvas.step * (editor ? 0.39f : 0.38f)),
                    WithOpacity(style.reachedTurnPoint, opacity));
        DrawCircleV(point, std::min(4.0f, canvas.step * 0.16f),
                    WithOpacity(editor ? Color{255, 225, 228, 255}
                                       : Color{255, 229, 230, 255}, opacity));
    }
}

void DrawPlayerHitFlash(Vector2 marker, float markerRadius, HitFlashFrame flash) {
    if (flash.intensity <= 0.0f) return;
    const float radius = markerRadius + 4.0f + 17.0f * flash.progress;
    // Tiny halo, expanding ring and bright core. All decay within 180 ms.
    DrawCircleV(marker, markerRadius * (1.48f + 0.27f * flash.progress),
                WithOpacity(Color{153, 255, 235, 90}, flash.intensity));
    DrawRing(marker, radius - 1.5f, radius + 1.5f, 0.0f, 360.0f, 40,
             WithOpacity(Color{255, 233, 174, 235}, flash.intensity));
    DrawCircleV(marker, markerRadius * 0.68f,
                WithOpacity(Color{250, 255, 255, 125}, flash.intensity));
}

} // namespace locus::app
