#include "app/play_outro_transition.h"

#include <cmath>
#include <iostream>

int main() {
    using namespace locus;
    bool ok = true;
    const auto ck = [&](bool v, const char* msg) {
        if (!v) std::cerr << "PLAY_OUTRO_FAIL " << msg << '\n';
        ok &= v;
    };
    ck(app::EaseSmoothstep(-10.0) == 0.0f &&
       app::EaseSmoothstep(1.0) == 1.0f &&
       std::abs(app::EaseSmoothstep(0.25) - 0.15625f) < 1e-6,
       "easing is clamped and non-linear");
    const auto chart = core::MakeDemoChart();
    core::Chart early = chart;
    early.turns = {{4, core::Heading::Left}};
    app::PlayOutroTransition earlyOutro;
    earlyOutro.Begin(early, 3.95, 4.0);
    ck(earlyOutro.Frame(4.0).outgoing == core::Heading::Left,
       "early final hit departs in authored outgoing direction");
    app::PlayOutroTransition outro;
    outro.Begin(chart, 3.25, 5.0);
    const auto a = outro.Frame(5.0);
    const auto b = outro.Frame(5.30);
    const auto c = outro.Frame(5.80);
    ck(a.active && a.distancePx == 0.0f &&
       a.blackOpacity == 0.0f &&
       b.distancePx > 0.0f && b.distancePx < c.distancePx &&
       b.blackOpacity < c.blackOpacity && c.blackOpacity < 1.0f,
       "freeze camera and accelerate character away as screen darkens");
    ck(!outro.Done(5.90) && outro.Done(6.00),
       "outro completes after approximately one second");
    outro.BeginResults(6.00);
    ck(!outro.Frame(6.00).active &&
       outro.Frame(6.00).resultBlackOpacity == 1.0f &&
       outro.Frame(6.50).resultBlackOpacity == 0.0f,
       "results fade in non-linearly from black");
    outro.Reset();
    outro.Begin(chart, 3.25, 10.0);
    outro.Pause(10.25);
    ck(!outro.Done(20.0) && outro.Frame(20.0).distancePx ==
       outro.Frame(10.25).distancePx,
       "quit modal freezes outro progression");
    outro.Resume(20.0);
    ck(!outro.Done(20.3) && outro.Done(20.8),
       "outro resumes where the modal paused it");
    outro.Reset();
    ck(!outro.Active() && outro.Frame(10.0).resultBlackOpacity == 0.0f,
       "retry and screen exits reset transition");
    if (ok) std::cout << "PLAY_OUTRO_PASS departure=1 reveal=1\n";
    return ok ? 0 : 1;
}
