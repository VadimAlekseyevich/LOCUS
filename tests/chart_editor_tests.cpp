#include "core/chart_editor.h"
#include "core/route.h"

#include <cmath>
#include <iostream>
#include <limits>

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
                "default start-cell bounds");
    ok &= Check(draft.SetGridSize(8, 8) && draft.SetStart({7, 7}) &&
                draft.Data().route.gridWidth == 8 &&
                draft.Data().route.gridHeight == 8,
                "8/8 game field expands selectable start cells");
    ok &= Check(!draft.SetGridSize(4, 4) &&
                draft.Data().route.gridWidth == 8 &&
                draft.Data().route.start.x == 7 &&
                !draft.SetGridSize(17, 8),
                "resizing never silently discards an out-of-bounds start cell");
    ok &= Check(draft.SetStart({1, 1}) && draft.SetGridSize(4, 4),
                "shrinking succeeds after moving the start inside the field");
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

    // Direct text-field setters: no repeated +/- clicking or drifting offsets.
    ok &= Check(draft.SetBpm(137.5) && draft.Data().route.bpm == 137.5,
                "typed decimal BPM replaces the previous value");
    ok &= Check(draft.SetBpm(400.0) && draft.Data().route.bpm == 400.0 &&
                draft.ChangeBpm(1) && draft.Data().route.bpm == 401.0 &&
                draft.SetBpm(1200.5) && draft.Data().route.bpm == 1200.5,
                "400 BPM and higher values are accepted without an arbitrary 300 cap");
    ok &= Check(!draft.SetBpm(0.0) && !draft.SetBpm(-20.0) &&
                !draft.SetBpm(std::numeric_limits<double>::infinity()) &&
                !draft.ChangeBpm(-2000),
                "nonpositive and nonfinite BPM values remain invalid");
    ok &= Check(draft.SetOffsetMs(-175.25) &&
                    std::abs(draft.Data().offsetSeconds + 0.17525) < 1e-10,
                "typed millisecond offset can be fractional and negative");
    ok &= Check(!draft.SetOffsetMs(4000000.0) &&
                    draft.SetLength(64) &&
                    draft.Data().route.lengthBeats == 64,
                "typed beat count is validated");
    draft.SelectBeat(63);
    draft.PutTurn(Turn::Left);
    ok &= Check(!draft.SetLength(20) &&
                    draft.Data().route.lengthBeats == 64,
                "typed length cannot silently erase a distant turn");
    ok &= Check(draft.SetTitle("Typed title") &&
                    draft.Data().title == "Typed title" &&
                    !draft.SetTitle("   "),
                "typed title is stored and empty titles rejected");

    // Ctrl+arrow navigation jumps four beats (one 4/4 bar), with edge
    // clamping and no accidental modification of the saved chart.
    draft.NewDraft();
    ok &= Check(draft.StepSelection(4) && draft.SelectedBeat() == 4 &&
                draft.StepSelection(4) && draft.SelectedBeat() == 8 &&
                draft.StepSelection(-4) && draft.SelectedBeat() == 4,
                "Ctrl+arrows move the selection by four beats");
    ok &= Check(draft.StepSelection(-100) && draft.SelectedBeat() == 0 &&
                !draft.StepSelection(-4) && draft.StepSelection(100) &&
                draft.SelectedBeat() == 31 && !draft.StepSelection(4) &&
                !draft.Dirty(), "bar navigation clamps at both edges and stays non-destructive");

    // 173765 is an offset in milliseconds, not an excessively large BPM.
    // Space on beat 16 should seek to the correct *song* time.
    draft.NewDraft();
    ok &= Check(draft.SetBpm(120.0) && draft.SetOffsetMs(173765.0),
                "173765 ms is a valid offset");
    draft.SelectBeat(16);
    ok &= Check(std::abs(draft.AudioSecondsAtBeat(0) - 173.765) < 1e-9 &&
                std::abs(draft.AudioSecondsAtBeat(16) - 181.765) < 1e-9,
                "selected beat maps to the correct audio timestamp");
    ok &= Check(draft.SetStart({0, 3}) &&
                SampleRouteStep(draft.Data().route, 0).from.x == 0 &&
                SampleRouteStep(draft.Data().route, 0).from.y == 3,
                "new starting cell is used by actual route geometry");

    if (!ok) return 1;
    std::cout << "CHART_EDITOR_PASS bpm=1 offset_ms=1 turns=1 wrap=1\n";
    return 0;
}
