#include "app/path_mesh.h"

#include "app/visual_style.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace locus::app {
namespace {

struct PathSample {
    core::GridPoint position{};
    double tick = 0.0;
    float halfWidth = 0.0f;
    float opacity = 0.0f;
};

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
        sample.opacity, sample.tick, startsRun
    };
}

} // namespace

std::vector<PathRibbonRow> BuildScoredPathRibbon(
    const core::Chart& chart, double fractionalTick,
    RouteCanvas canvas, const Skin& skin) {
    std::vector<PathRibbonRow> rows;
    if (chart.lengthBeats <= chart.firstBeat ||
        !std::isfinite(fractionalTick) ||
        !std::isfinite(canvas.step) || canvas.step <= 0.0f ||
        !std::isfinite(skin.lookaheadBeats) || skin.lookaheadBeats <= 0.0f)
        return rows;

    const double tick = std::clamp(
        fractionalTick, static_cast<double>(chart.firstBeat),
        static_cast<double>(chart.lengthBeats));
    const double horizon = skin.lookaheadBeats * chart.ticksPerBeat;
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
            const float opacity = ScoredPathOpacity(
                ahead, horizon, skin.fadePower) * skin.pathOpacity;
            const float halfWidth = ScoredPathWidth(
                ahead, horizon, skin.pathWidth, skin.fadePower) * 0.5f;
            samples.push_back({point, time, halfWidth, opacity});
        }
    }
    if (samples.size() < 2) return rows;

    rows.reserve(samples.size() + 8);
    constexpr double kReverseDot = -0.9999;
    constexpr double kMiterLimit = 2.75; // Covers a 135-degree turn (2.613).
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const Direction previous = i > 0
            ? UnitDirection(samples[i - 1].position, samples[i].position)
            : UnitDirection(samples[i].position, samples[i + 1].position);
        const Direction next = i + 1 < samples.size()
            ? UnitDirection(samples[i].position, samples[i + 1].position)
            : previous;
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
    return rows;
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
