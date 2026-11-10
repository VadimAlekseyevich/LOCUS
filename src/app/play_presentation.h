#pragma once
#include "app/adaptive_camera.h"
#include "app/gameplay_ui.h"
#include "core/timed_route.h"
#include "app/play_outro_transition.h"
#include <string>

namespace locus::app {
// Owns all presentation-only state during PLAY. This class never judges a
// note, mutates the transport, draws via raylib or touches file storage.
class PlayPresentation {
public:
    PlayOutroTransition& Outro() noexcept { return outro_; }
    void EnterPlay() noexcept;
    void Pause(double now) noexcept { outro_.Pause(now); }
    void Resume(double now) noexcept { outro_.Resume(now); }
    void ShowAdjustment(std::string message, double now);
    // The borrowed GameplayView references live for the duration of RenderFrame.
    // Fill its read-only visual fields without exposing camera/animation state.
    GameplayView Prepare(GameplayView view, bool playing,
                         bool quitConfirmation, double now);
private:
    void ResetCamera() noexcept;
    PlayOutroTransition outro_;
    AdaptiveCameraFollower camera_;
    double lastVisualSeconds_ = 0.0;
    core::TimedRoute route_;
    bool routeReady_ = false;
    bool wasPlaying_ = false;
    std::string adjustmentNotice_;
    double adjustmentNoticeUntil_ = -1.0;
};
} // namespace locus::app
