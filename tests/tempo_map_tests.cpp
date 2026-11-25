#include "core/chart_editor.h"
#include "core/chart_json.h"
#include "core/tempo_map.h"
#include "core/timed_route.h"
#include "core/judgement.h"
#include "app/song_timing.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace locus::core;

namespace {
bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "TEMPO_MAP_FAIL " << message << '\n';
    return condition;
}
bool Near(double lhs, double rhs, double epsilon = 1e-7) {
    return std::abs(lhs - rhs) <= epsilon;
}
}

int main() {
    bool ok = true;
    const std::vector<TempoMarker> map = {
        {250, 120.0}, {20250, 180.0}, {40250, 90.0}
    };
    std::string error;
    ok &= Check(ValidTempoMap(map, error) &&
                Near(TempoBeatAtMs(map, 250), 0) &&
                Near(TempoBeatAtMs(map, 20250), 40) &&
                Near(TempoBeatAtMs(map, 40250), 100) &&
                Near(TempoBeatAtMs(map, 50250), 115) &&
                Near(TempoBeatAtMs(map, 0), -0.5),
                "aligned markers preserve their exact song-time positions");
    for (double beat : {-3.0, 0.0, 12.25, 40.0, 59.5, 100.0, 120.0}) {
        ok &= Check(Near(TempoBeatAtMs(map, TempoMsAtBeat(map, beat)), beat),
                    "beat to song milliseconds is invertible on both sides of markers");
    }
    // A BPM marker at an arbitrary millisecond becomes beat zero of its
    // new local meter, without retiming anything before or after it.
    const std::vector<TempoMarker> offGrid = {
        {0, 120.0}, {1250, 240.0}, {2250, 90.0}
    };
    ok &= Check(Near(TempoBeatAtMs(offGrid, 1249), 2.498) &&
                Near(TempoBeatAtMs(offGrid, 1250), 3.0) &&
                Near(TempoBeatAtMs(offGrid, 1500), 4.0) &&
                Near(TempoBeatAtMs(offGrid, 2250), 7.0) &&
                Near(TempoSegmentStartBeat(offGrid, 1), 3.0) &&
                Near(TempoSegmentStartBeat(offGrid, 2), 7.0),
                "each BPM marker restarts the local beat grid on an exact downbeat");
    ok &= Check(Near(TempoMsAtBeat(offGrid, 3.0), 1250) &&
                Near(TempoMsAtBeat(offGrid, 4.0), 1500) &&
                Near(TempoMsAtBeat(offGrid, 7.0), 2250) &&
                Near(TempoMsAtBeat(offGrid, 2.7), 1250),
                "beat-to-time positions are exact; unplayed gap is pinned to marker");

    Chart anchored;
    anchored.bpm = 120.0;
    anchored.tempoMap = offGrid;
    anchored.ticksPerBeat = 24;
    anchored.snapDivision = 4;
    anchored.firstBeat = 0;
    anchored.lengthBeats = 240;
    ok &= Check(TempoSnapTickAtSeconds(anchored, 0.0, 1.25) == 72 &&
                TempoSnapTickAtSeconds(anchored, 0.0, 1.50) == 96 &&
                TempoSnapTickAtSeconds(anchored, 0.0, 2.25) == 168 &&
                TempoSnapTickAtSeconds(anchored, 0.0, 1.20) == 72,
                "clicking an inserted BPM marker lands on its own beat and grid");
    ok &= Check(Near(TempoMsAtBeat(offGrid,
                     TempoSnapTickAtSeconds(anchored, 0.0, 1.50) / 24.0), 1500),
                "snapped tick maps back to correct source audio milliseconds");
    ok &= Check(TempoAdjacentBarTick(anchored, 0.0, 48, 1) == 72 &&
                TempoAdjacentBarTick(anchored, 0.0, 72, 1) == 168 &&
                TempoAdjacentBarTick(anchored, 0.0, 96, -1) == 72,
                "bar navigation lands at each local BPM marker downbeat");
    // Explicitly break the old rule that markers always continue the prior
    // fractional beat: this is a pre-release semantic format change.
    // A single audio cursor belongs to the SONG, never to the tick grid of
    // whichever compared difficulty originally started preview playback.
    Chart coarse, fine;
    coarse.bpm = fine.bpm = map.front().bpm;
    coarse.tempoMap = fine.tempoMap = map;
    coarse.ticksPerBeat = 12;
    fine.ticksPerBeat = 24;
    for (double songSeconds : {0.0, 1.25, 20.25, 29.75, 40.25, 51.0}) {
        const double coarseTick = TempoBeatAtSeconds(coarse, 0.25,
            songSeconds) * coarse.ticksPerBeat;
        const double fineTick = TempoBeatAtSeconds(fine, 0.25,
            songSeconds) * fine.ticksPerBeat;
        ok &= Check(Near(coarseTick / 12.0, fineTick / 24.0),
                    "shared audio time projects to each difficulty's own tick grid");
    }

    auto invalid = map;
    invalid[1].timeMs = 250;
    ok &= Check(!ValidTempoMap(invalid, error),
                "duplicate or backwards BPM markers are rejected");
    invalid = map;
    invalid[1].bpm = -1.0;
    ok &= Check(!ValidTempoMap(invalid, error),
                "invalid BPM is rejected before any chart changes");

    LoadedChart source;
    source.title = "Tempo sample";
    source.audioPath = "song.mp3";
    source.route.bpm = 120.0;
    source.offsetSeconds = 0.25;
    source.route.tempoMap = map;
    source.route.ticksPerBeat = 6;
    source.route.snapDivision = 3;
    source.route.turns = {
        {9, Heading::Down, 1000},
        {270, Heading::Left, 22000},
        {605, Heading::Up, 40500}
    };
    std::string json;
    LoadedChart reread;
    ok &= Check(SerializeChartJson(source, json, error) &&
                json.find("\"formatVersion\": 4") != std::string::npos &&
                ParseChartJson(json, reread, error) &&
                reread.route.tempoMap == map &&
                reread.route.turns.size() == 3 &&
                reread.route.turns[0].timeMs == 1000 &&
                reread.route.turns[1].timeMs == 22000,
                "chart v4 serializes all BPM segments and absolute note timestamps");
    ok &= Check(PrepareChartForSong(reread, 60.0, error),
                "song duration projects tempo-map ticks with negative pre-offset");
    TimedRoute route;
    ok &= Check(route.Compile(reread.route, reread.offsetSeconds) &&
                Near(route.Turns()[0].songSeconds, 1.0) &&
                Near(route.Turns()[1].songSeconds, 22.0) &&
                Near(route.Turns()[2].songSeconds, 40.5),
                "compiled gameplay route retains absolute event times");

    ChartEditor editor;
    editor.Load(reread);
    ok &= Check(editor.SetSongDuration(60.0), "load song duration for tempo editing");
    const auto originalTimes = std::vector<std::int64_t>{
        editor.Data().route.turns[0].timeMs,
        editor.Data().route.turns[1].timeMs,
        editor.Data().route.turns[2].timeMs};
    const int previousTick = editor.Data().route.turns[1].beat;
    auto changed = map;
    changed[1].bpm = 220.0;
    ok &= Check(editor.SetTempoMap(changed) &&
                editor.Data().route.turns[1].timeMs == originalTimes[1] &&
                editor.Data().route.turns[1].beat != previousTick &&
                editor.Undo() &&
                editor.Data().route.tempoMap == map &&
                editor.Data().route.turns[1].timeMs == originalTimes[1],
                "tempo edit, note re-projection and Undo preserve authored audio time");

    ok &= Check(!editor.SetTempoMap(invalid) &&
                editor.Data().route.tempoMap == map,
                "invalid map is transactional and cannot damage the draft");

    // T05 canonical example: 750ms is not aligned to the 120 BPM
    // 500ms beat. The following 240 BPM starts a new local downbeat
    // exactly there; previously authored timestamps must not move.
    const std::vector<TempoMarker> at750{
        {0, 120.0}, {750, 240.0}
    };
    ok &= Check(Near(TempoBeatAtMs(at750, 749), 1.498) &&
                Near(TempoBeatAtMs(at750, 750), 2.0) &&
                Near(TempoBeatAtMs(at750, 1000), 3.0) &&
                Near(TempoMsAtBeat(at750, 2.0), 750.0) &&
                Near(TempoMsAtBeat(at750, 3.0), 1000.0),
                "750ms off-grid BPM change itself is the next strong beat");
    ChartEditor markerEditor;
    ok &= Check(markerEditor.SetSongDuration(8.0),
                "load 8s song for tempo re-projection");
    markerEditor.SelectBeat(1);
    markerEditor.PutTurn(Heading::Left);
    markerEditor.SelectBeat(2);
    markerEditor.PutTurn(Heading::Up);
    const std::int64_t originalFirst =
        markerEditor.Data().route.turns[0].timeMs;
    const std::int64_t originalSecond =
        markerEditor.Data().route.turns[1].timeMs;
    ok &= Check(markerEditor.SetTempoMap(at750) &&
                markerEditor.Data().route.turns[0].timeMs == originalFirst &&
                markerEditor.Data().route.turns[1].timeMs == originalSecond &&
                markerEditor.Data().route.turns[0].beat == 1 &&
                markerEditor.Data().route.turns[1].beat == 3 &&
                markerEditor.Undo() &&
                markerEditor.Data().route.turns[1].beat == 2 &&
                markerEditor.Data().route.turns[1].timeMs == originalSecond,
                "BPM change reprojects ticks but never retimes notes; Undo restores");

    // T05: JSON already serializes integer song milliseconds. Grid-tick
    // collisions are NOT timestamp collisions. At 120 BPM, both 1000 and
    // 1010ms round to tick two, and both must survive real file roundtrip.
    LoadedChart denseStored;
    denseStored.title = "Sub-tick turns";
    denseStored.audioPath = "song.wav";
    denseStored.route.bpm = 120.0;
    denseStored.route.ticksPerBeat = 1;
    denseStored.route.turns = {
        {2, Heading::Up, 1000}, {2, Heading::Right, 1010}
    };
    std::string denseJson;
    LoadedChart denseRead;
    ok &= Check(SerializeChartJson(denseStored, denseJson, error) &&
                ParseChartJson(denseJson, denseRead, error) &&
                PrepareChartForSong(denseRead, 4.0, error) &&
                denseRead.route.turns.size() == 2 &&
                denseRead.route.turns[0].beat == 2 &&
                denseRead.route.turns[1].beat == 2 &&
                denseRead.route.turns[0].timeMs == 1000 &&
                denseRead.route.turns[1].timeMs == 1010,
                "both exact notes survive v4 serialization in one grid cell");
    TimedRoute denseRoute;
    ok &= Check(denseRoute.Compile(denseRead.route, 0.0) &&
                denseRoute.Turns().size() == 2 &&
                Near(denseRoute.Turns()[0].songSeconds, 1.0) &&
                Near(denseRoute.Turns()[1].songSeconds, 1.010),
                "PLAY route preserves both exact 10ms-spaced turns");
    TurnJudge denseJudge(denseRead.route);
    const auto up = denseJudge.PressHeading(Heading::Up, 1.0);
    const auto right = denseJudge.PressHeading(Heading::Right, 1.010);
    ok &= Check(up && right &&
                denseJudge.Counts().perfect == 2 &&
                denseJudge.Counts().Resolved() == 2,
                "PLAY judge awards two independent Perfect notes in one tick");
    ChartEditor denseDraft;
    denseDraft.Load(denseRead);
    ok &= Check(denseDraft.SetSongDuration(4.0) &&
                denseDraft.SetTempoMap({{0, 120.0}, {750, 240.0}}) &&
                denseDraft.Data().route.turns[0].beat == 3 &&
                denseDraft.Data().route.turns[1].beat == 3 &&
                denseDraft.Data().route.turns[0].timeMs == 1000 &&
                denseDraft.Data().route.turns[1].timeMs == 1010 &&
                denseDraft.Undo() &&
                denseDraft.Data().route.turns[0].beat == 2 &&
                denseDraft.Data().route.turns[1].beat == 2,
                "editor BPM reprojects without moving microtimed events");
    ok &= Check(locus::app::ApplySongTiming(
                    denseRead, {{0, 120.0}, {750, 240.0}}, error) &&
                denseRead.route.turns.size() == 2 &&
                denseRead.route.turns[0].beat == 3 &&
                denseRead.route.turns[1].beat == 3 &&
                denseRead.route.turns[0].timeMs == 1000 &&
                denseRead.route.turns[1].timeMs == 1010,
                "Song Set BPM updates preserve both exact notes atomically");

    // Compare the indexed batch path to the original scalar math across
    // negative preroll, marker boundaries, reserved gaps and the full
    // 256-marker limit. Exact integer tick identity must not change.
    {
        Chart indexed;
        indexed.bpm=120.0;
        indexed.ticksPerBeat=24;
        const double offset=-0.137;
        bool equivalent=true;
        for (int mode=0;mode<4;++mode) {
            indexed.tempoMap.clear();
            if (mode==1) indexed.tempoMap={{-137,123.45}};
            if (mode==2) {
                for(int i=0;i<256;++i)
                    indexed.tempoMap.push_back({-137+i*1237LL,75.25+(i%11)*17.125});
            }
            // Tiny positive BPM is valid; exercise repeated anchor beats
            // and the original ceil epsilon's overlapping inverse bounds.
            if (mode==3) indexed.tempoMap={{-137,1e-9},{-136,1e-9},
                {-135,120.0},{-134,240.0},{1250,90.0}};
            const TempoProjection projection(indexed,offset);
            for(int i=-2048;i<20000;++i) {
                const auto ms=static_cast<std::int64_t>(i)*31;
                equivalent &= projection.TickAtMs(ms)==TempoTickAtMs(indexed,offset,ms);
                const double tick=i*0.875;
                equivalent &= Near(projection.SecondsAtTick(tick),
                    TempoSecondsAtTick(indexed,offset,tick));
            }
            for(const auto& marker:indexed.tempoMap) {
                for(const auto delta:{-1LL,0LL,1LL}) {
                    const auto ms=marker.timeMs+delta;
                    equivalent &= projection.TickAtMs(ms)==TempoTickAtMs(indexed,offset,ms);
                }
                const double tick=TempoBeatAtSeconds(indexed,offset,marker.timeMs/1000.0)
                    *indexed.ticksPerBeat;
                for(double delta:{-0.000001,0.0,0.000001})
                    equivalent &= Near(projection.SecondsAtTick(tick+delta),
                        TempoSecondsAtTick(indexed,offset,tick+delta));
            }
        }
        ok &= Check(equivalent,"indexed projection preserves scalar timing and reserved gaps");
    }

    if (ok) std::cout << "TEMPO_MAP_PASS local_downbeat=1 formats=4\n";
    return ok ? 0 : 1;
}
