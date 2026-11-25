#include "core/chart_json.h"
#include "core/tempo_map.h"
#include "core/utf8.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace locus::core {
namespace {
using Json = nlohmann::json;

// The pinned JSON 3.12 callback DOM parser scans its parent array at
// every end_object (including when the callback always returns true).
// For the turns array that makes strict duplicate checking quadratic.
// Use the public streaming interface to validate unique decoded keys,
// then the ordinary linear DOM parser to feed the existing schema checks.
// No private library internals, schema duplication or permissive fallback.
class UniqueKeysSax final : public nlohmann::json_sax<Json> {
public:
    explicit UniqueKeysSax(std::string& error) : error_(error) {}
    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(number_integer_t) override { return true; }
    bool number_unsigned(number_unsigned_t) override { return true; }
    bool number_float(number_float_t,const string_t&) override { return true; }
    bool string(string_t&) override { return true; }
    bool binary(binary_t&) override { return true; }
    bool start_array(std::size_t) override { return true; }
    bool end_array() override { return true; }
    bool start_object(std::size_t) override {
        members_.emplace_back();
        return true;
    }
    bool key(string_t& value) override {
        if (!members_.back().insert(value).second) {
            error_="JSON: duplicate JSON key: "+value;
            return false;
        }
        return true;
    }
    bool end_object() override {
        members_.pop_back();
        return true;
    }
    bool parse_error(std::size_t,const std::string&,
                     const Json::exception& exception) override {
        error_=std::string("JSON: ")+exception.what();
        return false;
    }
private:
    std::string& error_;
    std::vector<std::set<std::string>> members_;
};

bool HasExactMembers(const Json& value, std::initializer_list<const char*> names,
                     const std::string& field, std::string& error) {
    if (!value.is_object()) {
        error = field + ": expected an object";
        return false;
    }
    std::set<std::string> expected(names.begin(), names.end());
    for (const auto& item : value.items()) {
        if (!expected.count(item.key())) {
            error = field + "." + item.key() + ": unknown field";
            return false;
        }
    }
    for (const auto& name : expected) {
        if (!value.contains(name)) {
            error = field + "." + name + ": required field is missing";
            return false;
        }
    }
    return true;
}

bool IntegerInRange(const Json& value, int minimum, int maximum, int& output,
                    const std::string& field, std::string& error) {
    if (!value.is_number_integer() || value.get<double>() < minimum ||
        value.get<double>() > maximum) {
        error = field + ": expected a whole number within the supported range";
        return false;
    }
    output = value.get<int>();
    return true;
}

bool Number(const Json& value, double& output, const std::string& field,
            bool positive, std::string& error) {
    if (!value.is_number()) {
        error = field + ": expected a number";
        return false;
    }
    output = value.get<double>();
    if (!std::isfinite(output) || (positive && output <= 0.0)) {
        error = field + ": expected a finite " +
                (positive ? std::string("positive number") : std::string("number"));
        return false;
    }
    return true;
}

bool Text(const Json& value, std::string& output, const std::string& field,
          std::string& error) {
    if (!value.is_string()) {
        error = field + ": expected a string";
        return false;
    }
    output = value.get<std::string>();
    if (!utf8::ValidName(output, 1024, 1024)) {
        error = field + ": expected valid, nonempty UTF-8 text of at most 1024 bytes";
        return false;
    }
    return true;
}

} // namespace

bool ValidChartAudioPath(std::string_view path) noexcept {
    if (path.empty() || path.front() == '/' || path.back() == '/' ||
        path.find('\\') != path.npos || path.find(':') != path.npos ||
        path.find('\0') != path.npos || path.find('?') != path.npos ||
        path.find('*') != path.npos || path.find('"') != path.npos ||
        path.find('<') != path.npos || path.find('>') != path.npos ||
        path.find('|') != path.npos) return false;
    std::size_t begin = 0;
    do {
        const auto end = path.find('/', begin);
        const auto part = path.substr(begin, end == path.npos ? path.npos : end - begin);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == path.npos) break;
        begin = end + 1;
    } while (true);
    return true;
}

