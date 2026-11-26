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
    for (double value : {d.overall,d.speed,d.fingerStrain,d.rhythm,d.coordination,d.reading,
                         d.stamina,d.peak,d.notesPerSecondPeak,
                         d.keysPerSecondPeak,d.activeSeconds})
        if (!std::isfinite(value) || value < 0.0) return false;
    return true;
}
bool SameDimensions(const ChartDifficulty& a, const ChartDifficulty& b) {
    for (const auto diff : {a.overall-b.overall, a.speed-b.speed, a.fingerStrain-b.fingerStrain,
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

    const auto stream = Sequence(128,{100},{Heading::Right,Heading::Up});
    const auto normal = CalculateChartDifficulty(stream);
    const auto half = CalculateChartDifficulty(stream,0,nullptr,0,0.75);
    const auto modFast = CalculateChartDifficulty(stream,0,nullptr,0,1.25);
    ok &= Check(half.overall < normal.overall && normal.overall < modFast.overall &&
                half.keyPresses == modFast.keyPresses && half.notes == modFast.notes &&
                half.activeSeconds > modFast.activeSeconds,
                "rate mods recalculate physical strain on real elapsed time");
    ok &= Check(SameDimensions(normal,CalculateChartDifficulty(stream,0,nullptr,0,1.0)) &&
                CalculateChartDifficulty(stream,0,nullptr,0,0).notes == 0,
                "default is native difficulty; invalid rate rejected");
    const auto equivalent = Sequence(128,{80},{Heading::Right,Heading::Up});
    ok &= Check(SameDimensions(modFast,CalculateChartDifficulty(equivalent)),
                "125 percent is equivalent to actually shortening every gap by 20 percent");

    auto modShifted = stream;
    for (auto& note : modShifted.turns) note.timeMs += 123456;
    ok &= Check(SameDimensions(CalculateChartDifficulty(stream,0,nullptr,0,1.01),
                               CalculateChartDifficulty(modShifted,0,nullptr,0,1.01)),
                "fractional rate is invariant under a shift of exact timestamps");

    DifficultyTimeline graph;
    const auto timelineChart=Sequence(40,{100},{Heading::Right,Heading::Up});
    const auto withGraph=CalculateChartDifficulty(timelineChart,0,&graph,20);
    ok &= Check(SameDimensions(withGraph,CalculateChartDifficulty(timelineChart)) &&
                graph.maximum>0 && graph.peaks.front()==0 && graph.peaks.back()==0 &&
                *std::max_element(graph.peaks.begin(),graph.peaks.end())==graph.maximum,
                "timeline preserves rating and song-time silence");
    CalculateChartDifficulty(empty,0,&graph,20);
    ok &= Check(graph.maximum==0,"empty chart clears timeline");

    // Imported library difficulties have not yet decoded their song file.
    // Their chart length stays at its default until play activation.
    Chart unprepared = Sequence(60, {200}, {Heading::Right, Heading::Up});
    unprepared.lengthBeats = 32;
    const auto indexed = CalculateChartDifficulty(unprepared);
    ok &= Check(indexed.notes == unprepared.turns.size() &&
                indexed.overall > 0.0 && indexed.reading > 0.0,
                "valid imported chart is rated before audio duration is known");

    // Imported multi-minute charts have no decoded duration. The parser
    // deliberately leaves lengthBeats=1. Reproduce the sizes, tempos and
    // offsets of all four Eviternity-package difficulties.
    struct ImportedProfile {
        int ticks;
        int count;
        int firstMs;
        int spacingMs;
    };
    for (const ImportedProfile p : {
            ImportedProfile{12, 228, 19413, 240},
            ImportedProfile{24, 127, 165333, 120},
            ImportedProfile{24, 79, 149973, 240},
            ImportedProfile{12, 271, 19413, 240}}) {
        Chart imported;
        imported.bpm = 250.0;
        imported.ticksPerBeat = p.ticks;
        imported.lengthBeats = 1; // ParseChartJson before audio loading.
        imported.start = {0, 0};
        for (int i = 0; i < p.count; ++i) {
            const auto time = static_cast<std::int64_t>(p.firstMs) +
                              static_cast<std::int64_t>(i) * p.spacingMs;
            imported.turns.push_back({
                TickAtTimeMs(imported.bpm, imported.ticksPerBeat, 1.173, time),
                (i % 2 == 0) ? Heading::UpRight : Heading::Left, time});
        }
        const auto actual = CalculateChartDifficulty(imported, 1.173);
        ok &= Check(actual.notes == imported.turns.size() &&
                    actual.overall > 0.0 && actual.speed > 0.0 &&
                    actual.reading > 0.0,
                    "dense imported chart has nonzero difficulty before audio");
    }

    const auto easyChart = Sequence(45, {500}, {Heading::Right});
    const auto fastChart = Sequence(45, {100}, {Heading::Right});
    const auto easy = CalculateChartDifficulty(easyChart);
    const auto fast = CalculateChartDifficulty(fastChart);
    ok &= Check(Finite(easy) && Finite(fast) && fast.speed > easy.speed &&
                fast.overall > easy.overall && fast.notes == 45 &&
                fast.keyPresses == 45,
                "more notes per second raises speed and rating, not BPM alone");

    const auto denser = CalculateChartDifficulty(
        Sequence(180, {55}, {Heading::Right}));
    const auto extreme = CalculateChartDifficulty(
        Sequence(180, {35}, {Heading::Right}));
    ok &= Check(Finite(extreme) && denser.speed > fast.speed &&
                extreme.speed > denser.speed &&
                extreme.overall > denser.overall &&
                extreme.overall > 10.0 && extreme.speed > 10.0,
                "difficulty grows past ten without saturating on dense streams");

    const auto sustained = CalculateChartDifficulty(
        Sequence(180, {100}, {Heading::Right}));
    const auto fragmented = CalculateChartDifficulty(
        Sequence(180, {100,100,100,100,100,100,100,100,100,100,3000},
                 {Heading::Right}));
    ok &= Check(sustained.notes == fragmented.notes &&
                sustained.speed > fragmented.speed &&
                sustained.overall > fragmented.overall,
                "whole-chart sustained density matters beyond isolated bursts");

    const auto diagonal = CalculateChartDifficulty(
        Sequence(45, {100}, {Heading::UpRight}));
    ok &= Check(diagonal.keyPresses == 90 &&
                diagonal.coordination > fast.coordination &&
                diagonal.keysPerSecondPeak > fast.keysPerSecondPeak &&
                diagonal.overall > fast.overall,
                "diagonal is one note but two physical keys and greater strain");

    // The difficult case is NOT merely a chord: the next note reuses a
    // physical key from that chord, so the finger must reattack quickly.
    // Compare equally dense sequences with identical note/key totals.
    const auto sharedChordToSingle = CalculateChartDifficulty(
        Sequence(80, {100}, {Heading::UpRight, Heading::Right}));
    const auto freshChordToSingle = CalculateChartDifficulty(
        Sequence(80, {100}, {Heading::UpRight, Heading::Down}));
    ok &= Check(sharedChordToSingle.notes == freshChordToSingle.notes &&
                sharedChordToSingle.keyPresses == freshChordToSingle.keyPresses &&
                sharedChordToSingle.coordination >
                    freshChordToSingle.coordination &&
                sharedChordToSingle.overall > freshChordToSingle.overall,
                "diagonal followed by its shared cardinal key is harder");

    const auto sharedChordToChord = CalculateChartDifficulty(
        Sequence(80, {100}, {Heading::UpRight, Heading::DownRight}));
    const auto freshChordToChord = CalculateChartDifficulty(
        Sequence(80, {100}, {Heading::UpRight, Heading::DownLeft}));
    ok &= Check(sharedChordToChord.keyPresses == freshChordToChord.keyPresses &&
                sharedChordToChord.coordination >
                    freshChordToChord.coordination &&
                sharedChordToChord.overall > freshChordToChord.overall,
                "rapid overlapping diagonal doubles carry extra coordination strain");

    const auto relaxedChordReuse = CalculateChartDifficulty(
        Sequence(80, {650}, {Heading::UpRight, Heading::Right}));
    ok &= Check(sharedChordToSingle.coordination >
                relaxedChordReuse.coordination &&
                sharedChordToSingle.overall > relaxedChordReuse.overall,
                "shared-key retrigger pressure falls as intervals get longer");

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
                loops.fingerStrain < steady.fingerStrain &&
                loops.overall < steady.overall,
                "alternating fingers are easier than jacks despite route overlap");
    const auto roll = CalculateChartDifficulty(Sequence(80, {100},
        {Heading::Up, Heading::Right, Heading::Down, Heading::Left}));
    const auto trill = CalculateChartDifficulty(Sequence(80, {100},
        {Heading::Up, Heading::Right}));
    const auto jack = CalculateChartDifficulty(Sequence(80, {100}, {Heading::Up}));
    ok &= Check(roll.keyPresses == jack.keyPresses && trill.keyPresses == jack.keyPresses &&
                roll.fingerStrain < trill.fingerStrain && trill.fingerStrain < jack.fingerStrain &&
                roll.overall < trill.overall && trill.overall < jack.overall,
                "same NPS: distributed roll < trill < one-finger jack");
    const auto chordJack = CalculateChartDifficulty(Sequence(80, {100}, {Heading::UpRight}));
    ok &= Check(chordJack.overall > jack.overall &&
                chordJack.coordination > jack.coordination,
                "two-finger chordjack is harder than single jack");
    // Same physical workload (one-to-one remapping of all four keys)
    // may change route crossings and turn angles. It must not change score.
    const auto routeA = CalculateChartDifficulty(Sequence(120, {100,140},
        {Heading::Up,Heading::Right,Heading::Down,Heading::Left}));
    const auto routeB = CalculateChartDifficulty(Sequence(120, {100,140},
        {Heading::Up,Heading::Down,Heading::Right,Heading::Left}));
    ok &= Check(std::abs(routeA.overall - routeB.overall) < 1e-8 &&
                std::abs(routeA.reading - routeB.reading) > 1e-3,
                "geometry cannot change difficulty for equivalent input");
    const auto irregular = CalculateChartDifficulty(Sequence(80, {75,150,50,125,110,190,90},
        {Heading::Right}));
    const auto repeating = CalculateChartDifficulty(Sequence(80, {75,150},
        {Heading::Right}));
    ok &= Check(irregular.rhythm > repeating.rhythm,
                "repeating rhythm is discounted relative to irregular gaps");
    auto padded = Sequence(80, {100}, {Heading::Right});
    const auto burst = CalculateChartDifficulty(padded);
    auto tail = Sequence(500, {800}, {Heading::Right});
    for (auto e : tail.turns) {
        e.timeMs += 30000;
        e.beat = TickAtTimeMs(padded.bpm, padded.ticksPerBeat, 0, e.timeMs);
        padded.turns.push_back(e);
    }
    ok &= Check(CalculateChartDifficulty(padded).overall + 1e-8 >= burst.overall,
                "padding a hard passage with easy notes cannot dilute rating");

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

    // A BPM map is editor notation. Identical song-time inputs and headings
    // must produce identical rating dimensions, even with tempo changes.
    Chart slowerGrid = transformed;
    slowerGrid.bpm = 60.0;
    ok &= Check(SameDimensions(reference,
                               CalculateChartDifficulty(slowerGrid)),
                "changing only BPM does not change any difficulty component");
    Chart variableGrid = transformed;
    variableGrid.bpm = 180.0;
    variableGrid.tempoMap = {{0, 180.0}, {7200, 73.0}, {11000, 260.0}};
    ok &= Check(SameDimensions(reference,
                               CalculateChartDifficulty(variableGrid)),
                "changing tempo map without note timestamps is rating-invariant");

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
    std::cout << "DIFFICULTY_V4_PASS physical key strain and recovery\n";
    return 0;
}
