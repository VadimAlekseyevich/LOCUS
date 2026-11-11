#include "app/play_presentation.h"
#include "app/movement_speed.h"
#include "core/timed_route.h"
#include <utility>

namespace locus::app {
void PlayPresentation::ResetCamera() noexcept {
    camera_.Reset();
    wasPlaying_ = false;
    lastVisualSeconds_ = 0.0;
}
void PlayPresentation::EnterPlay() noexcept {
    outro_.Reset();
    routeReady_ = false; // Chart may have changed under the stable address.
    preview_ = RoutePreviewFrame{};
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
    // Compile once per activated chart, never once per visual frame.
    // Chart mutations cannot change the cached route during an active play.
    if (!routeReady_)
        routeReady_ = route_.Compile(view.chart, view.offsetSeconds);
    view.timedRoute = routeReady_ ? &route_ : nullptr;
    if (!routeReady_) return view;

    // Presentation uses song seconds. The editor tick grid never enters
    // camera placement, route projection or outro freeze.
    const double visualSeconds = view.outro.active
        ? view.outro.frozenSongSeconds : view.songSeconds;
    if (!wasPlaying_ || visualSeconds < lastVisualSeconds_ - 0.01)
        camera_.Reset();
    wasPlaying_ = true;
    lastVisualSeconds_ = visualSeconds;
    const float step = StepForFixedSpeed(
        view.skin.baseSpeedPxPerSecond, view.chart.bpm,
        view.chart.ticksPerBeat, view.practiceSpeed);
    PrepareRoutePreview(route_, visualSeconds, view.skin.lookaheadMs, preview_);
    view.routePreview = &preview_;
    view.cameraAnchor = camera_.Follow(
        preview_.marker, step, now,
        view.paused || view.outro.active || quitConfirmation,
        view.skin.cameraLagMs);
    view.hasCameraAnchor = true;
    return view;
}
} // namespace locus::app