bool ParseChartJson(std::string_view text, LoadedChart& result, std::string& error) {
    try {
        UniqueKeysSax uniqueKeys(error);
        if (!Json::sax_parse(text.begin(),text.end(),&uniqueKeys)) return false;
        const Json document = Json::parse(text.begin(),text.end());
        if (!document.is_object()) {
            error = "chart: expected an object";
            return false;
        }
        const bool hasDifficultyId = document.contains("difficultyId");
        const bool hasTicks = document.contains("ticksPerBeat");
        const bool hasSnap = document.contains("snapDivision");
        std::set<std::string> expected = {
            "formatVersion", "title", "audioPath", "bpm",
            "offsetMs", "tempoMap", "grid", "start", "previewLeadBeats", "turns"
        };
        if (hasDifficultyId) expected.insert("difficultyId");
        if (hasTicks) expected.insert("ticksPerBeat");
        if (hasSnap) expected.insert("snapDivision");
        for (const auto& member : document.items())
            if (!expected.count(member.key())) {
                error = "chart." + member.key() + ": unknown field";
                return false;
            }
        for (const auto& name : expected)
            if (!document.contains(name)) {
                error = "chart." + name + ": required field is missing";
                return false;
            }
        int version = 0;
        if (!IntegerInRange(document["formatVersion"], 4, 5, version,
                            "formatVersion", error)) {
            error = "formatVersion: expected version 4 or 5";
            return false;
        }

        LoadedChart parsed;
        if (!Text(document["title"], parsed.title, "title", error) ||
            !Text(document["audioPath"], parsed.audioPath, "audioPath", error))
            return false;
        if (hasDifficultyId) {
            if (!document["difficultyId"].is_string() ||
                !ValidDifficultyId(document["difficultyId"].get<std::string>())) {
                error = "difficultyId: expected a 32-digit lowercase hexadecimal identifier";
                return false;
            }
            parsed.difficultyId = document["difficultyId"].get<std::string>();
        }
        if (!ValidChartAudioPath(parsed.audioPath)) {
            error = "audioPath: expected a safe relative forward-slash path";
            return false;
        }
        if (!Number(document["bpm"], parsed.route.bpm, "bpm", true, error) ||
            !Number(document["previewLeadBeats"], parsed.previewLeadBeats,
                    "previewLeadBeats", true, error)) return false;
        if (hasTicks && !IntegerInRange(document["ticksPerBeat"], 1, 48,
                parsed.route.ticksPerBeat, "ticksPerBeat", error)) return false;
        if (hasSnap && !IntegerInRange(document["snapDivision"], 1, 24,
                parsed.route.snapDivision, "snapDivision", error)) return false;
        const int divisor = parsed.route.snapDivision;
        if ((divisor != 1 && divisor != 2 && divisor != 3 &&
             divisor != 4 && divisor != 6 && divisor != 8 &&
             divisor != 12 && divisor != 16 && divisor != 24) ||
            parsed.route.ticksPerBeat % divisor != 0) {
            error = "snapDivision: must divide ticksPerBeat exactly";
            return false;
        }
        int offsetMs = 0;
        if (!IntegerInRange(document["offsetMs"], -3600000, 3600000,
                            offsetMs, "offsetMs", error)) return false;
        parsed.offsetSeconds = offsetMs / 1000.0;
        {
            const Json& changes = document["tempoMap"];
            if (!changes.is_array() || changes.empty() || changes.size() > 256) {
                error = "tempoMap: expected 1 to 256 entries";
                return false;
            }
            for (std::size_t i = 0; i < changes.size(); ++i) {
                const std::string where = "tempoMap[" + std::to_string(i) + "]";
                if (!HasExactMembers(changes[i], {"timeMs", "bpm"}, where, error))
                    return false;
                if (!changes[i]["timeMs"].is_number_integer()) {
                    error = where + ".timeMs: expected integer milliseconds";
                    return false;
                }
                const std::int64_t time = changes[i]["timeMs"].get<std::int64_t>();
                double tempo = 0.0;
                if (!Number(changes[i]["bpm"], tempo, where + ".bpm",
                            true, error)) return false;
                parsed.route.tempoMap.push_back({time, tempo});
            }
            if (!ValidTempoMap(parsed.route.tempoMap, error)) return false;
            if (parsed.route.tempoMap.front().timeMs != offsetMs ||
                parsed.route.tempoMap.front().bpm != parsed.route.bpm) {
                error = "tempoMap[0]: must match chart bpm and offsetMs";
                return false;
            }
        }

        if (!HasExactMembers(document["grid"], {"width", "height"}, "grid", error))
            return false;
        int width = 0, height = 0;
        if (!IntegerInRange(document["grid"]["width"], kMinGridSize, kMaxGridSize, width,
                            "grid.width", error) ||
            !IntegerInRange(document["grid"]["height"], kMinGridSize, kMaxGridSize, height,
                            "grid.height", error)) return false;
        parsed.route.gridWidth = width;
        parsed.route.gridHeight = height;

        if (!HasExactMembers(document["start"], {"x", "y", "heading"}, "start", error))
            return false;
        if (!IntegerInRange(document["start"]["x"], 0, width - 1,
                            parsed.route.start.x, "start.x", error) ||
            !IntegerInRange(document["start"]["y"], 0, height - 1,
                            parsed.route.start.y, "start.y", error)) return false;
        const Json& heading = document["start"]["heading"];

        if (!heading.is_string()) {
            error = "start.heading: expected a compass direction";
            return false;
        }
        constexpr const char* headingNames[] = {
            "u", "r", "d", "l",
            "ru", "rd", "ld", "lu"
        };
        const std::string name = heading.get<std::string>();
        bool recognized = false;
        for (int i = 0; i < 8; ++i) {
            if (name == headingNames[i]) {
                parsed.route.initialHeading = static_cast<Heading>(i);
                recognized = true;
                break;
            }
        }
        if (!recognized) {
            error = "start.heading: expected a compass direction";
            return false;
        }

        const Json& turns = document["turns"];
        if (!turns.is_array()) {
            error = "turns: expected an array";
            return false;
        }
        if (turns.size() > 65536) {
            error = "turns: too many events";
            return false;
        }
        const TempoProjection projection(parsed.route,parsed.offsetSeconds);
        parsed.route.turns.reserve(turns.size());
        int prior = std::numeric_limits<int>::min();
        std::int64_t previousMs = std::numeric_limits<std::int64_t>::min();
        std::int64_t previousTail = std::numeric_limits<std::int64_t>::min();
        for (std::size_t i = 0; i < turns.size(); ++i) {
            const std::string where = "turns[" + std::to_string(i) + "]";
            const bool hasHold = version == 5 && turns[i].contains("holdMs");
            if (hasHold) {
                if (!HasExactMembers(turns[i], {"timeMs", "heading", "holdMs"}, where, error))
                    return false;
            } else if (!HasExactMembers(turns[i], {"timeMs", "heading"}, where, error))
                return false;
            const Json& stamp = turns[i]["timeMs"];
            if (!stamp.is_number_integer()) {
                error = where + ".timeMs: expected whole milliseconds";
                return false;
            }
            const std::int64_t timeMs = stamp.get<std::int64_t>();
            if (timeMs < -3600000 || timeMs > 86400000 ||
                timeMs <= previousMs) {
                error = where + ".timeMs: out of range or not increasing";
                return false;
            }
            int holdMs = 0;
            if (hasHold && !IntegerInRange(turns[i]["holdMs"], 1, 90000000,
                                           holdMs, where + ".holdMs", error))
                return false;
            if (!ValidHoldDuration(timeMs, holdMs) || timeMs < previousTail) {
                error = where + ".holdMs: invalid duration or overlapping hold";
                return false;
            }
            previousMs = timeMs;
            previousTail = timeMs + holdMs;
            const int beat = projection.TickAtMs(timeMs);
            if (beat < -1000000 || beat > 999999) {
                error = where + ".timeMs: outside tick range";
                return false;
            }
            // Absolute timestamps are unique and strictly increasing;
            // several valid events may project to one editor grid tick.
            // The grid is a placement aid, not the persistent event identity.
            if (beat < prior) {
                error = where + ".timeMs: projected tick order reversed";
                return false;
            }
            prior = beat;

            if (!turns[i]["heading"].is_string()) {
                error = where + ".heading: expected an absolute compass direction";
                return false;
            }
            const std::string direction = turns[i]["heading"].get<std::string>();
            bool recognized = false;
            for (int j = 0; j < 8; ++j) {
                if (direction == headingNames[j]) {
                    parsed.route.turns.push_back(
                        {beat, static_cast<Heading>(j), timeMs, holdMs});
                    recognized = true;
                    break;
                }
            }
            if (!recognized) {
                error = where + ".heading: unknown compass direction";
                return false;
            }
        }
        parsed.route.lengthBeats = 1; // Replaced by PrepareChartForSong().
        result = std::move(parsed);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string("JSON: ") + exception.what();
        return false;
    }
}

