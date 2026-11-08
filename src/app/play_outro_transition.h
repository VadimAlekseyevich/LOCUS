#pragma once

#include "core/route.h"
#include "core/route_view.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

// Presentation-only post-judgement phase. The audio clock is NOT stopped;
// only note input stops. Camera locks at the last judged position while the
// avatar accelerates along the outgoing heading and exits the play square.
struct PlayOutroFrame {
    bool active = false;
    core::GridPoint frozenCamera{};
    double frozenBeat = 0.0;
    core::Heading outgoing = core::Heading::Right;
    float distancePx = 0.0f;
    float blackOpacity = 0.0f;
    float resultBlackOpacity = 0.0f;
};

inline float EaseCubic(double progress) noexcept {
    progress = std::clamp(progress, 0.0, 1.0);
    return static_cast<float>(progress * progress * (3.0 - 2.0 * progress));
}

inline constexpr double kPlayOutroSeconds = 0.96;
inline constexpr double kResultsRevealSeconds = 0.34;

class PlayOutroTransition {
public:
    void Reset() noexcept {
        active_ = false;
        resultsAt_ = -1.0;
        paused_ = false;
    }
    void Pause(double now) noexcept {
        if (!paused_) { paused_ = true; pausedAt_ = now; }
    }
    void Resume(double now) noexcept {
        if (!paused_) return;
        const double held = std::max(0.0, now - pausedAt_);
        if (active_) start_ += held;
        if (resultsAt_ >= 0.0) resultsAt_ += held;
        paused_ = false;
    }
    double Time(double now) const noexcept {
        return paused_ ? pausedAt_ : now;
    }

    void Begin(const core::Chart& chart, double beat, double now) {
        if (active_ || chart.lengthBeats <= chart.firstBeat) return;
        const double safe = std::clamp(beat,
            static_cast<double>(chart.firstBeat),
            static_cast<double>(chart.lengthBeats) - 0.000001);
        anchor_ = core::RoutePositionAt(chart, safe);
        anchorBeat_ = safe;
        const int tick = static_cast<int>(std::floor(safe));
        // A final note can score EARLY. Take the authored outgoing
        // direction of the last turn, not the direction sampled just
        // before the judgement timestamp reaches that turn.
        const int exitTick = chart.turns.empty()
            ? tick : chart.turns.back().beat;
        outgoing_ = core::SampleRouteStep(chart, exitTick).heading;
        start_ = now;
        active_ = true;
        resultsAt_ = -1.0;
    }

    bool Active() const noexcept { return active_; }
    bool Done(double now) const noexcept {
        return active_ && Time(now) - start_ >= kPlayOutroSeconds;
    }
    void BeginResults(double now) noexcept {
        active_ = false;
        resultsAt_ = now;
    }

    PlayOutroFrame Frame(double now) const noexcept {
        PlayOutroFrame result;
        now = Time(now);
        if (active_) {
            result.active = true;
            result.frozenCamera = anchor_;
            result.frozenBeat = anchorBeat_;
            result.outgoing = outgoing_;
            const double t = std::clamp(
                (now - start_) / kPlayOutroSeconds, 0.0, 1.0);
            // Slow departure first, then rapid exit. Full opacity arrives
            // only as the marker leaves the square.
            result.distancePx = 660.0f *
                EaseCubic(std::clamp((t - 0.04) / 0.82, 0.0, 1.0));
            result.blackOpacity =
                EaseCubic(std::clamp((t - 0.20) / 0.80, 0.0, 1.0));
        } else if (resultsAt_ >= 0.0) {
            const double t = (now - resultsAt_) / kResultsRevealSeconds;
            result.resultBlackOpacity = 1.0f - EaseCubic(t);
        }
        return result;
    }

private:
    bool active_ = false;
    bool paused_ = false;
    double pausedAt_ = 0.0;
    double start_ = 0.0;
    double resultsAt_ = -1.0;
    core::GridPoint anchor_{};
    double anchorBeat_ = 0.0;
    core::Heading outgoing_ = core::Heading::Right;
};

} // namespace locus::app
