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
    // Every resolved note contributes once. Miss = 0, and its denominator
    // still counts; otherwise missing notes would never reduce accuracy.
    GradeCounts accuracy;
    ok &= Check(accuracy.Resolved() == 0 &&
                std::abs(accuracy.AccuracyPercent() - 100.0) < 1e-9,
                "no resolved notes show neutral initial 100 percent");
    ++accuracy.perfect;
    ok &= Check(accuracy.AccuracyPercent() == 100.0,
                "Perfect contributes 100 percent");
    ++accuracy.good;
    ok &= Check(accuracy.AccuracyPercent() == 87.5,
                "Good contributes 75 points and averages with Perfect");
    ++accuracy.bad;
    ok &= Check(accuracy.AccuracyPercent() == 75.0,
                "Bad contributes 50 points");
    ++accuracy.miss;
    ok &= Check(accuracy.Resolved() == 4 &&
                accuracy.AccuracyPercent() == 56.25,
                "Miss contributes zero but reduces the 4-note accuracy");
    accuracy.miss += 2;
    ok &= Check(accuracy.Resolved() == 6 &&
                accuracy.AccuracyPercent() == 37.5,
                "additional misses continue lowering accuracy without false hits");
    GradeCounts allMiss;
    allMiss.miss = 3;
    ok &= Check(allMiss.AccuracyPercent() == 0.0,
                "chart with only misses has 0 percent accuracy");

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

    // T/Test from a selected beat should not penalize the skipped intro.
    TurnJudge fromMiddle(demo);
    fromMiddle.ResetFromBeat(8);
    ok &= Check(fromMiddle.Counts().Resolved() == 0 &&
                fromMiddle.NextEvent() &&
                fromMiddle.NextEvent()->beat == 8,
                "test practice starts at selected beat without earlier misses");
    ok &= Check(Is(fromMiddle.Press(Turn::Left, third), Grade::Perfect, 8) &&
                fromMiddle.Counts().perfect == 1 &&
                fromMiddle.Counts().miss == 0,
                "the event exactly at the test origin can still score");
    fromMiddle.ResetFromBeat(13);
    ok &= Check(fromMiddle.NextEvent() &&
                fromMiddle.NextEvent()->beat == 16 &&
                fromMiddle.Counts().Resolved() == 0,
                "fractional-beat test skips the turn already behind the cursor");
    fromMiddle.ResetFromBeat(999);
    ok &= Check(fromMiddle.NextEvent() == nullptr &&
                fromMiddle.Counts().Resolved() == 0,
                "test after final event has no phantom misses");

    // Last-note completion is event-driven, not a percentage of the
    // backing audio duration. A 20-second song with two events at
    // 0.5/1.5 s must be complete at 1.5 s after a successful last hit.
    Chart sparse;
    sparse.lengthBeats = 40; // 20 seconds at 120 BPM.
    sparse.turns = {{1, Turn::Left}, {3, Turn::Right}};
    TurnJudge lastNote(sparse);
    ok &= Check(lastNote.ScorableCount() == 2 &&
                !lastNote.AllScorableJudged(),
                "a chart cannot finish while any scorable note is pending");
    ok &= Check(Is(lastNote.Press(Turn::Left, 0.5), Grade::Perfect, 1) &&
                !lastNote.AllScorableJudged(),
                "judging the first note does not finish the chart");
    ok &= Check(Is(lastNote.Press(Turn::Right, 1.5), Grade::Perfect, 3) &&
                lastNote.AllScorableJudged() &&
                lastNote.Counts().Resolved() == 2 &&
                lastNote.NextEvent() == nullptr,
                "last successful note finishes chart before 20-second music end");
    lastNote.Advance(20.0);
    ok &= Check(lastNote.Counts().perfect == 2 &&
                lastNote.Counts().miss == 0,
                "post-completion time does not create phantom misses");

    lastNote.Reset();
    lastNote.Press(Turn::Left, 0.5);
    lastNote.Advance(1.68);
    ok &= Check(!lastNote.AllScorableJudged() &&
                lastNote.NextEvent() != nullptr,
                "last missed note stays eligible through exact Bad-window edge");
    lastNote.Advance(1.681);
    ok &= Check(lastNote.AllScorableJudged() &&
                lastNote.Counts().miss == 1 &&
                lastNote.Counts().perfect == 1 &&
                lastNote.Counts().AccuracyPercent() == 50.0,
                "last Miss ends chart only after the late judgement window");

    lastNote.ResetFromBeat(3);
    ok &= Check(lastNote.ScorableCount() == 1 &&
                lastNote.Counts().Resolved() == 0 &&
                !lastNote.AllScorableJudged(),
                "editor TEST excludes earlier events from accuracy denominator");
    lastNote.Press(Turn::Right, 1.5);
    ok &= Check(lastNote.AllScorableJudged() &&
                lastNote.Counts().perfect == 1 &&
                lastNote.Counts().AccuracyPercent() == 100.0,
                "mid-song test completes on the last eligible note");
    lastNote.ResetFromAudioSeconds(1.501);
    ok &= Check(lastNote.ScorableCount() == 0 &&
                !lastNote.AllScorableJudged(),
                "test starting after last event does not instantly auto-finish");
    Chart emptyChart;
    TurnJudge empty(emptyChart);
    ok &= Check(empty.ScorableCount() == 0 &&
                !empty.AllScorableJudged(),
                "chart without events uses audio-end completion fallback");

    Chart absoluteMs;
    absoluteMs.turns = {{2, Turn::Left, 1250}};
    TurnJudge millisecondJudge(absoluteMs);
    ok &= Check(!millisecondJudge.AllScorableJudged() &&
                Is(millisecondJudge.Press(Turn::Left, 1.25), Grade::Perfect, 2) &&
                millisecondJudge.AllScorableJudged(),
                "absolute millisecond last note is judged at exact stored time");

    Chart diagonalChart;
    diagonalChart.initialHeading = Heading::Right;
    diagonalChart.turns = {{2, Turn::Left45}, {4, Turn::Right45}};
    TurnJudge diagonalJudge(diagonalChart);
    const auto hitDiagonal = diagonalJudge.PressHeading(Heading::UpRight, 1.0);
    const auto hitCardinal = diagonalJudge.PressHeading(Heading::Right, 2.0);
    ok &= Check(hitDiagonal && hitDiagonal->grade == Grade::Perfect &&
                hitCardinal && hitCardinal->grade == Grade::Perfect &&
                diagonalJudge.Counts().perfect == 2,
                "diagonal and cardinal absolute outgoing headings score correctly");
    diagonalJudge.Reset();
    const auto wrongDiagonal = diagonalJudge.PressHeading(Heading::Up, 1.0);
    ok &= Check(wrongDiagonal && wrongDiagonal->wrongDirection &&
                diagonalJudge.Counts().miss == 1,
                "single arrow is wrong for an authored diagonal stroke");
    diagonalJudge.Reset();
    ok &= Check(!diagonalJudge.PressHeading(Heading::UpRight, 0.1) &&
                diagonalJudge.Counts().Resolved() == 0,
                "early chord does not consume a later diagonal note");

    if (!ok) return 1;
    std::cout << "JUDGEMENT_PASS turns=9 perfect-good-bad-miss no-fail=true\n";
    return 0;
}