bool SerializeChartJson(const LoadedChart& chart, std::string& json,
                        std::string& error) {
    try {
        using Ordered = nlohmann::ordered_json;
        Ordered document = Ordered::object();
        bool hasHolds = false;
        for (const auto& event : chart.route.turns)
            hasHolds |= event.holdMs != 0;
        document["formatVersion"] = hasHolds ? 5 : 4;
        document["title"] = chart.title;
        if (!chart.difficultyId.empty()) {
            if (!ValidDifficultyId(chart.difficultyId)) {
                error = "difficultyId: invalid persistent identity";
                return false;
            }
            document["difficultyId"] = chart.difficultyId;
        }
        document["audioPath"] = chart.audioPath;
        document["bpm"] = chart.route.bpm;
        const auto tempo = EffectiveTempoMap(chart.route, chart.offsetSeconds);
        if (!ValidTempoMap(tempo, error)) return false;
        document["tempoMap"] = Ordered::array();
        for (const auto& marker : tempo)
            document["tempoMap"].push_back({
                {"timeMs", marker.timeMs}, {"bpm", marker.bpm}});
        if (chart.route.ticksPerBeat != 1)
            document["ticksPerBeat"] = chart.route.ticksPerBeat;
        if (chart.route.snapDivision != 1)
            document["snapDivision"] = chart.route.snapDivision;
        // Use only integer absolute milliseconds for song time and
        // offset; the authoring-grid resolution is separate metadata.
        document["offsetMs"] = static_cast<std::int64_t>(
            std::llround(chart.offsetSeconds * 1000.0));
        document["grid"] = {{"width", chart.route.gridWidth},
                             {"height", chart.route.gridHeight}};

        const char* headings[] = {
            "u", "r", "d", "l",
            "ru", "rd", "ld", "lu"
        };
        const int heading = static_cast<int>(chart.route.initialHeading);
        if (!ValidHeading(chart.route.initialHeading)) {
            error = "start.heading: invalid heading";
            return false;
        }
        document["start"] = {{"x", chart.route.start.x},
                             {"y", chart.route.start.y},
                             {"heading", headings[heading]}};
        document["previewLeadBeats"] = chart.previewLeadBeats;
        // Song length is derived from the audio file, not authored.
        document["turns"] = Ordered::array();

        for (const auto& event : chart.route.turns) {
            if (!ValidHeading(event.direction)) {
                error = "turns: invalid absolute compass heading";
                return false;
            }
            document["turns"].push_back({
                {"timeMs", EventTimeMs(chart.route, event, chart.offsetSeconds)},
                {"heading", headings[static_cast<int>(event.direction)]}
            });
            if (event.holdMs != 0)
                document["turns"].back()["holdMs"] = event.holdMs;
        }
        std::string encoded = document.dump(2, ' ', false,
                                           Ordered::error_handler_t::strict) + "\n";
        LoadedChart checked;
        if (!ParseChartJson(encoded, checked, error)) return false;
        json = std::move(encoded);
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = std::string("Cannot serialize chart: ") + exception.what();
        return false;
    }
}

