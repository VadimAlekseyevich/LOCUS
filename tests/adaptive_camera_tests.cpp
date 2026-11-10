#include "app/adaptive_camera.h"

#include <cmath>
#include <iostream>

namespace {
bool Check(bool condition, const char* description) {
    if (!condition) std::cerr << "ADAPTIVE_CAMERA_FAIL " << description << '\n';
    return condition;
}
bool Near(double a, double b, double tolerance = 0.02) {
    return std::abs(a - b) < tolerance;
}
double OffsetX(locus::core::GridPoint marker,
               locus::core::GridPoint camera, float pixelsPerUnit) {
    return (marker.x - camera.x) * pixelsPerUnit;
}
double OffsetY(locus::core::GridPoint marker,
               locus::core::GridPoint camera, float pixelsPerUnit) {
    return (marker.y - camera.y) * pixelsPerUnit;
}
struct DriveResult {
    locus::core::GridPoint player;
    locus::core::GridPoint camera;
};
DriveResult Drive(float speedPxPerSecond, float pixelsPerUnit,
                  int fps, double duration = 2.0) {
    locus::app::AdaptiveCameraFollower follow;
    const int frames = static_cast<int>(duration * fps);
    auto camera = follow.Follow({0.0, 0.0}, pixelsPerUnit, 0.0);
    locus::core::GridPoint player{};
    for (int frame = 1; frame <= frames; ++frame) {
        const double t = static_cast<double>(frame) / fps;
        player = {speedPxPerSecond * t / pixelsPerUnit, 0.0};
        camera = follow.Follow(player, pixelsPerUnit, t);
    }
    return {player, camera};
}
}

int main() {
    using namespace locus;
    bool ok = true;

    const auto slow = Drive(100.0f, 3.0f, 60);
    const auto fast = Drive(200.0f, 3.0f, 60);
    ok &= Check(OffsetX(slow.player, slow.camera, 3.0f) > 0.0 &&
                Near(OffsetX(slow.player, slow.camera, 3.0f), 18.0) &&
                Near(OffsetX(fast.player, fast.camera, 3.0f), 36.0),
                "camera trails the current player and lag grows linearly with speed");

    const auto sixty = Drive(135.0f, 2.5f, 60);
    const auto oneTwenty = Drive(135.0f, 2.5f, 120);
    const auto twoForty = Drive(135.0f, 2.5f, 240);
    ok &= Check(Near(OffsetX(sixty.player, sixty.camera, 2.5f),
                     OffsetX(oneTwenty.player, oneTwenty.camera, 2.5f)) &&
                Near(OffsetX(oneTwenty.player, oneTwenty.camera, 2.5f),
                     OffsetX(twoForty.player, twoForty.camera, 2.5f)),
                "closed-form follow update is independent of the rendering FPS");

    const auto highSpeed = Drive(2000.0f, 2.5f, 120);
    ok &= Check(Near(OffsetX(highSpeed.player, highSpeed.camera, 2.5f),
                     app::kAdaptiveCameraMaxOffsetPx),
                "maximum speed cannot push the player out of the play square");

    // Camera sees only the moving avatar, not the next turn. At the instant
    // the player starts turning, the old-direction lag remains; it then
    // rotates gradually as the camera catches up to the new movement.
    app::AdaptiveCameraFollower corner;
    constexpr float scale = 4.0f;
    auto anchor = corner.Follow({0.0, 0.0}, scale, 0.0);
    for (int frame = 1; frame <= 60; ++frame) {
        const double t = frame / 60.0;
        anchor = corner.Follow({30.0 * t, 0.0}, scale, t);
    }
    const double oldLag = OffsetX({30.0, 0.0}, anchor, scale);
    anchor = corner.Follow({30.0, 0.0}, scale, 1.0);
    ok &= Check(oldLag > 10.0 &&
                Near(OffsetX({30.0, 0.0}, anchor, scale), oldLag),
                "camera does not move early to anticipate a corner");
    anchor = corner.Follow({30.0, -0.5}, scale, 1.0 + 1.0 / 60.0);
    ok &= Check(OffsetX({30.0, -0.5}, anchor, scale) > 0.0 &&
                OffsetX({30.0, -0.5}, anchor, scale) < oldLag &&
                OffsetY({30.0, -0.5}, anchor, scale) < 0.0,
                "turn follows player motion while the old-direction lag decays");

    app::AdaptiveCameraFollower pause;
    pause.Follow({0.0, 0.0}, scale, 0.0);
    auto moving = pause.Follow({10.0, 0.0}, scale, 0.5);
    const double lagBefore = OffsetX({10.0, 0.0}, moving, scale);
    auto frozen = pause.Follow({10.0, 0.0}, scale, 200.0, true);
    auto unpaused = pause.Follow({10.0, 0.0}, scale, 200.0);
    ok &= Check(Near(OffsetX({10.0, 0.0}, frozen, scale), lagBefore) &&
                Near(OffsetX({10.0, 0.0}, unpaused, scale), lagBefore),
                "pause and outro do not consume catch-up time or jump");

    // Scaling world units to screen pixels must not change camera lag.
    auto reScaled = pause.Follow({10.0, 0.0}, 2.0f, 200.0);
    ok &= Check(Near(OffsetX({10.0, 0.0}, reScaled, 2.0f), lagBefore),
                "practice and zoom changes keep the same on-screen offset");
    pause.Reset();
    const auto restarted = pause.Follow({0.0, 0.0}, scale, 201.0);
    ok &= Check(Near(OffsetX({0.0, 0.0}, restarted, scale), 0.0),
                "restart and chart change reset the follow state");

    if (ok) std::cout << "ADAPTIVE_CAMERA_PASS lag=1 speed=1 fps=1 turns=1\n";
    return ok ? 0 : 1;
}
