#include "core/chart_json.h"
#include "core/route.h"

#include <cmath>
#include <iostream>
#include <string>

using namespace locus::core;

namespace {
bool Expect(bool ok, const char* name) {
    if (!ok) std::cerr << "CHART_JSON_FAIL " << name << '\n';
    return ok;
}
std::string Sample() {
    return R"({
        "formatVersion": 3,
        "title": "Absolute Directions",
        "audioPath": "audio/song.mp3",
        "bpm": 120,
        "offsetMs": 250,
        "ticksPerBeat": 1,
        "snapDivision": 1,
        "grid": {"width": 8, "height": 8},
        "start": {"x": 3, "y": 3, "heading": "right"},
        "previewLeadBeats": 4,
        "turns": [
            {"timeMs": 1250, "heading": "up"},
            {"timeMs": 2750, "heading": "up-right"},
            {"timeMs": 4250, "heading": "left"}
        ]
    })";
}
bool Reject(const std::string& original, const std::string& before,
            const std::string& after, const std::string& needle) {
    std::string input = original;
    const auto pos = input.find(before);
    if (pos == std::string::npos) return false;
    input.replace(pos, before.size(), after);
    LoadedChart parsed;
    std::string error;
    return !ParseChartJson(input, parsed, error) &&
           error.find(needle) != std::string::npos;
}
}

int main() {
    bool ok = true;
    const std::string source = Sample();
    LoadedChart parsed;
    std::string error;
    ok &= Expect(ParseChartJson(source, parsed, error),
                 "strict version 3 absolute compass JSON parses");
    ok &= Expect(parsed.route.turns.size() == 3 &&
        parsed.route.turns[0].direction == Heading::Up &&
        parsed.route.turns[1].direction == Heading::UpRight &&
        parsed.route.turns[2].direction == Heading::Left &&
        parsed.route.turns[0].beat == 2 &&
        parsed.route.turns[1].beat == 5 &&
        parsed.route.turns[0].timeMs == 1250,
        "absolute headings and times do not depend on incoming travel");
    ok &= Expect(SampleRouteStep(parsed.route, 2).heading == Heading::Up &&
                 SampleRouteStep(parsed.route, 5).heading == Heading::UpRight &&
                 SampleRouteStep(parsed.route, 8).heading == Heading::Left,
                 "JSON heading drives geometry without relative rotation");
    ok &= Expect(PrepareChartForSong(parsed, 11.0, error) &&
                 parsed.route.lengthBeats == 22,
                 "audio duration derives playable chart span");
    std::string encoded;
    ok &= Expect(SerializeChartJson(parsed, encoded, error) &&
                 encoded.find("\"formatVersion\": 3") != std::string::npos &&
                 encoded.find("\"heading\": \"up-right\"") != std::string::npos &&
                 encoded.find("\"turn\"") == std::string::npos,
                 "writer emits only version 3 absolute headings");
    LoadedChart again;
    ok &= Expect(ParseChartJson(encoded, again, error) &&
                 again.route.turns.size() == 3 &&
                 again.route.turns[2].direction == Heading::Left &&
                 again.route.turns[2].timeMs == 4250,
                 "round trip preserves absolute directions and music timestamps");
    ok &= Expect(Reject(source, "\"formatVersion\": 3",
                       "\"formatVersion\": 2", "formatVersion") &&
                 Reject(source, "\"formatVersion\": 3",
                       "\"formatVersion\": 1", "formatVersion"),
                 "no pre-release backwards compatibility for old chart schemas");
    ok &= Expect(Reject(source, "\"heading\": \"up-right\"",
                       "\"turn\": \"left45\"", "turn") &&
                 Reject(source, "\"heading\": \"up-right\"",
                       "\"heading\": \"reverse\"", "heading"),
                 "relative turn fields and values are rejected");
    ok &= Expect(Reject(source, "\"heading\": \"up-right\"",
                       "\"heading\": \"north\"", "heading") &&
                 Reject(source, "\"heading\": \"right\"",
                       "\"heading\": \"north\"", "start.heading"),
                 "all compass headings must use supported spellings");
    ok &= Expect(Reject(source, "\"timeMs\": 2750",
                       "\"timeMs\": 1250", "timeMs") &&
                 Reject(source, "\"timeMs\": 2750",
                       "\"timeMs\": 1251", "grid resolution") &&
                 Reject(source, "\"timeMs\": 2750",
                       "\"timeMs\": 999999999", "timeMs"),
                 "absolute event times must be ordered, distinct on grid and bounded");
    ok &= Expect(Reject(source, "\"audioPath\": \"audio/song.mp3\"",
                       "\"audioPath\": \"../escape.mp3\"", "audioPath") &&
                 Reject(source, "\"width\": 8",
                       "\"width\": 17", "grid.width") &&
                 Reject(source, "\"offsetMs\": 250",
                       "\"offsetMs\": 250.3", "offsetMs"),
                 "safe paths, square view bounds and whole-millisecond offsets");
    ok &= Expect(Reject(source, "\"title\": \"Absolute Directions\"",
                       "\"title\": \"Absolute Directions\", \"title\": \"Again\"",
                       "duplicate JSON key") &&
                 Reject(source, "\"bpm\": 120",
                       "\"bpm\": 120, \"secret\": 1", "unknown field"),
                 "duplicate and unknown properties are not silently accepted");
    ok &= Expect(!ValidChartAudioPath("../song.mp3") &&
                 ValidChartAudioPath("audio/song.mp3"),
                 "only normalized relative audio references are accepted");
    if (!ok) return 1;
    std::cout << "CHART_JSON_PASS format=3 headings=absolute old_format=rejected\n";
    return 0;
}
