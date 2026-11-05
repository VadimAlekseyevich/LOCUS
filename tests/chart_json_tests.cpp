#include "core/chart_json.h"
#include "core/route.h"

#include <iostream>
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
                 parsed.route.lengthBeats == 22,
                 "song duration determines a bounded number of beats");
    if (parsed.route.lengthBeats > 4) {
        const RouteStep first = SampleRouteStep(parsed.route, 0);
        const RouteStep later = SampleRouteStep(parsed.route, 4);
        ok &= Expect(first.heading == Heading::Up && first.wraps &&
                     first.to.x == 3 && first.to.y == 3 &&
                     later.heading == Heading::Right && later.wraps,
                     "loaded JSON drives unchanged deterministic Wrap route");
    }
    ok &= Expect(Reject(valid, "\"formatVersion\": 1", "\"formatVersion\": 2",
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
                 error.find("offsetSeconds") != std::string::npos,
                 "rejects charts beginning beyond end of audio");
    LoadedChart early;
    const std::string negative = std::string(valid).replace(
        valid.find("\"offsetSeconds\": 0.25"), sizeof("\"offsetSeconds\": 0.25") - 1,
        "\"offsetSeconds\": -0.5");
    ok &= Expect(ParseChartJson(negative, early, error) &&
                 PrepareChartForSong(early, 10.0, error) &&
                 early.route.lengthBeats == 21,
                 "negative offsets include chart-side silent preroll");

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
    editable.offsetSeconds = -0.137;
    std::string serialized;
    ok &= Expect(SerializeChartJson(editable, serialized, error),
                 "new chart serializes: " + error);
    ok &= Expect(serialized.find("\"offsetMs\"") != std::string::npos &&
                 serialized.find("\"offsetSeconds\"") == std::string::npos &&
                 serialized.find("\"lengthBeats\": 16") != std::string::npos,
                 "writer uses offsetMs and saves the editor timeline length");
    LoadedChart reopened;
    ok &= Expect(ParseChartJson(serialized, reopened, error) &&
                 reopened.authoredLengthBeats == 16 &&
                 reopened.route.turns.size() == 3 &&
                 std::abs(reopened.offsetSeconds + 0.137) < 1e-9 &&
                 reopened.route.turns.back().beat == 9,
                 "editor creation round-trips through JSON");
    ok &= Expect(PrepareChartForSong(reopened, 11.0, error) &&
                 reopened.route.lengthBeats == 16,
                 "shorter authored timeline survives song-duration validation");
    LoadedChart invalidLength = reopened;
    invalidLength.authoredLengthBeats = 65536;
    ok &= Expect(!PrepareChartForSong(invalidLength, 11.0, error) &&
                 error.find("lengthBeats") != std::string::npos,
                 "chart timeline cannot exceed available song duration");

    if (!ok) return 1;
    std::cout << "CHART_JSON_PASS version=1 validation=1 wrap=1 preroll=1\n";
    return 0;
}
