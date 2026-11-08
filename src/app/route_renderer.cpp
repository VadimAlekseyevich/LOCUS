#include "app/route_renderer.h"

#include "app/visual_style.h"
#include "app/path_mesh.h"

#include "rlgl.h"
#include "app/reverse_cap.h"

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

// Attach a filled semicircular cap to the ordinary junction dot. The cap
// curves toward the ARRIVING side; its diameter is perpendicular to the
// authored line. Neither the route nor its player marker is displaced.
void DrawReverseCapInternal(Vector2 point, float radius, core::Heading outgoing,
                            Color color, float opacity, bool glow, float capScale) {
    if (radius <= 0.0f || opacity <= 0.0f || capScale <= 0.0f) return;
    const ReverseCapArc arc = ReverseCapForOutgoing(outgoing);
    const float capRadius = radius * capScale;
    if (glow)
        DrawCircleSector(point, capRadius * 1.15f, arc.startDegrees,
                         arc.endDegrees, 24, WithOpacity(color, opacity * 0.22f));
    DrawCircleSector(point, capRadius, arc.startDegrees,
                     arc.endDegrees, 24, WithOpacity(color, opacity));
}

int CurrentBeat(const core::Chart& chart, double beat) {
    return std::clamp(static_cast<int>(std::floor(beat)),
                      chart.firstBeat, chart.lengthBeats - 1);
}

// Draw a continuous joined ribbon, not a collection of independent thick
// line quads. Each adjoining pair of triangles uses the exact SAME row
// vertices and interpolated alpha, eliminating raster seams on moving turns.
void RibbonVertex(Vector2 point, Color tint, float opacity) {
    const Color color = WithOpacity(tint, opacity);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex2f(point.x, point.y);
}

Vector2 Offset(Vector2 center, Vector2 normal, float scale) {
    return {center.x + normal.x * scale, center.y + normal.y * scale};
}

void DrawRibbonLayer(const std::vector<PathRibbonRow>& rows,
                     Color tint, float scale) {
    rlBegin(RL_TRIANGLES);
    // Draw far-to-near, as the old renderer did. When a route retraces
    // itself, the part closer to the player should remain visually on top.
    for (std::size_t i = rows.size() - 1; i > 0; --i) {
        const auto& from = rows[i - 1];
        const auto& to = rows[i];
        if (to.startsRun || (from.opacity <= 0.0f && to.opacity <= 0.0f))
            continue;
        const Vector2 a = Offset(from.center, from.offset, scale);
        const Vector2 b = Offset(from.center, from.offset, -scale);
        const Vector2 c = Offset(to.center, to.offset, scale);
        const Vector2 d = Offset(to.center, to.offset, -scale);

        RibbonVertex(a, tint, from.opacity);
        RibbonVertex(b, tint, from.opacity);
        RibbonVertex(c, tint, to.opacity);

        RibbonVertex(c, tint, to.opacity);
        RibbonVertex(b, tint, from.opacity);
        RibbonVertex(d, tint, to.opacity);
    }
    rlEnd();
}

void DrawScoredPath(const core::Chart& chart, double beat,
                    RouteCanvas canvas, const Skin& skin) {
    const auto rows = BuildScoredPathRibbon(chart, beat, canvas, skin);
    if (rows.size() < 2) return;
    // As before, outline is behind trajectory, but both layers now have
    // watertight corners and vertex alpha rather than per-slice alpha jumps.
    DrawRibbonLayer(rows, skin.outline, skin.outlineScale);
    DrawRibbonLayer(rows, skin.trajectory, 1.0f);
}

} // namespace

void DrawReverseCap(Vector2 point, float junctionRadius, core::Heading outgoing,
                    Color color, float opacity, bool glow, float capScale) {
    DrawReverseCapInternal(point, junctionRadius, outgoing,
                           color, opacity, glow, capScale);
}

