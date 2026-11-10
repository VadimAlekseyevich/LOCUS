#include "app/play_presentation.h"
#include "app/movement_speed.h"
#include "core/route_view.h"
#include <utility>

namespace locus::app {
void PlayPresentation::ResetCamera() noexcept {
    camera_.Reset();
    wasPlaying_ = false;
    lastBeat_ = 0.0;
}
void PlayPresentation::EnterPlay() noexcept {
    outro_.Reset();
    ResetCamera();
}
void PlayPresentation::ShowAdjustment(std::string message, double now) {
    adjustmentNotice_ = std::move(message);
    adjustmentNoticeUntil_ = now + 1.5;
}
GameplayView PlayPresentation::Prepare(GameplayView view, bool playing,
                                       bool quitConfirmation, double now) {
    view.outro = outro_.Frame(now);
    view.adjustmentNoticeUntil = adjustmentNoticeUntil_;
    view.adjustmentNotice = adjustmentNotice_.c_str();
    if (!playing) {
        ResetCamera();
        return view;
    }
    // Camera follows the current position only; future turns never affect it.
    // Re-entering PLAY or a backwards restart resets accumulated lag.
    const double visualBeat = view.outro.active
        ? view.outro.frozenBeat : view.beat;
    if (!wasPlaying_ || visualBeat < lastBeat_ - 0.01)
        camera_.Reset();
    wasPlaying_ = true;
    lastBeat_ = visualBeat;
    const float step = StepForFixedSpeed(
        view.skin.baseSpeedPxPerSecond, view.chart.bpm,
        view.chart.ticksPerBeat, view.practiceSpeed);
    const core::GridPoint player = core::RoutePositionAt(view.chart, visualBeat);
    view.cameraAnchor = camera_.Follow(
        player, step, now,
        view.paused || view.outro.active || quitConfirmation,
        view.skin.cameraLagMs);
    view.hasCameraAnchor = true;
    return view;
}
} // namespace locus::app
