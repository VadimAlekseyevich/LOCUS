#pragma once

#include "app/transition_easing.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

// MENU / EDITOR CHROME ONLY. Never attach this animation API to Screen::Play,
// gameplay HUD, pause overlay, scoring visuals, path/marker/camera, or
// PlayPresentation/GameSession. Existing gameplay effects stay unchanged.
// Motion uses monotonic wall-clock time; hitboxes and input are never animated.
// Menu controllers supply interaction state; this header never polls input.
struct UiMotionDurations {
    double hoverSeconds = 0.130;
    double pressSeconds = 0.080;
    double selectionSeconds = 0.160;
    double modalEnterSeconds = 0.170;
    double modalExitSeconds = 0.130;
};

struct UiInteraction {
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool selected = false;
    bool disabled = false;
};

struct UiMotionFrame {
    float hover = 0.0f;
    float press = 0.0f;
    float focus = 0.0f;
    float selected = 0.0f;
};

class UiMotionTrack {
public:
    void Snap(float value) noexcept {
        from_ = to_ = std::clamp(value, 0.0f, 1.0f);
        startedAt_ = 0.0;
        duration_ = 0.0;
    }

    void Set(bool active, double now, double duration,
             bool reducedMotion = false) noexcept {
        const float target = active ? 1.0f : 0.0f;
        if (reducedMotion || !std::isfinite(now) ||
            !std::isfinite(duration) || duration <= 0.0) {
            Snap(target);
            return;
        }
        if (to_ == target) return; // Do not restart every frame.
        from_ = Value(now);
        to_ = target;
        startedAt_ = now;
        duration_ = duration;
    }

    float Value(double now) const noexcept {
        if (duration_ <= 0.0 || !std::isfinite(now)) return to_;
        return EaseMix(from_, to_,
            std::clamp((now - startedAt_) / duration_, 0.0, 1.0));
    }

private:
    float from_ = 0.0f;
    float to_ = 0.0f;
    double startedAt_ = 0.0;
    double duration_ = 0.0;
};

class UiMotionState {
public:
    void Reset() noexcept {
        hover_.Snap(0.0f);
        press_.Snap(0.0f);
        focus_.Snap(0.0f);
        selected_.Snap(0.0f);
    }

    void Update(UiInteraction state, double now, bool reducedMotion = false,
                UiMotionDurations durations = {}) noexcept {
        hover_.Set(state.hovered && !state.disabled, now,
                   durations.hoverSeconds, reducedMotion);
        press_.Set(state.pressed && !state.disabled, now,
                   durations.pressSeconds, reducedMotion);
        focus_.Set(state.focused && !state.disabled, now,
                   durations.selectionSeconds, reducedMotion);
        selected_.Set(state.selected, now,
                      durations.selectionSeconds, reducedMotion);
    }

    UiMotionFrame Frame(double now) const noexcept {
        return {hover_.Value(now), press_.Value(now),
                focus_.Value(now), selected_.Value(now)};
    }

private:
    UiMotionTrack hover_;
    UiMotionTrack press_;
    UiMotionTrack focus_;
    UiMotionTrack selected_;
};

class UiDialogMotion {
public:
    void Reset() noexcept { visibility_.Snap(0.0f); }

    void SetOpen(bool open, double now, bool reducedMotion = false,
                 UiMotionDurations durations = {}) noexcept {
        visibility_.Set(open, now, open ? durations.modalEnterSeconds
                                      : durations.modalExitSeconds,
                        reducedMotion);
    }

    float Opacity(double now) const noexcept {
        return visibility_.Value(now);
    }

    // Draw-only offset. Actual input/hit geometry remains unchanged.
    float OffsetY(double now) const noexcept {
        return (1.0f - Opacity(now)) * 6.0f;
    }

private:
    UiMotionTrack visibility_;
};

} // namespace locus::app
