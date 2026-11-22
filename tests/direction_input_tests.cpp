#include "app/direction_input_events.h"
#include "core/judgement.h"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
using namespace locus;

struct Transition {
    int key;
    bool down;
    double at;
    bool repeat = false;
};

// Two notes inside a single 60 Hz interval, including a re-press of the
// SAME key. The diagonal's second physical component is seven ms later.
const std::vector<Transition> recorded{
    {74, true, 1.000},
    {75, true, 1.007},
    {74, true, 1.008, true},
    {74, false, 1.010},
    {74, true, 1.014},
    {75, false, 1.018},
    {74, false, 1.021}
};

bool Check(bool ok, const char* what) {
    if (!ok) std::cerr << "DIRECTION_INPUT_FAIL " << what << '\n';
    return ok;
}

std::vector<app::DirectionKeyEvent> Replay(double fps, bool stall = false) {
    app::DirectionInputEvents input;
    input.Configure({74, 75, 70, 68});
    std::vector<app::DirectionKeyEvent> output;
    std::size_t next = 0;
    for (std::size_t frame = 0; next < recorded.size(); ++frame) {
        double boundary = (frame + 1) / fps;
        if (stall && boundary < 1.20) continue;
        while (next < recorded.size() && recorded[next].at <= boundary) {
            const auto& e = recorded[next++];
            input.Observe(e.key, e.down, e.at, e.repeat);
        }
        const auto batch = input.Drain();
        output.insert(output.end(), batch.events.begin(),
                      batch.events.begin() + batch.size);
    }
    return output;
}

bool Identical(const std::vector<app::DirectionKeyEvent>& a,
               const std::vector<app::DirectionKeyEvent>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].arrow != b[i].arrow || a[i].pressed != b[i].pressed ||
            a[i].capturedAt != b[i].capturedAt || a[i].order != b[i].order)
            return false;
    }
    return true;
}

struct Verdict {
    std::vector<core::Judgement> notes;
    core::GradeCounts totals;
};

Verdict Judge(const std::vector<app::DirectionKeyEvent>& events) {
    core::Chart chart;
    chart.turns = {{2, core::Heading::UpRight, 1000},
                   {3, core::Heading::Up, 1015}};
    core::TurnJudge judge(chart);
    Verdict result;
    for (const auto& e : events) {
        if (!e.pressed) continue;
        if (const auto graded = judge.PressHeading(e.arrow, e.capturedAt))
            result.notes.push_back(*graded);
    }
    judge.FinishRemainingMisses();
    result.totals = judge.Counts();
    return result;
}

int main() {
    bool ok = true;
    const auto baseline = Replay(240);
    ok &= Check(baseline.size() == 6,
                "OS auto-repeat filtered; press-release-press retained");
    ok &= Check(baseline.size() >= 4 &&
                baseline[0].arrow == core::Heading::Up &&
                baseline[1].arrow == core::Heading::Right &&
                std::abs(baseline[1].capturedAt -
                         baseline[0].capturedAt - 0.007) < 1e-10 &&
                !baseline[2].pressed && baseline[3].pressed,
                "7ms diagonal components and repeat of the first key");
    const auto firstVerdict = Judge(baseline);
    ok &= Check(firstVerdict.notes.size() == 3 &&
                firstVerdict.totals.perfect == 3 &&
                firstVerdict.totals.Resolved() == 3 &&
                std::abs(firstVerdict.notes[1].timingErrorSeconds - 0.007) < 1e-10 &&
                std::abs(firstVerdict.notes[2].timingErrorSeconds + 0.001) < 1e-10,
                "one perfect grade per physical press and signed errors preserved");
    for (double fps : {30.0, 60.0, 144.0, 240.0}) {
        for (bool stall : {false, true}) {
            const auto replay = Replay(fps, stall);
            const auto verdict = Judge(replay);
            bool equal = Identical(baseline, replay) &&
                         verdict.totals.perfect == firstVerdict.totals.perfect &&
                         verdict.totals.Resolved() == firstVerdict.totals.Resolved() &&
                         verdict.notes.size() == firstVerdict.notes.size();
            if (equal) {
                for (std::size_t i = 0; i < verdict.notes.size(); ++i)
                    equal &= verdict.notes[i].grade == firstVerdict.notes[i].grade &&
                             verdict.notes[i].timingErrorSeconds ==
                                 firstVerdict.notes[i].timingErrorSeconds;
            }
            ok &= Check(equal, "same ordered event stream, grades and errors at any FPS");
        }
    }
    app::DirectionInputEvents input;
    input.Configure({74, 75, 70, 68});
    input.Observe(74, true, 2.0);
    input.DiscardPending();
    ok &= Check(!input.Observe(74, true, 2.1) &&
                input.Drain().Empty() && input.Held()[0],
                "pause drops pending edges but does not rearm a held key");
    input.Observe(74, false, 2.15);
    input.Observe(74, true, 2.2);
    ok &= Check(input.Drain().size == 2, "new press follows actual release");
    input.Reset();
    ok &= Check(input.Drain().Empty() && !input.Held()[0],
                "focus loss clears held states and pending edges");
    return ok ? 0 : 1;
}
