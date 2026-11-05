#include "core/chart_editor.h"
#include "core/route.h"

#include <cmath>
#include <iostream>

using namespace locus::core;

namespace {

bool Check(bool ok, const char* name) {
    if (!ok) std::cerr << "CHART_EDITOR_FAIL " << name << '\n';
    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ChartEditor draft;
    ok &= Check(draft.Data().route.lengthBeats == 32 &&
                draft.Data().route.turns.empty() && !draft.Dirty() &&
                draft.Data().offsetSeconds == 0.0, "fresh draft is editable");

    ok &= Check(draft.ChangeBpm(5) && draft.Data().route.bpm == 125.0 &&
                draft.ChangeOffsetMs(150) &&
                std::abs(draft.Data().offsetSeconds - 0.15) < 1e-9,
                "BPM and milliseconds are editable");
    ok &= Check(draft.ChangeOffsetMs(-180) &&
                std::abs(draft.Data().offsetSeconds + 0.03) < 1e-9,
                "negative millisecond preroll");
    ok &= Check(draft.SetStart({3, 0}) && !draft.SetStart({4, 0}),
                "start-cell bounds");
    draft.RotateHeading(-1);
    ok &= Check(draft.Data().route.initialHeading == Heading::Up,
                "left heading rotation");

    draft.SelectBeat(5);
    draft.PutTurn(Turn::Right);
    draft.SelectBeat(2);
    draft.PutTurn(Turn::Left);
    ok &= Check(draft.Data().route.turns.size() == 2 &&
                draft.Data().route.turns[0].beat == 2 &&
                draft.Data().route.turns[1].beat == 5,
                "events inserted in beat order");
    draft.PutTurn(Turn::Right);
    ok &= Check(draft.Data().route.turns.size() == 2 &&
                draft.Data().route.turns[0].direction == Turn::Right,
                "same-beat turn replaces rather than duplicates");
    ok &= Check(!draft.MoveTurn(3), "moving onto an occupied beat is blocked");
    ok &= Check(draft.MoveTurn(1) && draft.SelectedBeat() == 3 &&
                draft.Data().route.turns[0].beat == 3,
                "move the selected turn one beat");
    ok &= Check(draft.RemoveTurn() && draft.Data().route.turns.size() == 1 &&
                !draft.RemoveTurn(), "remove selected event exactly once");
    ok &= Check(!draft.ChangeLength(-28) &&
                draft.Data().route.lengthBeats == 32,
                "shortening must not silently discard later turns");
    ok &= Check(draft.ChangeLength(16) &&
                draft.Data().route.lengthBeats == 48 &&
                draft.SelectBeat(47) && !draft.SelectBeat(48),
                "timeline can extend beyond first page and clamps selection");
    const auto routeBefore = SampleRouteStep(draft.Data().route, 5);
    draft.SelectBeat(5);
    draft.PutTurn(Turn::Left);
    const auto routeAfter = SampleRouteStep(draft.Data().route, 5);
    ok &= Check(routeBefore.heading != routeAfter.heading &&
                draft.Dirty(), "preview route recalculates from edited events");

    LoadedChart loaded;
    loaded.route = MakeDemoChart();
    loaded.title = "Loaded Song";
    loaded.offsetSeconds = 0.125;
    draft.Load(loaded);
    ok &= Check(!draft.Dirty() && draft.SelectedBeat() == 0 &&
                draft.Data().route.turns.size() == 9 &&
                std::abs(draft.Data().offsetSeconds * 1000.0 - 125.0) < 1e-9,
                "load an existing chart without changing its timing");
    ok &= Check(draft.ChangeOffsetMs(10) &&
                std::abs(draft.Data().offsetSeconds - 0.135) < 1e-9,
                "loaded seconds edited as millisecond increments");
    draft.NewDraft();
    ok &= Check(!draft.Dirty() && draft.Data().route.turns.empty() &&
                draft.Data().offsetSeconds == 0.0,
                "new draft resets controls without hidden chart events");

    if (!ok) return 1;
    std::cout << "CHART_EDITOR_PASS bpm=1 offset_ms=1 turns=1 wrap=1\n";
    return 0;
}
