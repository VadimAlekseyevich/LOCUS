#include "app/path_mesh.h"

#include "app/visual_style.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>

namespace locus::app {
namespace {

using PathSample=PathRibbonWorkspace::Sample;

struct Direction {
    double x = 1.0;
    double y = 0.0;
};

Direction UnitDirection(core::GridPoint a, core::GridPoint b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    return len > 1e-12 ? Direction{dx / len, dy / len} : Direction{};
}

Direction Normal(Direction direction) {
    return {-direction.y, direction.x};
}

double Dot(Direction a, Direction b) {
    return a.x * b.x + a.y * b.y;
}

PathRibbonRow MakeRow(const PathSample& sample, RouteCanvas canvas,
                      Direction normal, double miter, bool startsRun) {
    return {
        canvas.Position(sample.position),
        {static_cast<float>(normal.x * sample.halfWidth * miter),
         static_cast<float>(normal.y * sample.halfWidth * miter)},
        sample.opacity, sample.tick, startsRun, sample.halfWidth
    };
}

void MakeRows(const std::vector<PathSample>& samples,
              RouteCanvas canvas,std::vector<PathRibbonRow>& rows) {
    rows.clear();
    if (samples.size() < 2) return;
    // At most two rows per sample (reverse/bevel), no growth mid-build.
    rows.reserve(samples.size()*2);
    constexpr double kReverseDot = -0.9999;
    constexpr double kMiterLimit = 2.75; // Covers a 135-degree turn (2.613).
    Direction next=UnitDirection(samples[0].position,samples[1].position);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const Direction previous=next;
        if (i>0 && i+1<samples.size())
            next=UnitDirection(samples[i].position,samples[i+1].position);
        const Direction prevNormal = Normal(previous);
        const Direction nextNormal = Normal(next);

        if (i == 0 || i + 1 == samples.size()) {
            rows.push_back(MakeRow(samples[i], canvas,
                                   i == 0 ? nextNormal : prevNormal,
                                   1.0, i == 0));
            continue;
        }

        const double alignment = Dot(previous, next);
        if (alignment < kReverseDot) {
            // A 180-degree U-turn cannot form a finite miter. End the
            // preceding strip and start another at the identical junction,
            // retaining the user-authored overlapping trajectory.
            rows.push_back(MakeRow(samples[i], canvas, prevNormal, 1.0, false));
            rows.push_back(MakeRow(samples[i], canvas, nextNormal, 1.0, true));
            continue;
        }

        // Both sides of each joined corner use the same transformed normal.
        // This eliminates the independent-quad cracks of DrawLineEx.
        const Direction sum{prevNormal.x + nextNormal.x,
                            prevNormal.y + nextNormal.y};
        const double length = std::hypot(sum.x, sum.y);
        const Direction bisector{sum.x / length, sum.y / length};
        const double cosine = Dot(bisector, nextNormal);
        const double miter = 1.0 / cosine;
        if (!std::isfinite(miter) || miter > kMiterLimit) {
            // Unexpected extremely acute geometry: bevel instead of a
            // potentially unbounded spike; the intermediate rows still
            // meet at the exact same path junction.
            rows.push_back(MakeRow(samples[i], canvas, prevNormal, 1.0, false));
            rows.push_back(MakeRow(samples[i], canvas, nextNormal, 1.0, false));
        } else {
            rows.push_back(MakeRow(samples[i], canvas, bisector, miter, false));
        }
    }
}

} // namespace

std::vector<PathRibbonRow> BuildScoredPathRibbon(
    const core::Chart& chart, double fractionalTick,
    RouteCanvas canvas, const Skin& skin) {
    std::vector<PathRibbonRow> rows;
    if (chart.lengthBeats <= chart.firstBeat ||
        !std::isfinite(fractionalTick) ||
        !std::isfinite(canvas.step) || canvas.step <= 0.0f ||
        !std::isfinite(skin.lookaheadMs) || skin.lookaheadMs <= 0.0f)
        return rows;

    const double tick = std::clamp(
        fractionalTick, static_cast<double>(chart.firstBeat),
        static_cast<double>(chart.lengthBeats));
    const double horizon = ScoredPreviewTicks(
        skin.lookaheadMs, chart.bpm, chart.ticksPerBeat);
    const double lastTime = std::min(
        static_cast<double>(chart.lengthBeats), tick + horizon);
    if (!std::isfinite(horizon) || horizon <= 0.0 || lastTime <= tick)
        return rows;

    const int first = static_cast<int>(std::floor(tick));
    const int last = std::min(chart.lengthBeats - 1,
                              static_cast<int>(std::ceil(lastTime)) - 1);
    if (first > last) return rows;

    const auto steps = core::SampleRouteWindow(chart, first, last);
    const int slices = std::max(1, kScoredPathSlices / chart.ticksPerBeat);
    std::vector<PathSample> samples;
    samples.reserve(static_cast<std::size_t>(last - first + 1) *
                    static_cast<std::size_t>(slices) + 1);
    for (int target = first; target <= last; ++target) {
        const double begin = std::max(tick, static_cast<double>(target));
        const double end = std::min(lastTime, static_cast<double>(target + 1));
        if (end <= begin) continue;
        const auto& step = steps[static_cast<std::size_t>(target - first)];
        for (int part = 0; part <= slices; ++part) {
            const double time = begin + (end - begin) * part / slices;
            // Consecutive ticks must use ONE shared vertex. Ignore tiny
            // duplicate times near floating-point boundaries as well.
            if (!samples.empty() && time - samples.back().tick <= 1e-10)
                continue;
            const double progress = time - target;
            const core::GridPoint point{
                step.from.x + (step.to.x - step.from.x) * progress,
                step.from.y + (step.to.y - step.from.y) * progress};
            const double ahead = time - tick;
            const float proximity=ScoredPathOpacity(ahead,horizon,skin.fadePower);
            const float opacity=proximity*skin.pathOpacity;
            const float halfWidth=ScoredPathWidthFromOpacity(proximity,skin.pathWidth)*0.5f;
            samples.push_back({point, time, halfWidth, opacity});
        }
    }
    MakeRows(samples,canvas,rows);
    return rows;
}