bool ComputeSongBeatRange(double bpm, double offsetSeconds, double durationSeconds,
                          int& firstBeat, int& endBeat, std::string& error) {
    if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0) {
        error = "audioPath: music duration is invalid";
        return false;
    }
    if (!std::isfinite(bpm) || bpm <= 0.0 ||
        !std::isfinite(offsetSeconds)) {
        error = "bpm/offsetMs: BPM or offset is invalid";
        return false;
    }
    const double earliest = -offsetSeconds * bpm / 60.0;
    const double latest = (durationSeconds - offsetSeconds) * bpm / 60.0;
    // Keep old negative-offset charts playable in their silent preroll:
    // their first authored beat is still zero, not the first audio sample.
    const double first = std::min(0.0, std::floor(earliest));
    const double end = std::ceil(latest);
    if (!std::isfinite(first) || !std::isfinite(end) ||
        first < -1000000.0 || end > 1000000.0 || end - first < 1.0 ||
        end - first > 1000000.0) {
        error = "bpm/offsetMs: song must contain 1 to 1000000 timeline ticks";
        return false;
    }
    firstBeat = static_cast<int>(first);
    endBeat = static_cast<int>(end);
    error.clear();
    return true;
}

bool PrepareChartForSong(LoadedChart& chart, double songDurationSeconds,
                         std::string& error) {
    int first = 0, end = 0;
    if (chart.route.ticksPerBeat < 1 || chart.route.ticksPerBeat > 48 ||
        chart.route.snapDivision < 1 ||
        chart.route.ticksPerBeat % chart.route.snapDivision != 0) {
        error = "ticksPerBeat/snapDivision: unsupported tick resolution";
        return false;
    }
    if (!chart.route.tempoMap.empty() &&
        !ValidTempoMap(chart.route.tempoMap, error)) return false;
    if (!TempoTickRange(chart.route, chart.offsetSeconds,
                        songDurationSeconds, first, end, error)) return false;
    std::int64_t previousTail = std::numeric_limits<std::int64_t>::min();
    for (std::size_t i = 0; i < chart.route.turns.size(); ++i) {
        const auto& event = chart.route.turns[i];
        const auto timeMs = EventTimeMs(chart.route, event, chart.offsetSeconds);
        const double timestamp = timeMs / 1000.0;
        if (!ValidHoldDuration(timeMs, event.holdMs) || timeMs < previousTail ||
            (event.holdMs > 0 &&
                (timeMs + event.holdMs) / 1000.0 > songDurationSeconds + 1e-9)) {
            error = "turns[" + std::to_string(i) +
                    "].holdMs: overlapping hold or tail outside audio";
            return false;
        }
        previousTail = timeMs + event.holdMs;
        if (event.beat < first || event.beat >= end ||
            !std::isfinite(timestamp) ||
            (timestamp < 0.0 &&
             !(chart.offsetSeconds < 0.0 && event.beat >= 0)) ||
            timestamp >= songDurationSeconds) {
            error = "turns[" + std::to_string(i) +
                    "].beat: note outside actual song audio";
            return false;
        }
    }
    chart.route.firstBeat = first;
    chart.route.lengthBeats = end;
    error.clear();
    return true;
}

} // namespace locus::core
