#include "core/chart_json.h"
#include "core/route.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace locus::core;

namespace {
bool Expect(bool ok, const std::string& explanation) {
    if (!ok) std::cerr << "CHART_JSON_FAIL: " << explanation << '\n';
    return ok;
}

std::string ValidChartText() {
    return R"({
        "formatVersion": 1,
        "title": "My Chart",
        "audioPath": "audio/song.mp3",
        "bpm": 120,
        "offsetSeconds": 0.25,
        "grid": {"width": 4, "height": 4},
        "start": {"x": 3, "y": 0, "heading": "right"},
        "previewLeadBeats": 4,
        "turns": [
            {"beat": 0, "turn": "left"},
            {"beat": 4, "turn": "right"}
        ]
    })";
}

bool Reject(std::string text, const std::string& from, const std::string& to,
            const std::string& errorFragment) {
    const auto index = text.find(from);
    if (index == std::string::npos) return false;
    text.replace(index, from.size(), to);
    LoadedChart parsed;
    std::string error;
    const bool accepted = ParseChartJson(text, parsed, error);
    return !accepted && error.find(errorFragment) != std::string::npos;
}
} // namespace

int main() {
    bool ok = true;
    const std::string valid = ValidChartText();
    LoadedChart parsed;
    std::string error;
    ok &= Expect(ParseChartJson(valid, parsed, error), "valid v1 chart parses: " + error);
    ok &= Expect(parsed.title == "My Chart" && parsed.audioPath == "audio/song.mp3" &&
                 parsed.route.bpm == 120 && parsed.offsetSeconds == 0.25 &&
                 parsed.route.start.x == 3 && parsed.route.start.y == 0 &&
                 parsed.route.initialHeading == Heading::Right &&
                 parsed.route.turns.size() == 2 &&
                 parsed.route.turns.front().beat == 0,
                 "original versioned chart fields preserved");
    ok &= Expect(PrepareChartForSong(parsed, 11.0, error) &&
                 parsed.route.firstBeat == -1 &&
                 parsed.route.lengthBeats == 22,
                 "song duration covers the whole track, including pre-offset negative beats");
    if (parsed.route.lengthBeats > 4) {
        const RouteStep first = SampleRouteStep(parsed.route, 0);
        const RouteStep later = SampleRouteStep(parsed.route, 4);
        ok &= Expect(first.heading == Heading::Up &&
                     first.from.x == 4 && first.from.y == 0 &&
                     first.to.x == 4 && first.to.y == -1 &&
                     later.heading == Heading::Right &&
                     later.from.x == 4 && later.from.y == -4 &&
                     later.to.x == 5 && later.to.y == -4,
                     "legacy JSON now follows continuous infinite world geometry");
    }
    ok &= Expect(Reject(valid, "\"formatVersion\": 1", "\"formatVersion\": 3",
                        "formatVersion"), "rejects unsupported future schema");
    ok &= Expect(Reject(valid, "\"width\": 4", "\"width\": 17", "grid.width") &&
                 Reject(valid, "\"height\": 4", "\"height\": 1", "grid.height"),
                 "rejects grid dimensions outside supported 2..16 range");
    std::string largeGrid = valid;
    const auto replaceOnce = [](std::string& value, const std::string& before,
                                const std::string& after) {
        const auto offset = value.find(before);
        if (offset == std::string::npos) return false;
        value.replace(offset, before.size(), after);
        return true;
    };
    bool largeFixture = replaceOnce(largeGrid, "\"width\": 4", "\"width\": 8");
    largeFixture &= replaceOnce(largeGrid, "\"height\": 4", "\"height\": 8");
    largeFixture &= replaceOnce(largeGrid, "\"x\": 3", "\"x\": 7");
    LoadedChart wideParsed;
    ok &= Expect(largeFixture && ParseChartJson(largeGrid, wideParsed, error) &&
                 wideParsed.route.gridWidth == 8 &&
                 wideParsed.route.gridHeight == 8 &&
                 wideParsed.route.start.x == 7,
                 "8/8 field size and a seventh-index start parse correctly");
    std::string largeSerialized;
    LoadedChart largeAuthored = wideParsed;
    largeAuthored.route.lengthBeats = 8;
    largeAuthored.authoredLengthBeats = 8;
    ok &= Expect(SerializeChartJson(largeAuthored, largeSerialized, error) &&
                 largeSerialized.find("\"width\": 8") != std::string::npos &&
                 largeSerialized.find("\"height\": 8") != std::string::npos,
                 "8/8 field dimensions persist through serialization");
    ok &= Expect(Reject(valid, "\"width\": 4", "\"width\": 2", "start.x"),
                 "start cell is validated against the actual grid width");
    std::string rectangleJson = valid;
    bool rectangleFixture = replaceOnce(rectangleJson, "\"width\": 4", "\"width\": 6");
    rectangleFixture &= replaceOnce(rectangleJson, "\"height\": 4", "\"height\": 10");
    LoadedChart rectangle;
    ok &= Expect(rectangleFixture && ParseChartJson(rectangleJson, rectangle, error) &&
                 rectangle.route.gridWidth == 6 && rectangle.route.gridHeight == 10,
                 "rectangular field sizes are supported independently");
    ok &= Expect(Reject(valid, "\"heading\": \"right\"",
                        "\"heading\": \"north\"", "start.heading"),
                 "rejects unknown heading");
    ok &= Expect(Reject(valid, "\"audioPath\": \"audio/song.mp3\"",
                        "\"audioPath\": \"../outside.mp3\"", "audioPath"),
                 "blocks chart-directory escape");
    ok &= Expect(Reject(valid, "\"beat\": 4", "\"beat\": 0", "turns[1].beat"),
                 "blocks duplicate beats");
    ok &= Expect(Reject(valid, "\"title\": \"My Chart\"",
                        "\"title\": \"My Chart\", \"title\": \"Overwritten\"",
                        "duplicate JSON key"), "rejects duplicate JSON keys");
    ok &= Expect(Reject(valid, "\"offsetSeconds\": 0.25",
                        "\"offsetSeconds\": \"wrong\"", "offsetSeconds"),
                 "rejects string numeric fields");
    ok &= Expect(Reject(valid, "\"start\": {", "\"unexpected\": true, \"start\": {",
                        "unexpected"), "rejects unknown fields");
    ok &= Expect(Reject(valid, "\"turns\": [", "\"turns\": [42,",
                        "turns[0]"), "rejects malformed turn entries");
    ok &= Expect(!ValidChartAudioPath("audio//song.mp3") &&
                 !ValidChartAudioPath("C:/song.wav") &&
                 !ValidChartAudioPath("file://song.wav") &&
                 !ValidChartAudioPath("..\\song.mp3") &&
                 !ValidChartAudioPath("sub/../song.mp3") &&
                 ValidChartAudioPath("audio/song.wav"),
                 "validates portable chart-relative audio paths");
    ok &= Expect(!PrepareChartForSong(parsed, 0.1, error) &&
                 error.find("turns[") != std::string::npos,
                 "rejects a turn that occurs outside the song even before offset");
    LoadedChart early;
    const std::string negative = std::string(valid).replace(
        valid.find("\"offsetSeconds\": 0.25"), sizeof("\"offsetSeconds\": 0.25") - 1,
        "\"offsetSeconds\": -0.5");
    ok &= Expect(ParseChartJson(negative, early, error) &&
                 PrepareChartForSong(early, 10.0, error) &&
                 early.route.firstBeat == 0 &&
                 early.route.lengthBeats == 21 &&
                 early.route.turns[0].beat == 0,
                 "negative offset preserves existing beat-zero silent preroll turns");

    // New format is milliseconds; legacy second-based charts remain valid.
    std::string millis = valid;
    const std::string secondsField = "\"offsetSeconds\": 0.25";
    const auto position = millis.find(secondsField);
    ok &= Expect(position != std::string::npos, "legacy offset fixture exists");
    if (position != std::string::npos) {
        millis.replace(position, secondsField.size(), "\"offsetMs\": 250");
        LoadedChart millisecondChart;
        ok &= Expect(ParseChartJson(millis, millisecondChart, error) &&
                         millisecondChart.offsetSeconds == 0.25 &&
                         PrepareChartForSong(millisecondChart, 11.0, error) &&
                         millisecondChart.route.lengthBeats == parsed.route.lengthBeats,
                     "offsetMs=250 must mean the same as legacy 0.25 seconds");
        ok &= Expect(Reject(millis, "\"offsetMs\": 250",
                            "\"offsetMs\": \"oops\"", "offsetMs"),
                     "offsetMs must be numeric");
        ok &= Expect(Reject(millis, "\"offsetMs\": 250",
                            "\"offsetMs\": 250, \"offsetSeconds\": 0.25", "offsetMs"),
                     "reject ambiguous units when both offset fields appear");
        ok &= Expect(Reject(millis, "\"offsetMs\": 250",
                            "\"offsetMs\": 250.0, \"offsetMs\": 300", "duplicate JSON key"),
                     "duplicate offsetMs remains forbidden");
    }
    ok &= Expect(Reject(valid, "\"offsetSeconds\": 0.25", "\"wrongOffset\": 250",
                        "offsetMs"), "an offset value is always required");

    // M-04: a saved chart must parse back with exact authored fields,
    // length and turn order, and must use offsetMs (not legacy seconds).
    LoadedChart editable;
    ok &= Expect(ParseChartJson(millis, editable, error),
                 "millisecond chart parses before editing");
    editable.route.lengthBeats = 16;
    editable.authoredLengthBeats = 16;
    editable.route.turns.push_back({9, Turn::Left});
    editable.offsetSeconds = 0.137;
    std::string serialized;
    ok &= Expect(SerializeChartJson(editable, serialized, error),
                 "new chart serializes: " + error);
    ok &= Expect(serialized.find("\"offsetMs\"") != std::string::npos &&
                 serialized.find("\"offsetSeconds\"") == std::string::npos &&
                 serialized.find("\"lengthBeats\"") == std::string::npos,
                 "writer uses offsetMs and never saves manual chart length");
    LoadedChart reopened;
    ok &= Expect(ParseChartJson(serialized, reopened, error) &&
                 !reopened.authoredLengthBeats.has_value() &&
                 reopened.route.turns.size() == 3 &&
                 std::abs(reopened.offsetSeconds - 0.137) < 1e-9 &&
                 reopened.route.turns.back().beat == 9,
                 "editor creation round-trips through JSON");
    ok &= Expect(PrepareChartForSong(reopened, 11.0, error) &&
                 reopened.route.lengthBeats == 22,
                 "the editor timeline always spans the full song");
    LoadedChart legacyLength = reopened;
    legacyLength.authoredLengthBeats = 65536;
    ok &= Expect(PrepareChartForSong(legacyLength, 11.0, error) &&
                 legacyLength.route.lengthBeats == 22 &&
                 !legacyLength.authoredLengthBeats,
                 "legacy saved length metadata no longer truncates the song");

    int firstBeat = 0, endBeat = 0;
    ok &= Expect(ComputeSongBeatRange(120.0, 2.5, 10.0,
                                     firstBeat, endBeat, error) &&
                 firstBeat == -5 && endBeat == 15,
                 "offset 2500ms gives five negative beats and fifteen nonnegative");
    const std::string negativeEvent = R"({
        "formatVersion":1,"title":"Early","audioPath":"song.mp3",
        "bpm":120,"offsetMs":2500,"grid":{"width":4,"height":4},
        "start":{"x":0,"y":0,"heading":"right"},
        "previewLeadBeats":8,
        "turns":[{"beat":-3,"turn":"left"},{"beat":1,"turn":"right"}]
    })";
    LoadedChart beforeOffset;
    std::string saved;
    ok &= Expect(ParseChartJson(negativeEvent, beforeOffset, error) &&
                 PrepareChartForSong(beforeOffset, 10.0, error) &&
                 beforeOffset.route.firstBeat == -5 &&
                 SampleRouteStep(beforeOffset.route, -3).heading == Heading::Up &&
                 SerializeChartJson(beforeOffset, saved, error) &&
                 saved.find("\"timeMs\": 1000") != std::string::npos,
                 "negative beat turn before offset works across parse, route and save");

    // v1 extension: old left/right remains 90 degrees, new tokens carry angles.
    LoadedChart diagonalChart;
    diagonalChart.route = Chart{};
    diagonalChart.title = "Diagonal Chart";
    diagonalChart.audioPath = "song.mp3";
    diagonalChart.route.start = {3, 0};
    diagonalChart.route.initialHeading = Heading::UpRight;
    diagonalChart.route.turns = {{0, Turn::Right45},
                                 {3, Turn::Left135}};
    std::string diagonalJson;
    LoadedChart parsedDiagonal;
    ok &= Expect(SerializeChartJson(diagonalChart, diagonalJson, error) &&
                 diagonalJson.find("\"up-right\"") != std::string::npos &&
                 diagonalJson.find("\"right45\"") != std::string::npos &&
                 diagonalJson.find("\"left135\"") != std::string::npos &&
                 ParseChartJson(diagonalJson, parsedDiagonal, error) &&
                 parsedDiagonal.route.initialHeading == Heading::UpRight &&
                 parsedDiagonal.route.turns[0].direction == Turn::Right45 &&
                 parsedDiagonal.route.turns[1].direction == Turn::Left135,
                 "diagonal initial heading and 45/135 turns roundtrip in v1 JSON");
    ok &= Expect(Reject(diagonalJson, "\"right45\"", "\"right180\"",
                        ".turn"), "unknown turn angle is rejected");
    ok &= Expect(Reject(diagonalJson, "\"up-right\"", "\"north-east\"",
                        "start.heading"), "unknown diagonal heading spelling is rejected");

    LoadedChart reversedChart = diagonalChart;
    reversedChart.title = "Overlapping route";
    reversedChart.route.initialHeading = Heading::Right;
    reversedChart.route.turns = {{2, Turn::Reverse}, {4, Turn::Reverse}};
    std::string reversedJson;
    LoadedChart parsedReverse;
    ok &= Expect(SerializeChartJson(reversedChart, reversedJson, error) &&
                 reversedJson.find("reverse") != std::string::npos &&
                 ParseChartJson(reversedJson, parsedReverse, error) &&
                 parsedReverse.route.turns.size() == 2 &&
                 parsedReverse.route.turns[0].direction == Turn::Reverse &&
                 parsedReverse.route.turns[1].direction == Turn::Reverse &&
                 parsedReverse.route.turns[0].timeMs == 1000 &&
                 parsedReverse.route.turns[1].timeMs == 2000,
                 "JSON v2 roundtrip preserves reversed turns and exact timestamps");
    ok &= Expect(Reject(reversedJson, "reverse", "turnaround", ".turn"),
                 "unknown 180-degree spelling is rejected");
    std::string oldFormatReverse = valid;
    const std::string oldTurn = "\"turn\": \"left\"";
    const auto oldTurnPos = oldFormatReverse.find(oldTurn);
    if (oldTurnPos != std::string::npos)
        oldFormatReverse.replace(oldTurnPos, oldTurn.size(),
                                 "\"turn\": \"reverse\"");
    LoadedChart parsedOldReverse;
    ok &= Expect(oldTurnPos != std::string::npos &&
                 ParseChartJson(oldFormatReverse, parsedOldReverse, error) &&
                 parsedOldReverse.route.turns[0].direction == Turn::Reverse,
                 "relative reversal parses consistently for authored v1 and v2 charts");

    // Version 2 keeps absolute integer millisecond timestamps.
    const std::string msV2 = R"({
        "formatVersion":2,"title":"Timing","audioPath":"song.mp3",
        "bpm":250,"offsetMs":500,"ticksPerBeat":6,"snapDivision":3,
        "grid":{"width":4,"height":4},
        "start":{"x":0,"y":0,"heading":"right"},
        "previewLeadBeats":4,
        "turns":[{"timeMs":580,"turn":"left"},
                 {"timeMs":660,"turn":"right"}]
    })";
    LoadedChart precise;
    std::string v2saved;
    ok &= Expect(ParseChartJson(msV2, precise, error) &&
                 precise.route.turns.size() == 2 &&
                 precise.route.turns[0].beat == 2 &&
                 precise.route.turns[0].timeMs == 580 &&
                 precise.route.turns[1].beat == 4 &&
                 precise.route.turns[1].timeMs == 660 &&
                 precise.route.bpm == 250 &&
                 precise.route.snapDivision == 3,
                 "v2 notes use whole song milliseconds, not musical beat indices");
    ok &= Expect(SerializeChartJson(precise, v2saved, error) &&
                 v2saved.find("\"formatVersion\": 2") != std::string::npos &&
                 v2saved.find("\"timeMs\": 580") != std::string::npos &&
                 v2saved.find("\"timeMs\": 660") != std::string::npos &&
                 v2saved.find("\"offsetMs\": 500") != std::string::npos &&
                 v2saved.find("\"beat\"") == std::string::npos,
                 "v2 writes only integer audio timestamps for notes and offset");
    ok &= Expect(Reject(msV2, "\"offsetMs\":500", "\"offsetMs\":500.5",
                        "offsetMs"),
                 "v2 rejects fractional offset milliseconds");
    ok &= Expect(Reject(msV2, "\"timeMs\":580", "\"timeMs\":580.5",
                        "timeMs"),
                 "v2 rejects fractional note timestamps");
    ok &= Expect(Reject(msV2, "\"timeMs\":660", "\"timeMs\":580",
                        "timeMs"),
                 "v2 rejects duplicate timestamps");

    // The public example is part of the product experience; ensure it
    // remains a REAL current-format chart rather than stale v1 docs.
    std::ifstream fixture("examples/first-song.locus.json", std::ios::binary);
    ok &= Expect(fixture.good(), "shipped first-song chart file exists");
    if (fixture.good()) {
        const std::string example((std::istreambuf_iterator<char>(fixture)),
                                  std::istreambuf_iterator<char>());
        LoadedChart sample;
        ok &= Expect(ParseChartJson(example, sample, error),
                     "shipped v2 example parses: " + error);
        ok &= Expect(sample.route.gridWidth == 8 &&
                     sample.route.gridHeight == 8 &&
                     sample.route.turns.size() == 9 &&
                     sample.route.turns.front().timeMs == 1000 &&
                     sample.route.turns.back().timeMs == 15000 &&
                     sample.route.snapDivision == 1,
                     "shipped example has a square view and absolute ms timestamps");
        ok &= Expect(PrepareChartForSong(sample, 20.0, error),
                     "shipped sample validates for a 20-second audio file: " + error);
        std::string current;
        ok &= Expect(SerializeChartJson(sample, current, error) &&
                     current.find("\"formatVersion\": 2") != std::string::npos &&
                     current.find("\"timeMs\": 15000") != std::string::npos,
                     "shipped example round-trips with the current v2 writer");
    }

    if (!ok) return 1;
    std::cout << "CHART_JSON_PASS version=2 validation=1 infinite=1 preroll=1 example=1\n";
    return 0;
}
