#include "core/judgement.h"
#include "core/route.h"

#include <cmath>
#include <iostream>

using namespace locus::core;

namespace {

bool Check(bool condition, const char* description) {
    if (!condition) std::cerr << "JUDGEMENT_TEST_FAIL: " << description << '\n';
    return condition;
}

bool Is(const std::optional<Judgement>& result, Grade grade, int beat) {
    return result && result->grade == grade && result->beat == beat;
}

bool SameRoute(const RouteStep& a, const RouteStep& b) {
    return a.from.x == b.from.x && a.from.y == b.from.y &&
           a.to.x == b.to.x && a.to.y == b.to.y &&
           a.heading == b.heading && a.wraps == b.wraps;
}

} // namespace

int main() {
    bool ok = true;
    const Chart demo = MakeDemoChart();
    TurnJudge judge(demo);

    // Early stray keys do not score or pre-buffer for a future event.
    ok &= Check(!judge.Press(Turn::Right, 0.1).has_value() &&
                    judge.Counts().Resolved() == 0,
                "out-of-window input must be ignored");

    const double first = 2.0 * 60.0 / demo.bpm;
    const double second = 5.0 * 60.0 / demo.bpm;
    const double third = 8.0 * 60.0 / demo.bpm;
    const double fourth = 12.0 * 60.0 / demo.bpm;

    const auto perfectEarly = judge.Press(Turn::Right, first - 0.035);
    ok &= Check(Is(perfectEarly, Grade::Perfect, 2) &&
                    perfectEarly->timingErrorSeconds < 0.0,
                "correct early input inside Perfect window");
    ok &= Check(!judge.Press(Turn::Right, first).has_value() &&
                    judge.Counts().Resolved() == 1,
                "a second key-down cannot score the same event");

    const auto goodLate = judge.Press(Turn::Left, second + 0.080);
    ok &= Check(Is(goodLate, Grade::Good, 5) &&
                    goodLate->timingErrorSeconds > 0.0,
                "correct late Good input");
    const auto badEarly = judge.Press(Turn::Left, third - 0.145);
    ok &= Check(Is(badEarly, Grade::Bad, 8),
                "correct key near the outer timing limit gives Bad");

    const auto wrong = judge.Press(Turn::Left, fourth);
    ok &= Check(Is(wrong, Grade::Miss, 12) && wrong->wrongDirection,
                "wrong key in the timing window must be Miss");
    ok &= Check(!judge.Press(Turn::Right, fourth + 0.01).has_value(),
                "wrong first key cannot be overwritten by a second key");

    // Missing a chart turn resolves exactly once when its late window ends.
    judge.Advance(8.18);
    ok &= Check(judge.Counts().Resolved() == 4 &&
                    judge.NextEvent() && judge.NextEvent()->beat == 16,
                "an event at the exact late edge must stay eligible");
    judge.Advance(8.181);
    ok &= Check(judge.Counts().miss == 2 &&
                    judge.Counts().Resolved() == 5 &&
                    judge.NextEvent() && judge.NextEvent()->beat == 19 &&
                    judge.Last() && !judge.Last()->wrongDirection,
                "the first event after its late window becomes Miss");
    judge.Advance(8.5);
    ok &= Check(judge.Counts().Resolved() == 5,
                "a missed event must not be counted twice");

    const auto nextPerfect = judge.Press(Turn::Right, 9.5);
    ok &= Check(Is(nextPerfect, Grade::Perfect, 19),
                "input after earlier misses still judges normally");

    // All remaining turns are missed if nothing is played.
    judge.Advance(16.0);
    ok &= Check(judge.Counts().perfect == 2 &&
                    judge.Counts().good == 1 &&
                    judge.Counts().bad == 1 &&
                    judge.Counts().miss == 5 &&
                    judge.Counts().Resolved() == 9 &&
                    judge.NextEvent() == nullptr,
                "all nine demo turns must resolve exactly once");
    ok &= Check(!judge.Press(Turn::Left, 15.8),
                "input after all turns cannot create a phantom score");

    // The authored route is immutable even when every turn is missed.
    const RouteStep before = SampleRouteStep(demo, 23);
    TurnJudge noFail(demo);
    noFail.Advance(16.0);
    const RouteStep after = SampleRouteStep(demo, 23);
    ok &= Check(SameRoute(before, after) &&
                    noFail.Counts().miss == static_cast<int>(demo.turns.size()),
                "No Fail: route must remain unchanged when all input is missing");

    // Explicit boundary-adjacent cases and resets.
    judge.Reset();
    ok &= Check(judge.Counts().Resolved() == 0 && !judge.Last() &&
                    judge.NextEvent() && judge.NextEvent()->beat == 2,
                "retry clears previous judgements");
    ok &= Check(!judge.Press(Turn::Right, first - 0.185),
                "too-early input outside window is ignored");
    ok &= Check(Is(judge.Press(Turn::Right, first - 0.155), Grade::Bad, 2),
                "negative error inside outer window scores Bad");
    judge.Reset();
    ok &= Check(Is(judge.Press(Turn::Right, first + 0.095), Grade::Good, 2),
                "positive error inside Good window");
    judge.Reset();
    ok &= Check(!judge.Press(Turn::Right, first + 0.185) &&
                    judge.Counts().miss == 1,
                "late press after window ends yields Miss, not a late hit");

    // External v1 charts use audio time at beat zero, not a second
    // frame-based timing domain. Both early and late offsets are legal.
    TurnJudge shifted(demo);
    shifted.SetOffsetSeconds(0.75);
    ok &= Check(!shifted.Press(Turn::Right, first) &&
                    Is(shifted.Press(Turn::Right, first + 0.75), Grade::Perfect, 2),
                "positive chart offset shifts the scheduled hit window");
    shifted.SetOffsetSeconds(-0.5);
    ok &= Check(Is(shifted.Press(Turn::Right, first - 0.5), Grade::Perfect, 2),
                "negative audio-time preroll preserves beat-zero judgements");
    shifted.FinishRemainingMisses();
    ok &= Check(shifted.Counts().Resolved() == static_cast<int>(demo.turns.size()),
                "song end resolves remaining chart events once");
    shifted.FinishRemainingMisses();
    ok &= Check(shifted.Counts().Resolved() == static_cast<int>(demo.turns.size()),
                "song completion must not double-count missing events");

    if (!ok) return 1;
    std::cout << "JUDGEMENT_PASS turns=9 perfect-good-bad-miss no-fail=true\n";
    return 0;
}
