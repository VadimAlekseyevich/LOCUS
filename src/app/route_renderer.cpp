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

// Draw a full four-beat measure of future authored path in PLAY/TEST.
// Each source segment is clipped to [beat, beat+4] using fractional
// song beats. The rear edge follows the player; nothing lingers behind,
// and new parts emerge at zero opacity instead of popping into view.
void DrawScoredPath(const core::Chart& chart, double beat,
                    RouteCanvas canvas, const Skin& skin) {
    const int first = CurrentBeat(chart, beat);
    const float horizon = skin.lookaheadBeats * chart.ticksPerBeat;
    // A fractional musical-beat window may cross many finer ticks.
    const int last = std::min(
        chart.lengthBeats - 1,
        first + static_cast<int>(std::ceil(horizon)));
    for (int target = last; target >= first; --target) {
        const ScoredPathInterval visible = ScoredPathVisibleInterval(beat, target, horizon);
        if (!visible.Visible()) continue;
        const double begin = visible.begin, end = visible.end;
        const core::RouteStep step = core::SampleRouteStep(chart, target);
        const double fromX = static_cast<double>(step.from.x);
        const double fromY = static_cast<double>(step.from.y);
        const double dx = static_cast<double>(step.to.x - step.from.x);
        const double dy = static_cast<double>(step.to.y - step.from.y);

        // Keep roughly the same gradient sampling per MUSICAL beat.
        // Otherwise a 1/24 snap would multiply renderer work by 24.
        const int slices = std::max(1, kScoredPathSlices / chart.ticksPerBeat);
        for (int i = slices - 1; i >= 0; --i) {
            const double a = begin + (end - begin) * i / slices;
            const double b = begin + (end - begin) * (i + 1) / slices;
            const double ahead = (a + b) * 0.5 - beat;
            const float opacity = ScoredPathOpacity(ahead, horizon, skin.fadePower)
                * skin.pathOpacity;
            if (opacity <= 0.0005f) continue;
            const float width = ScoredPathWidth(ahead, horizon,
                                                skin.pathWidth, skin.fadePower);
            const auto screen = [&](double time) {
                const double t = time - target;
                return canvas.Position({fromX + dx * t, fromY + dy * t});
            };
            const Vector2 p0 = screen(a);
            const Vector2 p1 = screen(b);
            DrawLineEx(p0, p1, width * skin.outlineScale,
                       WithOpacity(skin.outline, opacity));
            DrawLineEx(p0, p1, width,
                       WithOpacity(skin.trajectory, opacity));
        }
    }
}

} // namespace

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
    const bool editor = true;
    const float scale = std::clamp(canvas.step * chart.ticksPerBeat / (editor ? 80.0f : 76.0f),
                                   0.4f, 1.0f);
    const float width = editor ? style.editorNextStepWidth : style.gameNextStepWidth;

    // One four-beat bar including the current step, farthest step first.
    for (int ahead = RouteVisualStyle::lookaheadBeats * chart.ticksPerBeat - 1;
         ahead >= 1; --ahead) {
        const int target = current + ahead;
        if (target >= chart.lengthBeats) continue;
        const float opacity = TrajectoryOpacity((target - beat) / chart.ticksPerBeat);
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
        for (auto turn = first; turn != chart.turns.end() &&
                          static_cast<double>(turn->beat) <
                              beat + skin.lookaheadBeats * chart.ticksPerBeat; ++turn) {
            const float opacity = ScoredPathOpacity(turn->beat - beat,
                                                    skin.lookaheadBeats * chart.ticksPerBeat,
                                                    skin.fadePower)
                * skin.pathOpacity;
            if (opacity <= 0.0005f) continue;
            const core::GridPoint position =
                core::SampleRouteStep(chart, turn->beat).from;
            const Vector2 point = canvas.Position(position);
            const float radius = std::min(skin.junctionRadius, canvas.step * chart.ticksPerBeat * 0.45f);
            if (skin.junctionGlow)
                DrawCircleV(point, radius * 1.45f,
                            WithOpacity(skin.junction, opacity * 0.26f));
            DrawCircleV(point, radius, WithOpacity(skin.junction, opacity));
            DrawCircleV(point, radius * 0.35f,
                        WithOpacity(skin.playerCore, opacity));
        }
        return;
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
