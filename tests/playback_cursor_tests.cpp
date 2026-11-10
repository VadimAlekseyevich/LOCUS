#include "app/adaptive_camera.h"
#include "app/playback_cursor_smoother.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {
bool Check(bool ok, const char* message) {
    if (!ok) std::cerr << "PLAYBACK_CURSOR_FAIL " << message << '\n';
    return ok;
}
bool Near(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) <= tolerance;
}
} // namespace

int main() {
    using locus::app::PlaybackCursorSmoother;
    using locus::app::AdaptiveCameraFollower;
    bool ok = true;

    // GetMusicTimePlayed() can advance only when the next audio buffer is
    // consumed. Recreate 50ms source-time packets at slowed rates while
    // calling the renderer at 60, 120 and 240 FPS.
    for (const int fps : {60, 120, 240}) {
        for (const double rate : {0.5, 0.25, 0.1}) {
            for (const double speed : {200.0, 400.0, 600.0}) {
                PlaybackCursorSmoother smoother;
                AdaptiveCameraFollower camera;
                double previousDisplay = 0.0;
                double maximumRawStepPx = 0.0;
                double maximumVisualJitterPx = 0.0;
                double previousRaw = 0.0;
                double previousCameraOffset = 0.0;
                double maxCameraOffsetJump = 0.0;
                constexpr double kAudioPacket = 0.05;
                constexpr float kPixelsPerWorldUnit = 100.0f;
                for (int i = 0; i <= fps * 4; ++i) {
                    const double t = static_cast<double>(i) / fps;
                    const double trueAudio = t * rate;
                    const double raw = std::floor(
                        (trueAudio + 1e-9) / kAudioPacket) * kAudioPacket;
                    const double display = smoother.Sample(raw, t, rate);
                    const double scale = speed / rate;
                    if (i > 0) {
                        maximumRawStepPx = std::max(maximumRawStepPx,
                            (raw - previousRaw) * scale);
                        const double framePx = (display - previousDisplay) * scale;
                        maximumVisualJitterPx = std::max(maximumVisualJitterPx,
                            std::abs(framePx - speed / fps));
                    }
                    const locus::core::GridPoint player{
                        display * scale / kPixelsPerWorldUnit, 0.0};
                    const auto anchor = camera.Follow(
                        player, kPixelsPerWorldUnit, t, false, 180.0);
                    const double offsetPx =
                        (player.x - anchor.x) * kPixelsPerWorldUnit;
                    if (i > fps) {
                        maxCameraOffsetJump = std::max(maxCameraOffsetJump,
                            std::abs(offsetPx - previousCameraOffset));
                    }
                    previousCameraOffset = offsetPx;
                    previousDisplay = display;
                    previousRaw = raw;
                }
                ok &= Check(maximumRawStepPx >= 18.0 &&
                            maximumVisualJitterPx < 1.0 &&
                            maxCameraOffsetJump < 2.0,
                            "200-600px/s slow-play cursor and camera have no audio-packet jerks");
            }
        }
    }

    PlaybackCursorSmoother clock;
    clock.Sample(1.0, 10.0, 0.5);
    const double moving = clock.Sample(1.5, 11.0, 0.5);
    const double frozen = clock.Sample(1.5, 200.0, 0.5, true);
    const double unpaused = clock.Sample(1.5, 201.0, 0.5, false);
    const double resumed = clock.Sample(1.55, 201.1, 0.5, false);
    ok &= Check(Near(moving, 1.5) && Near(frozen, 1.5) &&
                Near(unpaused, frozen) && Near(resumed, 1.55),
                "pause/resume excludes elapsed paused wall time");

    // Seeking backwards is a deliberate discontinuity: it resets the
    // visual position, rather than easing through the entire old chart.
    const double seek = clock.Sample(0.4, 201.2, 0.5);
    ok &= Check(Near(seek, 0.4) &&
                Near(clock.Sample(0.45, 201.3, 0.5), 0.45),
                "seek resets phase instantly and resumes smooth playback");

    // A changing practice rate must not reset the displayed chart position.
    clock.Reset();
    clock.Sample(0.0, 0.0, 0.5);
    const double half = clock.Sample(0.5, 1.0, 0.5);
    const double quarter = clock.Sample(0.625, 1.5, 0.25);
    ok &= Check(Near(half, 0.5) && Near(quarter, 0.625),
                "practice speed changes preserve time continuity");

    // A genuine stream slowdown is different from normal buffer stepping.
    // The predictor gradually corrects drift rather than blindly using an
    // unrelated monotonic song clock forever.
    clock.Reset();
    double previous = clock.Sample(0.0, 0.0, 0.5);
    for (int i = 1; i <= 360; ++i) {
        const double now = i / 120.0;
        const double slowAudio = now * 0.45;
        const double quantized = std::floor((slowAudio + 1e-9) / 0.05) * 0.05;
        const double current = clock.Sample(quantized, now, 0.5);
        ok &= Check(current >= previous,
                    "a stalled or drifting audio stream cannot make the marker reverse");
        previous = current;
    }
    ok &= Check(std::abs(previous - 1.35) < 0.18,
                "visual cursor remains bounded to a persistently slower audio stream");
    clock.Reset();
    ok &= Check(Near(clock.Sample(7.0, 100.0, 0.5), 7.0),
                "fresh song or retry uses a new audio cursor anchor");

    if (ok) std::cout << "PLAYBACK_CURSOR_PASS rates=0.1-0.5 speed=200-600 fps=60-240\n";
    return ok ? 0 : 1;
}
