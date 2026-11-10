#include "app/play_presentation.h"
#include "app/movement_speed.h"
#include "core/route_view.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace {
bool Check(bool pass, const char* name) {
    if (!pass) std::cerr << "PLAY_PRESENTATION_FAIL " << name << '\n';
    return pass;
}
bool Near(double a, double b, double eps = 0.001) {
    return std::abs(a - b) < eps;
}
} // namespace

int main() {
    using namespace locus;
    bool ok = true;
    core::Chart chart = core::MakeDemoChart();
    chart.turns.clear();
    chart.initialHeading = core::Heading::Right;
    chart.lengthBeats = 64;
    core::TurnJudge judge(chart);
    app::HitFlash flash;
    app::GameSettings settings;
    app::Skin skin;
    std::string title = "presentation test";
    app::PlayPresentation presentation;

    const auto view = [&](double beat, bool paused = false) {
        return app::GameplayView{chart, judge, flash, title,
            beat * 0.5, 0.0, paused, false, false, 1.0,
            "PLAYING", true, settings, skin};
    };
    const float step = app::StepForFixedSpeed(
        skin.baseSpeedPxPerSecond, chart.bpm, chart.ticksPerBeat, 1.0);
    const auto player = [&](double beat) {
        return core::RoutePositionAt(chart, beat);
    };

    presentation.EnterPlay();
    auto first = presentation.Prepare(view(0.0), true, false, 0.0);
    auto moving = presentation.Prepare(view(2.0), true, false, 1.0);
    ok &= Check(first.hasCameraAnchor &&
                Near(first.cameraAnchor.x, player(0.0).x) &&
                moving.hasCameraAnchor &&
                (player(2.0).x - moving.cameraAnchor.x) * step > 0.0 &&
                (player(2.0).x - moving.cameraAnchor.x) * step <=
                    app::kAdaptiveCameraMaxOffsetPx,
                "a new session centers first; movement trails without lookahead");

    auto paused = presentation.Prepare(view(2.0, true), true, false, 10.0);
    auto resumed = presentation.Prepare(view(2.0), true, false, 10.0);
    ok &= Check(Near(paused.cameraAnchor.x, moving.cameraAnchor.x) &&
                Near(resumed.cameraAnchor.x, moving.cameraAnchor.x),
                "long pause preserves exact camera anchor");

    auto beforeOutro = presentation.Prepare(view(3.0), true, false, 10.5);
    presentation.Outro().Begin(chart, 1.5, 0.0, 11.0);
    auto duringOutro = presentation.Prepare(view(8.0), true, false, 11.5);
    ok &= Check(duringOutro.outro.active &&
                Near(duringOutro.outro.frozenSongSeconds, 1.5) &&
                Near(duringOutro.cameraAnchor.x, beforeOutro.cameraAnchor.x),
                "outro freezes current beat and preserves camera lag");

    const float beforeModalDistance = duringOutro.outro.distancePx;
    presentation.Pause(11.5);
    auto modal = presentation.Prepare(view(8.0), true, true, 25.0);
    presentation.Resume(25.0);
    auto afterModal = presentation.Prepare(view(8.0), true, false, 25.0);
    ok &= Check(Near(modal.outro.distancePx, beforeModalDistance) &&
                Near(afterModal.outro.distancePx, beforeModalDistance) &&
                Near(modal.cameraAnchor.x, duringOutro.cameraAnchor.x),
                "quit dialog freezes the outro and its camera independently of clock");

    presentation.Outro().BeginResults(25.5);
    const auto results = presentation.Prepare(view(8.0), false, false, 25.5);
    ok &= Check(!results.hasCameraAnchor && !results.outro.active &&
                Near(results.outro.resultBlackOpacity, 1.0),
                "results animation remains available without an active camera");

    presentation.EnterPlay();
    auto retried = presentation.Prepare(view(5.0), true, false, 26.0);
    ok &= Check(retried.hasCameraAnchor &&
                Near(retried.cameraAnchor.x, player(5.0).x) &&
                !retried.outro.active && retried.outro.resultBlackOpacity == 0.0f,
                "retry discards the prior outro and camera displacement");

    presentation.ShowAdjustment("SPEED 200 PX/S", 26.0);
    auto shown = presentation.Prepare(view(5.0), true, false, 26.1);
    ok &= Check(std::strcmp(shown.adjustmentNotice, "SPEED 200 PX/S") == 0 &&
                Near(shown.adjustmentNoticeUntil, 27.5),
                "transient adjustment notice is owned by presentation");

    skin.cameraLagMs = 0.0f;
    presentation.Prepare(view(6.0), true, false, 26.5);
    auto disabled = presentation.Prepare(view(7.0), true, false, 27.0);
    ok &= Check(Near(disabled.cameraAnchor.x, player(7.0).x),
                "skin camera lag zero restores fixed center");

    skin.cameraLagMs = 180.0f;
    presentation.Prepare(view(8.0), true, false, 27.5);
    auto rewound = presentation.Prepare(view(1.0), true, false, 28.0);
    ok &= Check(Near(rewound.cameraAnchor.x, player(1.0).x),
                "backward seek or restart resets accumulated camera lag");

    const auto outside = presentation.Prepare(view(1.0), false, false, 28.5);
    const auto again = presentation.Prepare(view(1.0), true, false, 29.0);
    ok &= Check(!outside.hasCameraAnchor && again.hasCameraAnchor &&
                Near(again.cameraAnchor.x, player(1.0).x),
                "leaving and reentering PLAY cannot carry camera state");

    if (ok) std::cout << "PLAY_PRESENTATION_PASS camera=1 outro=1 modal=1 retry=1\n";
    return ok ? 0 : 1;
}