Vector2 DrawRouteTrajectory(const core::Chart& chart, double fractionalBeat,
                            RouteCanvas canvas, RouteRenderMode mode, const Skin& skin) {
    if (chart.lengthBeats <= chart.firstBeat || !std::isfinite(fractionalBeat))
        return canvas.Point(chart.start);

    const auto& style = kDefaultRouteVisualStyle;
    const double beat = std::clamp(fractionalBeat,
                                  static_cast<double>(chart.firstBeat),
                                  static_cast<double>(chart.lengthBeats));
    const int current = CurrentBeat(chart, beat);
    if (mode == RouteRenderMode::Gameplay) {
        DrawScoredPath(chart, beat, canvas, skin);
        return canvas.Position(core::RoutePositionAt(chart, beat));
    }
    const double progress = std::clamp(beat - current, 0.0, 1.0);
    const float scale = std::clamp(
        canvas.step * chart.ticksPerBeat / 80.0f, 0.4f, 1.0f);
    const float width = style.editorNextStepWidth;

    // Sample the future bar once, without replaying all earlier turns
    // for every line segment. The editor uses this exact route geometry.
    const int lookahead = RouteVisualStyle::lookaheadBeats * chart.ticksPerBeat;
    const int last = current + std::min(
        chart.lengthBeats - 1 - current, lookahead - 1);
    const auto steps = core::SampleRouteWindow(chart, current, last);
    for (int target = last; target > current; --target) {
        const float opacity = TrajectoryOpacity((target - beat) / chart.ticksPerBeat);
        if (opacity <= 0.0f) continue;
        const core::StepView future = core::ViewRouteStep(
            steps[static_cast<std::size_t>(target - current)], 1.0);
        DrawSegments(future, canvas, WithOpacity(style.routeOutline, opacity),
                     width * style.outlineScale * scale);
        DrawSegments(future, canvas, WithOpacity(style.fullTrajectory, opacity),
                     width * scale);
    }

    const core::RouteStep& currentStep = steps.front();
    const core::StepView whole = core::ViewRouteStep(currentStep, progress);
    const core::StepView trace = core::TraceRouteStep(currentStep, progress);

    // The editor intentionally shows both the complete current step and
    // the traced portion, to allow precise note placement.
    DrawSegments(whole, canvas, style.fullTrajectory, 4.0f * scale);
    DrawSegments(trace, canvas, style.combinedTrajectory, 5.5f * scale);
    return canvas.Position(whole.marker);
}

