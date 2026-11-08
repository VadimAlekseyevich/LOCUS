#pragma once
#include "app/screen_flow.h"
#include "app/transition_easing.h"
namespace locus::app {
// Input and audio switch instantly; only the rendered presentation fades.
// Entering Play immediately displays Play to avoid hiding music progress.
inline constexpr double kScreenFadeOutSeconds = 0.065;
inline constexpr double kScreenFadeInSeconds = 0.095;
inline constexpr double kScreenEnterPlaySeconds = 0.100;
struct ScreenFadeFrame {
    Screen visibleScreen = Screen::Home;
    float opacity = 0.0f;
};
class ScreenFadeTransition {
public:
    void Reset(Screen screen = Screen::Home) noexcept {
        from_ = to_ = screen;
        startedAt_ = 0.0;
        active_ = false;
        enteringPlay_ = false;
    }
    void Begin(Screen previous, Screen next, double now) noexcept {
        if (previous == next) return;
        // Play->Results already has its own full blackout and reveal.
        if (previous == Screen::Play && next == Screen::Results) {
            Reset(next); return;
        }
        if (!std::isfinite(now)) { Reset(next); return; }
        const Screen visible = Frame(now).visibleScreen;
        from_ = active_ ? visible : previous;
        to_ = next;
        startedAt_ = now;
        enteringPlay_ = next == Screen::Play;
        active_ = true;
    }
    ScreenFadeFrame Frame(double now) const noexcept {
        if (!active_ || !std::isfinite(now)) return {to_, 0.0f};
        const double elapsed = std::max(0.0, now - startedAt_);
        if (enteringPlay_)
            return {to_, static_cast<float>(
                1.0 - EaseTime(elapsed, 0.0, kScreenEnterPlaySeconds))};
        if (elapsed < kScreenFadeOutSeconds)
            return {from_, static_cast<float>(
                EaseTime(elapsed, 0.0, kScreenFadeOutSeconds))};
        return {to_, static_cast<float>(1.0 - EaseTime(
            elapsed - kScreenFadeOutSeconds, 0.0, kScreenFadeInSeconds))};
    }
    bool Active(double now) const noexcept {
        return active_ && std::isfinite(now) &&
               now - startedAt_ < (enteringPlay_
                   ? kScreenEnterPlaySeconds
                   : kScreenFadeOutSeconds + kScreenFadeInSeconds);
    }
private:
    Screen from_ = Screen::Home;
    Screen to_ = Screen::Home;
    double startedAt_ = 0.0;
    bool active_ = false;
    bool enteringPlay_ = false;
};
} // namespace locus::app
