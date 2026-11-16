#pragma once
#include "app/screen_flow.h"
#include "app/transition_easing.h"
#include <cmath>

namespace locus::app {

// The RmlUi document switches immediately. Holding the old Screen during
// fade-out used to expose the legacy raylib fallback behind hidden RML.
inline constexpr double kScreenFadeInSeconds = 0.16;
inline constexpr double kScreenEnterPlaySeconds = 0.10;
struct ScreenFadeFrame {
    Screen visibleScreen = Screen::Home;
    float opacity = 0.0f;
};
class ScreenFadeTransition {
public:
    void Reset(Screen screen = Screen::Home) noexcept {
        to_ = screen;
        startedAt_ = 0.0;
        active_ = false;
        enteringPlay_ = false;
    }
    void Begin(Screen previous, Screen next, double now) noexcept {
        if (previous == next) return;
        if (previous == Screen::Play && next == Screen::Results) {
            Reset(next);
            return;
        }
        if (!std::isfinite(now)) {
            Reset(next);
            return;
        }
        to_ = next;
        startedAt_ = now;
        enteringPlay_ = next == Screen::Play;
        active_ = true;
    }
    ScreenFadeFrame Frame(double now) const noexcept {
        if (!active_ || !std::isfinite(now)) return {to_, 0.0f};
        const double duration = enteringPlay_
            ? kScreenEnterPlaySeconds : kScreenFadeInSeconds;
        return {to_, static_cast<float>(
            1.0 - EaseTime(std::max(0.0, now - startedAt_), 0.0, duration))};
    }
    bool Active(double now) const noexcept {
        return active_ && std::isfinite(now) &&
               now - startedAt_ < (enteringPlay_
                   ? kScreenEnterPlaySeconds : kScreenFadeInSeconds);
    }
private:
    Screen to_ = Screen::Home;
    double startedAt_ = 0.0;
    bool active_ = false;
    bool enteringPlay_ = false;
};
} // namespace locus::app