// Gameplay-only sampling: authored exact times are the segment boundaries.
// The renderer never rounds a song timestamp back onto the editing grid.
void PrepareScoredPathRibbon(const RoutePreviewFrame& preview,
        RouteCanvas canvas,const Skin& skin,PathRibbonWorkspace& workspace) {
    auto& samples=workspace.samples;
    samples.clear();
    workspace.rows.clear();
    if (!preview.ready || !std::isfinite(canvas.step) ||
        canvas.step <= 0.0f || !std::isfinite(preview.horizonSeconds) ||
        preview.horizonSeconds <= 0.0) return;
    const double horizon=preview.horizonSeconds;
    const auto& spans=preview.spans;
    if (spans.empty()) return;
    samples.reserve(spans.size()*2+33);
    for (const auto& span : spans) {
        const double duration = span.toSeconds - span.fromSeconds;
        const int slices = std::max(1, static_cast<int>(
            std::ceil(duration * kScoredPathSlices / horizon)));
        for (int part = 0; part <= slices; ++part) {
            const double fraction = static_cast<double>(part) / slices;
            const double at = span.fromSeconds + duration * fraction;
            if (!samples.empty() &&
                at - samples.back().tick <= 1e-10) continue;
            const core::GridPoint position{
                span.from.x + (span.to.x - span.from.x) * fraction,
                span.from.y + (span.to.y - span.from.y) * fraction};
            const double ahead = at - preview.songSeconds;
            const float proximity=ScoredPathOpacity(ahead,horizon,skin.fadePower);
            const float opacity=proximity*skin.pathOpacity*ModVisibility(preview,ahead);
            const float halfWidth=ScoredPathWidthFromOpacity(proximity,skin.pathWidth)*0.5f;
            samples.push_back({position, at, halfWidth, opacity});
        }
    }
    MakeRows(samples,canvas,workspace.rows);
}

std::vector<PathRibbonRow> BuildScoredPathRibbon(
    const RoutePreviewFrame& preview,RouteCanvas canvas,const Skin& skin) {
    PathRibbonWorkspace workspace;
    PrepareScoredPathRibbon(preview,canvas,skin,workspace);
    return std::move(workspace.rows);
}

std::vector<PathRibbonRow> BuildScoredPathRibbon(
    const core::TimedRoute& route, double songSeconds,
    RouteCanvas canvas, const Skin& skin) {
    return BuildScoredPathRibbon(
        MakeRoutePreview(route, songSeconds, skin.lookaheadMs), canvas, skin);
}

PathRibbonCoverageRow MakePathRibbonCoverageRow(
    const PathRibbonRow& row, float layerScale, float pixelsPerDrawUnit) noexcept {
    PathRibbonCoverageRow result{};
    for (auto& point : result.points) point = row.center;
    if (row.halfWidth <= 0.0f || row.opacity <= 0.0f ||
        !std::isfinite(layerScale) || layerScale <= 0.0f ||
        !std::isfinite(pixelsPerDrawUnit) || pixelsPerDrawUnit <= 0.0f)
        return result;

    // The one-framebuffer-pixel transition straddles the nominal edge.
    // Offset along the original miter, not its normalized direction:
    // both adjacent segments then share exactly the same feather vertices.
    const float halfPixel = 0.5f / pixelsPerDrawUnit;
    const float halfWidth = row.halfWidth * layerScale;
    const float inner = std::max(0.0f, halfWidth - halfPixel);
    const float outer = halfWidth + halfPixel;
    const float factors[4] = {-outer, -inner, inner, outer};
    for (int i = 0; i < 4; ++i) {
        const float factor = factors[i] / row.halfWidth;
        result.points[i] = {row.center.x + row.offset.x * factor,
                            row.center.y + row.offset.y * factor};
    }
    // Below one physical pixel the opaque interior collapses to the
    // centerline. Preserve the integrated coverage of the narrow stroke
    // instead of making it look artificially wider/brighter.
    const float coverage = std::min(1.0f, 2.0f * halfWidth / outer);
    result.opacity[1] = result.opacity[2] = row.opacity * coverage;
    return result;
}

PathRibbonQuad MakePathRibbonQuad(
    const PathRibbonRow& from, const PathRibbonRow& to, float scale) noexcept {
    const Vector2 fromLeft{
        from.center.x - from.offset.x * scale,
        from.center.y - from.offset.y * scale};
    const Vector2 fromRight{
        from.center.x + from.offset.x * scale,
        from.center.y + from.offset.y * scale};
    const Vector2 toRight{
        to.center.x + to.offset.x * scale,
        to.center.y + to.offset.y * scale};
    const Vector2 toLeft{
        to.center.x - to.offset.x * scale,
        to.center.y - to.offset.y * scale};
    // In raylib's screen coordinates this is clockwise. In GL's
    // Y-up projection it is front-facing (counter-clockwise).
    return {{fromLeft, fromRight, toRight, toLeft}};
}

} // namespace locus::app
