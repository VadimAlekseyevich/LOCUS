#include "core/chart_difficulty.h"
#include "core/chart_editor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using namespace locus::core;

namespace {
bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "DIFFICULTY_FAIL " << label << '\n';
    return condition;
}
Chart Sequence(std::size_t n, const std::vector<int>& gaps,
               const std::vector<Heading>& headings) {
    Chart chart;
    chart.bpm = 120.0;
    chart.ticksPerBeat = 24;
    chart.initialHeading = Heading::Right;
    int ms = 5000;
    for (std::size_t i = 0; i < n; ++i) {
        if (i) ms += gaps[(i-1) % gaps.size()];
        chart.turns.push_back({
            TickAtTimeMs(chart.bpm, chart.ticksPerBeat, 0, ms),
            headings[i % headings.size()], ms
        });
    }
    chart.lengthBeats = chart.turns.empty() ? 48 : chart.turns.back().beat + 48;
    return chart;
}
bool Finite(const ChartDifficulty& d) {
    for (double value : {d.overall,d.speed,d.rhythm,d.coordination,d.reading,
                         d.stamina,d.peak,d.notesPerSecondPeak,
                         d.keysPerSecondPeak,d.activeSeconds})
        if (!std::isfinite(value) || value < 0.0) return false;
    for (double value : {d.overall,d.speed,d.rhythm,d.coordination,d.reading,
                         d.stamina,d.peak})
        if (value > 10.0) return false;
    return true;
}
bool SameDimensions(const ChartDifficulty& a, const ChartDifficulty& b) {
    for (const auto diff : {a.overall-b.overall, a.speed-b.speed,
                           a.rhythm-b.rhythm, a.coordination-b.coordination,
                           a.reading-b.reading, a.stamina-b.stamina,
                           a.peak-b.peak})
        if (std::abs(diff) > 1e-8) return false;
    return true;
}
}
int main() {
    bool ok = true;
    Chart empty;
    const auto blank = CalculateChartDifficulty(empty);
    ok &= Check(blank.overall == 0 && blank.notes == 0 &&
                blank.keyPresses == 0, "empty chart has zero difficulty");

    const auto easyChart = Sequence(45, {500}, {Heading::Right});
    const auto fastChart = Sequence(45, {100}, {Heading::Right});
    const auto easy = CalculateChartDifficulty(easyChart);
    const auto fast = CalculateChartDifficulty(fastChart);
    ok &= Check(Finite(easy) && Finite(fast) && fast.speed > easy.speed &&
                fast.overall > easy.overall && fast.notes == 45 &&
                fast.keyPresses == 45,
                "more notes per second raises speed and rating, not BPM alone");

    const auto diagonal = CalculateChartDifficulty(
        Sequence(45, {100}, {Heading::UpRight}));
    ok &= Check(diagonal.keyPresses == 90 &&
                diagonal.coordination > fast.coordination &&
                diagonal.keysPerSecondPeak > fast.keysPerSecondPeak &&
                diagonal.overall > fast.overall,
                "diagonal is one note but two physical keys and greater strain");

    const auto steady = CalculateChartDifficulty(
        Sequence(60, {220}, {Heading::Right}));
    const auto swing = CalculateChartDifficulty(
        Sequence(60, {130,310}, {Heading::Right}));
    ok &= Check(swing.rhythm > steady.rhythm &&
                swing.notes == steady.notes,
                "irregular intervals increase rhythm complexity at equal average tempo");

    const auto loops = CalculateChartDifficulty(
        Sequence(60, {220}, {Heading::Right,Heading::Left}));
    ok &= Check(loops.reading > steady.reading &&
                loops.overall > steady.overall,
                "reversals and overlapping route segments increase reading strain");

    const auto shortStream = CalculateChartDifficulty(
        Sequence(18, {190}, {Heading::Right,Heading::Down}));
    const auto longStream = CalculateChartDifficulty(
        Sequence(180, {190}, {Heading::Right,Heading::Down}));
    ok &= Check(longStream.stamina > shortStream.stamina &&
                longStream.activeSeconds > shortStream.activeSeconds,
                "long sustained patterns require more stamina");

    Chart pauseChart = Sequence(3, {30000}, {Heading::Right});
    const auto pauses = CalculateChartDifficulty(pauseChart);
    ok &= Check(pauses.activeSeconds <= 3.01 && pauses.stamina < 0.4,
                "silent song gaps do not inflate stamina");

    Chart transformed = Sequence(
        50, {160,260,180}, {Heading::Right,Heading::Up,
                            Heading::DownLeft,Heading::UpRight});
    const auto reference = CalculateChartDifficulty(transformed);
    Chart reflected = transformed;
    reflected.initialHeading = MirrorHeadingHorizontal(reflected.initialHeading);
    for (auto& e : reflected.turns)
        e.direction = MirrorHeadingHorizontal(e.direction);
    const auto mirror = CalculateChartDifficulty(reflected);
    ok &= Check(SameDimensions(reference, mirror),
                "left-right reflection does not change intrinsic chart difficulty");

    Chart rotated = transformed;
    rotated.initialHeading = RotateHeading(rotated.initialHeading, 2);
    for (auto& e : rotated.turns)
        e.direction = RotateHeading(e.direction, 2);
    ok &= Check(SameDimensions(reference, CalculateChartDifficulty(rotated)),
                "90-degree rotation has the same difficulty");

    Chart reticked = transformed;
    reticked.ticksPerBeat *= 2;
    for (auto& e : reticked.turns) e.beat *= 2;
    reticked.lengthBeats *= 2;
    ok &= Check(SameDimensions(reference, CalculateChartDifficulty(reticked)),
                "increasing snap resolution alone cannot change difficulty");

    Chart shifted = transformed;
    for (auto& e : shifted.turns) e.timeMs += 10000;
    ok &= Check(SameDimensions(reference,
                CalculateChartDifficulty(shifted, 10.0)),
                "shifting all timestamps and offset together is rating-invariant");

    Chart single = Sequence(1, {200}, {Heading::Up});
    const auto lone = CalculateChartDifficulty(single);
    ok &= Check(Finite(lone) && lone.notes == 1 && lone.overall < 2.0,
                "one isolated note is not classified as an extreme chart");

    Chart invalid = transformed;
    invalid.bpm = 0.0;
    ok &= Check(CalculateChartDifficulty(invalid).notes == 0,
                "invalid tempo never produces NaN or undefined rating");

    // Editor exposes revision-cached difficulty without altering dirty state.
    ChartEditor editor;
    const auto before = editor.Difficulty();
    editor.SelectBeat(3);
    editor.PutTurn(Heading::UpRight);
    const auto changed = editor.Difficulty();
    ok &= Check(changed.notes == 1 && before.notes == 0 &&
                editor.Difficulty().notes == 1,
                "editor rating refreshes after modifying notes");
    ok &= Check(editor.Undo() && editor.Difficulty().notes == 0 &&
                editor.Redo() && editor.Difficulty().notes == 1,
                "editor rating refreshes after undo/redo");
    editor.NewDraft();
    ok &= Check(editor.Difficulty().notes == 0,
                "new blank editor does not reuse previous cached score");

    if (!ok) return 1;
    std::cout << "DIFFICULTY_V1_PASS intrinsic independent chart rating\n";
    return 0;
}
