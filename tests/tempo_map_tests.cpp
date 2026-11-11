#include "core/chart_editor.h"
#include "core/chart_json.h"
#include "core/tempo_map.h"
#include "core/timed_route.h"

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
                "tempo segments integrate without resetting beat phase");
    for (double beat : {-3.0, 0.0, 12.25, 40.0, 59.5, 100.0, 120.0}) {
        ok &= Check(Near(TempoBeatAtMs(map, TempoMsAtBeat(map, beat)), beat),
                    "beat to song milliseconds is invertible on both sides of markers");
    }
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

    if (ok) std::cout << "TEMPO_MAP_PASS continuous_phase=1 formats=4\n";
    return ok ? 0 : 1;
}
