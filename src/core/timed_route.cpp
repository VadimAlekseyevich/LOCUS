#include "core/timed_route.h"
#include "core/tempo_map.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace locus::core {
namespace {
constexpr double kDiagonal = 0.707106781186547524400844362104849039;

GridPoint Unit(Heading heading) noexcept {
    switch (heading) {
        case Heading::Up: return {0.0, -1.0};
        case Heading::Right: return {1.0, 0.0};
        case Heading::Down: return {0.0, 1.0};
        case Heading::Left: return {-1.0, 0.0};
        case Heading::UpRight: return {kDiagonal, -kDiagonal};
        case Heading::DownRight: return {kDiagonal, kDiagonal};
        case Heading::DownLeft: return {-kDiagonal, kDiagonal};
        case Heading::UpLeft: return {-kDiagonal, -kDiagonal};
    }
    return {};
}

GridPoint Advance(GridPoint at, Heading heading, double units) noexcept {
    const auto delta = Unit(heading);
    return {at.x + units * delta.x, at.y + units * delta.y};
}
} // namespace

bool TimedRoute::Compile(const Chart& chart, double offsetSeconds) {
    ready_ = false;
    runs_.clear();
    turns_.clear();
    if (!std::isfinite(chart.bpm) || chart.bpm <= 0.0 ||
        chart.ticksPerBeat <= 0 || !std::isfinite(offsetSeconds) ||
        chart.lengthBeats <= chart.firstBeat ||
        !ValidHeading(chart.initialHeading))
        return false;

    ticksPerBeat_ = chart.ticksPerBeat;
    unitsPerSecond_ = chart.bpm * chart.ticksPerBeat / 60.0;
    if (!std::isfinite(unitsPerSecond_) || unitsPerSecond_ <= 0.0)
        return false;
    const double secondsPerTick = 1.0 / unitsPerSecond_;
    const double gridStart = TempoSecondsAtTick(chart, offsetSeconds,
                                               chart.firstBeat);
    const double gridEnd = TempoSecondsAtTick(chart, offsetSeconds,
                                             chart.lengthBeats);
    if (!std::isfinite(gridStart) || !std::isfinite(gridEnd) ||
        gridEnd <= gridStart) return false;

    double earliest = gridStart, latest = gridEnd;
    double previousEvent = -std::numeric_limits<double>::infinity();
    for (const auto& event : chart.turns) {
        if (!ValidHeading(event.direction) ||
            event.beat < chart.firstBeat ||
            event.beat >= chart.lengthBeats) return false;
        const double at = event.timeMs == kNoTimeMs
            ? TempoSecondsAtTick(chart, offsetSeconds, event.beat)
            : static_cast<double>(event.timeMs) / 1000.0;
        if (!std::isfinite(at) || at <= previousEvent) return false;
        previousEvent = at;
        earliest = std::min(earliest, at);
        latest = std::max(latest, at + secondsPerTick);
    }

    startSeconds_ = earliest;
    endSeconds_ = latest;
    start_ = {static_cast<double>(chart.start.x),
              static_cast<double>(chart.start.y)};
    GridPoint position = start_;
    Heading heading = chart.initialHeading;
    double cursor = startSeconds_;
    runs_.reserve(chart.turns.size() + 1);
    turns_.reserve(chart.turns.size());

    for (const auto& event : chart.turns) {
        const double at = event.timeMs == kNoTimeMs
            ? TempoSecondsAtTick(chart, offsetSeconds, event.beat)
            : static_cast<double>(event.timeMs) / 1000.0;
        if (at > cursor) {
            runs_.push_back({cursor, at, position, heading});
            position = Advance(position, heading,
                               (at - cursor) * unitsPerSecond_);
        }
        turns_.push_back({at, position, heading, event.direction});
        heading = event.direction;
        cursor = at;
    }
    if (endSeconds_ > cursor) {
        runs_.push_back({cursor, endSeconds_, position, heading});
        position = Advance(position, heading,
                           (endSeconds_ - cursor) * unitsPerSecond_);
    }
    end_ = position;
    ready_ = !runs_.empty();
    return ready_;
}

GridPoint TimedRoute::PointOn(const Run& run, double seconds) const noexcept {
    return Advance(run.from, run.heading,
                   (seconds - run.fromSeconds) * unitsPerSecond_);
}

GridPoint TimedRoute::PositionAt(double songSeconds) const noexcept {
    if (!ready_ || !std::isfinite(songSeconds)) return start_;
    if (songSeconds <= startSeconds_) return start_;
    if (songSeconds >= endSeconds_) return end_;
    const auto after = std::upper_bound(runs_.begin(), runs_.end(),
        songSeconds, [](double time, const Run& run) {
            return time < run.fromSeconds;
        });
    if (after == runs_.begin()) return start_;
    const auto& run = *(after - 1);
    return PointOn(run, std::clamp(songSeconds,
                                  run.fromSeconds, run.toSeconds));
}

std::vector<TimedRouteSpan> TimedRoute::Window(double startSeconds,
                                               double endSeconds) const {
    std::vector<TimedRouteSpan> visible;
    Window(startSeconds, endSeconds, visible);
    return visible;
}

void TimedRoute::Window(double startSeconds, double endSeconds,
                        std::vector<TimedRouteSpan>& visible) const {
    visible.clear();
    if (!ready_ || !std::isfinite(startSeconds) ||
        !std::isfinite(endSeconds) || endSeconds <= startSeconds)
        return;
    const double first = std::max(startSeconds, startSeconds_);
    const double last = std::min(endSeconds, endSeconds_);
    if (last <= first) return;

    auto after = std::upper_bound(runs_.begin(), runs_.end(),
        first, [](double time, const Run& run) {
            return time < run.fromSeconds;
        });
    auto run = after == runs_.begin() ? runs_.begin() : after - 1;
    for (; run != runs_.end() && run->fromSeconds < last; ++run) {
        const double from = std::max(first, run->fromSeconds);
        const double to = std::min(last, run->toSeconds);
        if (to <= from) continue;
        visible.push_back({from, to, PointOn(*run, from),
                           PointOn(*run, to), run->heading});
    }
}

} // namespace locus::core
