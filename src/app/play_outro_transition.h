#pragma once

#include "core/route.h"
#include "core/timed_route.h"
#include "app/transition_easing.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

// Presentation-only post-judgement phase. The audio clock is NOT stopped;
// only note input stops. The camera freezes at the last judged song time,
// preserving its velocity-derived trailing offset while the avatar exits.
struct PlayOutroFrame {
    bool active = false;
    core::GridPoint frozenCamera{};
    double frozenSongSeconds = 0.0;
    core::Heading outgoing = core::Heading::Right;
    float distancePx = 0.0f;
    float blackOpacity = 0.0f;
    float resultBlackOpacity = 0.0f;
};

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

    void Begin(const core::Chart& chart, double songSeconds,
               double offsetSeconds, double now) {
        if (active_ || !std::isfinite(songSeconds)) return;
        core::TimedRoute route;
        if (!route.Compile(chart, offsetSeconds)) return;
        anchorSongSeconds_ = std::clamp(songSeconds,
            route.StartSeconds(), route.EndSeconds());
        anchor_ = route.PositionAt(anchorSongSeconds_);
        // A final note can score early: depart in the LAST authored
        // direction, even before its exact timestamp reaches the avatar.
        outgoing_ = chart.turns.empty()
            ? chart.initialHeading : chart.turns.back().direction;
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
            result.frozenSongSeconds = anchorSongSeconds_;
            result.outgoing = outgoing_;
            const double t = std::clamp(
                (now - start_) / kPlayOutroSeconds, 0.0, 1.0);
            // Slow departure first, then rapid exit. Full opacity arrives
            // only as the marker leaves the square.
            result.distancePx = 660.0f *
                static_cast<float>(EaseSmoothstep((t - 0.04) / 0.82));
            result.blackOpacity =
                static_cast<float>(EaseSmoothstep((t - 0.20) / 0.80));
        } else if (resultsAt_ >= 0.0) {
            const double t = (now - resultsAt_) / kResultsRevealSeconds;
            result.resultBlackOpacity = static_cast<float>(1.0 - EaseSmoothstep(t));
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
    double anchorSongSeconds_ = 0.0;
    core::Heading outgoing_ = core::Heading::Right;
};

} // namespace locus::app
