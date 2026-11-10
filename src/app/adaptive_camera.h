#pragma once

#include "core/route.h"

#include <algorithm>
#include <cmath>

namespace locus::app {

// Experimental presentation-only follower. Switch to false to return to
// the original fixed-center camera without changing saved files or scoring.
inline constexpr bool kExperimentalAdaptiveCamera = true;
inline constexpr double kAdaptiveCameraCatchupSeconds = 0.18;
inline constexpr double kAdaptiveCameraMaxOffsetPx = 72.0;

// Follows the CURRENT player position with a small, speed-proportional lag.
// It never queries route headings, past/future notes, or upcoming turns.
//
// The closed-form exponential update treats each frame's measured player
// movement as constant velocity. Thus steady motion gives almost identical
// results at 60/120/240 FPS, including arbitrary viewport/practice scales.
// Store the displacement in screen pixels, so changing px/tick doesn't make
// an existing lag jump. The camera and the route use the same world transform.
class AdaptiveCameraFollower {
public:
    void Reset() noexcept {
        initialized_ = false;
        lagX_ = lagY_ = 0.0;
    }

    core::GridPoint Follow(core::GridPoint player, float pixelsPerUnit,
                           double now, bool frozen = false,
                           double cameraLagMs = 180.0) noexcept {
        if (!kExperimentalAdaptiveCamera ||
            !std::isfinite(cameraLagMs) || cameraLagMs <= 0.0 ||
            !std::isfinite(player.x) || !std::isfinite(player.y) ||
            !std::isfinite(pixelsPerUnit) || pixelsPerUnit <= 0.0f ||
            !std::isfinite(now)) {
            Reset();
            return player;
        }
        if (!initialized_ || now < lastTime_) {
            initialized_ = true;
            lastTime_ = now;
            lastPlayer_ = player;
            lagX_ = lagY_ = 0.0;
            return player;
        }

        // A paused transport or frozen outro preserves the exact picture.
        // Reset the frame clock without consuming any player motion.
        if (frozen) {
            lastTime_ = now;
            return Anchor(player, pixelsPerUnit);
        }

        const double dt = now - lastTime_;
        if (dt > 0.0) {
            const double catchupSeconds = cameraLagMs / 1000.0;
            const double decay = std::exp(-dt / catchupSeconds);
            const double velocityResponse = catchupSeconds *
                (-std::expm1(-dt / catchupSeconds)) / dt;
            const double dx = (player.x - lastPlayer_.x) * pixelsPerUnit;
            const double dy = (player.y - lastPlayer_.y) * pixelsPerUnit;
            lagX_ = lagX_ * decay + dx * velocityResponse;
            lagY_ = lagY_ * decay + dy * velocityResponse;

            // Keep the avatar comfortably inside the 540x540 play square
            // even at the maximum user-configurable movement speed.
            const double distance = std::hypot(lagX_, lagY_);
            if (!std::isfinite(distance)) {
                lagX_ = lagY_ = 0.0;
            } else if (distance > kAdaptiveCameraMaxOffsetPx) {
                const double scale = kAdaptiveCameraMaxOffsetPx / distance;
                lagX_ *= scale;
                lagY_ *= scale;
            }
        }
        lastTime_ = now;
        lastPlayer_ = player;
        return Anchor(player, pixelsPerUnit);
    }

private:
    core::GridPoint Anchor(core::GridPoint player,
                           float pixelsPerUnit) const noexcept {
        return {player.x - lagX_ / pixelsPerUnit,
                player.y - lagY_ / pixelsPerUnit};
    }

    bool initialized_ = false;
    double lastTime_ = 0.0;
    core::GridPoint lastPlayer_{};
    double lagX_ = 0.0;
    double lagY_ = 0.0;
};

} // namespace locus::app