void DrawRouteJunctions(const core::Chart& chart, double fractionalBeat,
                        RouteCanvas canvas, RouteRenderMode mode, const Skin& skin) {
    if (chart.lengthBeats <= chart.firstBeat || !std::isfinite(fractionalBeat))
        return;
    const double beat = std::clamp(fractionalBeat,
                                   static_cast<double>(chart.firstBeat),
                                   static_cast<double>(chart.lengthBeats));
    const int current = CurrentBeat(chart, beat);
    const bool editor = mode == RouteRenderMode::Editor;
    const auto& style = kDefaultRouteVisualStyle;
    if (!editor) {
        // Same full-measure preview envelope as the path. Red turn targets
        // appear early enough to read and vanish once the player reaches
        // them; no completed targets linger behind the avatar.
        const int next = static_cast<int>(std::ceil(beat));
        const auto first = std::lower_bound(
            chart.turns.begin(), chart.turns.end(), next,
            [](const core::TurnEvent& e, int index) {
                return e.beat < index;
            });
        const auto last = std::lower_bound(
            first, chart.turns.end(), beat + skin.lookaheadBeats * chart.ticksPerBeat,
            [](const core::TurnEvent& event, double limit) {
                return event.beat < limit;
            });
        if (first == last) return;
        // One window even if every visible tick contains a turn.
        const int base = first->beat;
        const int end = (last - 1)->beat;
        const auto steps = core::SampleRouteWindow(chart, base, end);
        for (auto turn = first; turn != last; ++turn) {
            const float opacity = ScoredPathOpacity(turn->beat - beat,
                                                    skin.lookaheadBeats * chart.ticksPerBeat,
                                                    skin.fadePower)
                * skin.pathOpacity;
            if (opacity <= 0.0005f) continue;
            const core::GridPoint position =
                steps[static_cast<std::size_t>(turn->beat - base)].from;
            const Vector2 point = canvas.Position(position);
            const float radius = std::min(skin.junctionRadius, canvas.step * chart.ticksPerBeat * 0.45f);
            if (skin.junctionGlow)
                DrawCircleV(point, radius * 1.45f,
                            WithOpacity(skin.junction, opacity * 0.26f));
            if (NeedsReverseCap(turn->direction)) {
                const core::Heading outgoing =
                    steps[static_cast<std::size_t>(turn->beat - base)].heading;
                DrawReverseCap(point, radius, outgoing,
                               skin.junction, opacity, skin.junctionGlow,
                               skin.reverseCapScale);
            }
            DrawCircleV(point, radius, WithOpacity(skin.junction, opacity));
            DrawCircleV(point, radius * 0.35f,
                        WithOpacity(skin.playerCore, opacity));
        }
        return;
    }
    // The editor deduplicates coincident junction DOTS. Render the
    // reverse caps from authored events separately so an older turn at the
    // same world coordinate cannot hide a later 180-degree reversal.
    const int lastVisible = current + std::min(
        chart.lengthBeats - 1 - current,
        RouteVisualStyle::lookaheadBeats * chart.ticksPerBeat);
    const auto begin = std::lower_bound(
        chart.turns.begin(), chart.turns.end(), current,
        [](const core::TurnEvent& event, int tick) { return event.beat < tick; });
    const auto end = std::upper_bound(
        begin, chart.turns.end(), lastVisible,
        [](int tick, const core::TurnEvent& event) { return tick < event.beat; });
    if (std::any_of(begin, end, [](const core::TurnEvent& event) {
            return NeedsReverseCap(event.direction);
        })) {
        // Batch-sample once even when the editor shows many dense reversals.
        const int firstTurnBeat = begin->beat;
        const auto steps = core::SampleRouteWindow(chart, firstTurnBeat,
                                                   (end - 1)->beat);
        for (auto turn = begin; turn != end; ++turn) {
            if (!NeedsReverseCap(turn->direction)) continue;
            const float opacity = turn->beat <= current
                ? 1.0f : TrajectoryOpacity(
                    (turn->beat - chart.ticksPerBeat - beat) /
                    chart.ticksPerBeat);
            if (opacity <= 0.0f) continue;
            const auto& step = steps[static_cast<std::size_t>(
                turn->beat - firstTurnBeat)];
            const Vector2 point = canvas.Position(step.from);
            const float radius = std::min(
                11.0f, canvas.step * chart.ticksPerBeat * 0.39f);
            DrawReverseCap(point, radius, step.heading,
                           style.reachedTurnPoint, opacity, false,
                           skin.reverseCapScale);
        }
    }
    for (const auto& turn : core::PreviewTurnPoints(
             chart, beat, RouteVisualStyle::lookaheadBeats * chart.ticksPerBeat)) {
        const float opacity = turn.beat <= current
            ? 1.0f : TrajectoryOpacity((turn.beat - chart.ticksPerBeat - beat) /
                                        chart.ticksPerBeat);
        if (opacity <= 0.0f) continue;
        const Vector2 point = canvas.Position(turn.position);
        DrawCircleV(point, std::min(11.0f, canvas.step * chart.ticksPerBeat * (editor ? 0.39f : 0.38f)),
                    WithOpacity(style.reachedTurnPoint, opacity));
        DrawCircleV(point, std::min(4.0f, canvas.step * chart.ticksPerBeat * 0.16f),
                    WithOpacity(editor ? Color{255, 225, 228, 255}
                                       : Color{255, 229, 230, 255}, opacity));
    }
}

void DrawPlayerHitFlash(Vector2 marker, float markerRadius, HitFlashFrame flash,
                        const Skin& skin) {
    if (flash.intensity <= 0.0f) return;
    const float radius = markerRadius + (4.0f + 17.0f * flash.progress)
        * skin.hitFlashScale;
    // Tiny halo, expanding ring and bright core. All decay within 180 ms.
    DrawCircleV(marker, markerRadius * (1.48f + 0.27f * flash.progress),
                WithOpacity(skin.hitFlash, flash.intensity * 0.36f));
    DrawRing(marker, radius - 1.5f, radius + 1.5f, 0.0f, 360.0f, 40,
             WithOpacity(skin.hitFlash, flash.intensity));
    DrawCircleV(marker, markerRadius * 0.68f,
                WithOpacity(skin.playerCore, flash.intensity * 0.5f));
}

} // namespace locus::app
