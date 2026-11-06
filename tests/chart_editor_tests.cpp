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
    ok &= Check(draft.SetSongDuration(24.0) &&
                draft.Data().route.lengthBeats == 51 &&
                draft.SelectBeat(50) && !draft.SelectBeat(51),
                "full song length automatically sets the editable timeline");
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
                    draft.SetSongDuration(10.0) &&
                    draft.Data().route.lengthBeats > 64,
                "song duration, not manual length, controls chart extent");
    draft.SelectBeat(63);
    draft.PutTurn(Turn::Left);
    const int protectedEnd = draft.Data().route.lengthBeats;
    ok &= Check(!draft.SetSongDuration(1.0) &&
                    draft.Data().route.lengthBeats == protectedEnd &&
                    draft.Data().route.turns.back().beat == 63,
                "shorter songs cannot silently strand authored turns");
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

    // Re-reading a just-saved chart must not behave like opening another
    // document: the selected beat and Undo/Redo session must survive.
    ChartEditor savedSession;
    savedSession.NewDraft();
    savedSession.SelectBeat(19);
    savedSession.PutTurn(Turn::Left);
    const auto onDisk = savedSession.Data();
    ok &= Check(savedSession.Dirty() && savedSession.CanUndo(),
                "newly-authored turn is undoable before save");
    savedSession.ApplySavedChart(onDisk);
    ok &= Check(savedSession.SelectedBeat() == 19 &&
                !savedSession.Dirty() && savedSession.CanUndo(),
                "saving does not rewind selected beat or clear undo history");
    ok &= Check(savedSession.Undo() && savedSession.Dirty() &&
                savedSession.Redo() && !savedSession.Dirty() &&
                savedSession.SelectedBeat() == 19,
                "undo and redo still work around saved revision");

    // Empty leading/trailing tick marks are not part of copied content.
    ChartEditor boundedCopy;
    boundedCopy.NewDraft();
    boundedCopy.SelectBeat(5);
    boundedCopy.PutTurn(Turn::Left);
    boundedCopy.SelectBeat(8);
    boundedCopy.PutTurn(Turn::Right);
    const auto clipped = boundedCopy.CopyTurns(0, 11);
    ok &= Check(clipped.span == 4 && clipped.notes.size() == 2 &&
                clipped.notes[0].beat == 0 && clipped.notes[1].beat == 3,
                "clipboard starts at first turn and keeps only internal rests");
    ok &= Check(boundedCopy.PasteTurns(15, clipped) &&
                boundedCopy.Data().route.turns[2].beat == 15 &&
                boundedCopy.Data().route.turns[3].beat == 18,
                "paste aligns first note exactly to selected destination");
    const auto noNotes = boundedCopy.CopyTurns(10, 11);
    ok &= Check(noNotes.span == 0 && noNotes.notes.empty(),
                "empty selection copies no leading padding or phantom pattern");

    // A range-based pattern copies relative notes, including gaps, rather than
    // copying the entire chart. All group edits must be a single undo step.
    ChartEditor group;
    group.NewDraft();
    group.MarkSaved();
    group.SelectBeat(2);
    group.PutTurn(Turn::Left);
    group.SelectBeat(4);
    group.PutTurn(Turn::Right);
    group.SelectBeat(8);
    group.PutTurn(Turn::Left);
    const auto pattern = group.CopyTurns(2, 5);
    ok &= Check(pattern.span == 3 && pattern.notes.size() == 2 &&
                pattern.notes[0].beat == 0 && pattern.notes[1].beat == 2,
                "pattern copy stores source span and relative note positions");
    ok &= Check(!group.PasteTurns(30, pattern) &&
                group.Data().route.turns.size() == 3,
                "out-of-range paste is atomic");
    ok &= Check(group.PasteTurns(12, pattern) &&
                group.Data().route.turns.size() == 5 &&
                group.Data().route.turns[3].beat == 12 &&
                group.Data().route.turns[4].beat == 14,
                "pattern paste preserves rests and sorted notes");
    ok &= Check(group.InvertTurns(12, 14) &&
                group.Data().route.turns[3].direction == Turn::Right &&
                group.Data().route.turns[4].direction == Turn::Left,
                "flip changes Left/Right for the entire selected group");
    ok &= Check(group.Undo() &&
                group.Data().route.turns[3].direction == Turn::Left &&
                group.Data().route.turns[4].direction == Turn::Right &&
                group.Redo() &&
                group.Data().route.turns[3].direction == Turn::Right,
                "undo and redo invert the whole group together");
    ok &= Check(group.ShiftTurns(12, 14, 1) &&
                group.Data().route.turns[3].beat == 13 &&
                group.Data().route.turns[4].beat == 15,
                "group shift moves all notes by one beat");
    group.SelectBeat(16);
    group.PutTurn(Turn::Left);
    ok &= Check(!group.ShiftTurns(13, 15, 1) &&
                group.Data().route.turns[4].beat == 15 &&
                group.Data().route.turns[5].beat == 16,
                "collision with notes outside selection rejects entire shift");
    ok &= Check(group.DeleteTurns(13, 15) &&
                group.Data().route.turns.size() == 4 &&
                group.Undo() && group.Data().route.turns.size() == 6,
                "delete range is reversible in one undo operation");
    ok &= Check(group.Undo() && group.Data().route.turns.size() == 5 &&
                group.Undo() && group.Data().route.turns[3].beat == 12 &&
                group.Data().route.turns[4].beat == 14,
                "multiple undo steps restore earlier pattern positions");
    ok &= Check(group.SetGridSize(8, 8) && group.Undo() &&
                group.Data().route.gridWidth == 4 &&
                group.Redo() && group.Data().route.gridWidth == 8,
                "undo and redo also cover ordinary field edits");
    ChartEditor saved;
    saved.NewDraft();
    saved.PutTurn(Turn::Left);
    saved.MarkSaved();
    saved.PutTurn(Turn::Right);
    ok &= Check(saved.Dirty() && saved.Undo() && !saved.Dirty() &&
                saved.Redo() && saved.Dirty(),
                "undo to last saved revision restores clean status");

    ChartEditor autoTimeline;
    ok &= Check(autoTimeline.SetSongDuration(10.0) &&
                autoTimeline.Data().route.firstBeat == 0 &&
                autoTimeline.Data().route.lengthBeats == 20,
                "audio duration automatically sets 20 beats at 120 BPM");
    ok &= Check(autoTimeline.SetOffsetMs(2500.0) &&
                autoTimeline.Data().route.firstBeat == -5 &&
                autoTimeline.Data().route.lengthBeats == 15 &&
                autoTimeline.SelectBeat(-3),
                "positive offset exposes selectable negative beats in song");
    autoTimeline.PutTurn(Turn::Left);
    ok &= Check(autoTimeline.CopyTurns(-5, -2).notes.size() == 1 &&
                autoTimeline.Data().route.turns[0].beat == -3,
                "pattern editing supports pre-offset negative beat turn");
    ok &= Check(autoTimeline.SetBpm(240.0) &&
                autoTimeline.Data().route.firstBeat == -10 &&
                autoTimeline.Data().route.lengthBeats == 30 &&
                autoTimeline.Undo() &&
                autoTimeline.Data().route.firstBeat == -5 &&
                autoTimeline.Data().route.lengthBeats == 15,
                "BPM reflows full song and Undo restores calculated boundaries");

    ChartEditor compass;
    compass.NewDraft();
    ok &= Check(compass.SetOutgoingHeading(Heading::UpRight) &&
                compass.SelectedTurn() &&
                compass.SelectedTurn()->direction == Turn::Left45 &&
                SampleRouteStep(compass.Data().route, 0).heading == Heading::UpRight,
                "ROUTE compass authors diagonal direction as relative 45 turn");
    ok &= Check(!compass.SetOutgoingHeading(Heading::Left) &&
                compass.SelectedTurn()->direction == Turn::Left45,
                "opposite outgoing direction cannot be authored in one beat");
    compass.SelectBeat(1);
    ok &= Check(compass.SetOutgoingHeading(Heading::Down) &&
                compass.SelectedTurn()->direction == Turn::Right135 &&
                compass.InvertTurns(0, 1) &&
                compass.Data().route.turns[0].direction == Turn::Right45 &&
                compass.Data().route.turns[1].direction == Turn::Left135,
                "pattern inversion preserves 45/135 degree turn magnitudes");
    ok &= Check(compass.Undo() &&
                compass.Data().route.turns[0].direction == Turn::Left45 &&
                compass.Redo() &&
                compass.Data().route.turns[0].direction == Turn::Right45,
                "diagonal inversion is one undo/redo transaction");
    compass.RotateHeading45(1);
    ok &= Check(compass.Data().route.initialHeading == Heading::DownRight &&
                compass.SetOutgoingHeading(Heading::Down) &&
                SampleRouteStep(compass.Data().route, 1).heading == Heading::Down,
                "editor can set a diagonal initial heading in 45 degree increments");

    // A 250 BPM chart can gain triplets AFTER authoring without moving
    // existing notes in song time or modifying the musical BPM.
    ChartEditor subdivisions;
    subdivisions.NewDraft();
    ok &= Check(subdivisions.SetBpm(250) && subdivisions.SetOffsetMs(500) &&
                subdivisions.SetSongDuration(10.0) &&
                subdivisions.SelectBeat(5),
                "setup 250 BPM song with integer millisecond offset");
    subdivisions.PutTurn(Turn::Left);
    ok &= Check(subdivisions.Data().route.turns[0].timeMs == 1700,
                "newly placed note stores absolute timeMs, not beat");
    ok &= Check(subdivisions.SetSnapDivision(2) &&
                subdivisions.Data().route.ticksPerBeat == 2 &&
                subdivisions.Data().route.turns[0].beat == 10 &&
                subdivisions.Data().route.turns[0].timeMs == 1700,
                "half-beat grid preserves time of original note");
    ok &= Check(subdivisions.SetSnapDivision(3) &&
                subdivisions.Data().route.bpm == 250 &&
                subdivisions.Data().route.ticksPerBeat == 6 &&
                subdivisions.Data().route.turns[0].beat == 30 &&
                subdivisions.Data().route.turns[0].timeMs == 1700,
                "triplet grid expands LCM ticks but keeps BPM and note time");
    ok &= Check(subdivisions.StepSnapSelection(1) &&
                subdivisions.SelectedBeat() == 32,
                "right-arrow moves to next 1/3 musical-beat grid line");
    subdivisions.PutTurn(Turn::Right);
    ok &= Check(subdivisions.Data().route.turns.size() == 2 &&
                subdivisions.Data().route.turns[1].timeMs == 1780,
                "new triplet note falls exactly 80 ms after existing note at 250 BPM");
    ok &= Check(subdivisions.SetSnapDivision(1) &&
                subdivisions.Data().route.turns[0].timeMs == 1700 &&
                subdivisions.Data().route.turns[1].timeMs == 1780,
                "reducing grid density does not delete off-grid notes");
    ok &= Check(subdivisions.SetBpm(300) &&
                subdivisions.Data().route.bpm == 300 &&
                subdivisions.Data().route.turns[0].timeMs == 1700 &&
                subdivisions.Data().route.turns[1].timeMs == 1780,
                "changing BPM preserves absolute note timestamps");
    ok &= Check(subdivisions.SetOffsetMs(750) &&
                subdivisions.Data().route.turns[0].timeMs == 1700 &&
                subdivisions.Data().route.turns[1].timeMs == 1780 &&
                !subdivisions.SetOffsetMs(750.5),
                "offset changes preserve absolute notes and disallow fractional ms");
    ok &= Check(subdivisions.Undo() &&
                subdivisions.Data().route.turns[0].timeMs == 1700 &&
                subdivisions.Redo() &&
                subdivisions.Data().route.turns[1].timeMs == 1780,
                "undo/redo preserves millisecond timestamps through timing changes");

    // Even a timestamp slightly between grid ticks must be scored at its
    // exact integer ms instead of the nearest visual snap position.
    Chart preciseChart;
    preciseChart.bpm = 250;
    preciseChart.ticksPerBeat = 6;
    preciseChart.turns.push_back({2, Turn::Left, 583});
    TurnJudge exactJudge(preciseChart);
    const auto exactHit = exactJudge.Press(Turn::Left, 0.583);
    ok &= Check(exactHit && exactHit->grade == Grade::Perfect &&
                std::abs(exactHit->timingErrorSeconds) < 1e-12,
                "scoring uses exact stored 583 ms, not 580 ms grid projection");

    if (!ok) return 1;
    std::cout << "CHART_EDITOR_PASS bpm=1 offset_ms=1 turns=1 wrap=1\n";
    return 0;
}
