#pragma once

#include "core/route.h"

#include <vector>

namespace locus::core {

// Compiled gameplay geometry. Editor ticks are NOT event times: a chart
// turn's absolute timeMs is authoritative here, as it is in TurnJudge.
// Positions remain in legacy world units (one unit = one editor tick at
// the chart's current BPM); projected px/s is controlled by the camera.
struct TimedTurn {
    double songSeconds = 0.0;
    GridPoint position{};
    Heading incoming = Heading::Right;
    Heading outgoing = Heading::Right;
};

struct TimedRouteSpan {
    double fromSeconds = 0.0;
    double toSeconds = 0.0;
    GridPoint from{};
    GridPoint to{};
    Heading heading = Heading::Right;
};

class TimedRoute {
public:
    // Compile once after activating a chart. The borrowed chart is NOT
    // retained: editor changes cannot invalidate a gameplay route silently.
    bool Compile(const Chart& chart, double offsetSeconds);
    bool Ready() const noexcept { return ready_; }
    double StartSeconds() const noexcept { return startSeconds_; }
    double EndSeconds() const noexcept { return endSeconds_; }
    int TicksPerBeat() const noexcept { return ticksPerBeat_; }
    double UnitsPerSecond() const noexcept { return unitsPerSecond_; }

    // O(log event count), clamped to the chart's playable time span.
    GridPoint PositionAt(double songSeconds) const noexcept;
    // O(log event count + visible runs). Returned spans preserve every
    // authored event boundary, including repeated same-direction notes.
    std::vector<TimedRouteSpan> Window(double startSeconds,
                                       double endSeconds) const;
    const std::vector<TimedTurn>& Turns() const noexcept { return turns_; }

private:
    struct Run {
        double fromSeconds = 0.0;
        double toSeconds = 0.0;
        GridPoint from{};
        Heading heading = Heading::Right;
    };
    GridPoint PointOn(const Run& run, double seconds) const noexcept;

    std::vector<Run> runs_;
    std::vector<TimedTurn> turns_;
    GridPoint start_{};
    GridPoint end_{};
    double startSeconds_ = 0.0;
    double endSeconds_ = 0.0;
    double unitsPerSecond_ = 0.0;
    int ticksPerBeat_ = 1;
    bool ready_ = false;
};

} // namespace locus::core
