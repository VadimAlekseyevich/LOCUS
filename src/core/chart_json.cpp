#include "core/chart_json.h"
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
        // nlohmann/json otherwise silently keeps the last of duplicate keys.
        std::vector<std::set<std::string>> memberStack;
        const auto checkDuplicates = [&](int, Json::parse_event_t event, Json& value) {
            if (event == Json::parse_event_t::object_start) memberStack.emplace_back();
            if (event == Json::parse_event_t::key) {
                const auto key = value.get<std::string>();
                if (!memberStack.back().insert(key).second)
                    throw std::runtime_error("duplicate JSON key: " + key);
            }
            if (event == Json::parse_event_t::object_end) memberStack.pop_back();
            return true;
        };
        const Json document = Json::parse(text.begin(), text.end(), checkDuplicates);
        // No compatibility readers before release. Version 3 is the only format.
        if (!document.is_object()) {
            error = "chart: expected an object";
            return false;
        }
        const bool hasTicks = document.contains("ticksPerBeat");
        const bool hasSnap = document.contains("snapDivision");
        std::set<std::string> expected = {
            "formatVersion", "title", "audioPath", "bpm",
            "offsetMs", "grid", "start", "previewLeadBeats", "turns"
        };
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
        if (!IntegerInRange(document["formatVersion"], 3, 3, version,
                            "formatVersion", error)) {
            error = "formatVersion: only version 3 with absolute headings is supported";
            return false;
        }

        LoadedChart parsed;
        if (!Text(document["title"], parsed.title, "title", error) ||
            !Text(document["audioPath"], parsed.audioPath, "audioPath", error))
            return false;
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
        int prior = -65537;
        std::int64_t previousMs = std::numeric_limits<std::int64_t>::min();
        for (std::size_t i = 0; i < turns.size(); ++i) {
            const std::string where = "turns[" + std::to_string(i) + "]";
            if (!HasExactMembers(turns[i], {"timeMs", "heading"}, where, error))
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
            previousMs = timeMs;
            const int beat = TickAtTimeMs(parsed.route.bpm,
                parsed.route.ticksPerBeat, parsed.offsetSeconds, timeMs);
            if (beat < -1000000 || beat > 999999) {
                error = where + ".timeMs: outside tick range";
                return false;
            }
            if (beat <= prior) {
                error = where + ".timeMs: grid resolution merges notes";
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
                        {beat, static_cast<Heading>(j), timeMs});
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
        document["formatVersion"] = 3;
        document["title"] = chart.title;
        document["audioPath"] = chart.audioPath;
        document["bpm"] = chart.route.bpm;
        if (chart.route.ticksPerBeat != 1)
            document["ticksPerBeat"] = chart.route.ticksPerBeat;
        if (chart.route.snapDivision != 1)
            document["snapDivision"] = chart.route.snapDivision;
        // V3 uses ONLY integer absolute milliseconds for song time and
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
    if (!ComputeSongBeatRange(chart.route.bpm * chart.route.ticksPerBeat,
                              chart.offsetSeconds, songDurationSeconds,
                              first, end, error)) return false;
    for (std::size_t i = 0; i < chart.route.turns.size(); ++i) {
        const auto& event = chart.route.turns[i];
        const double timestamp = static_cast<double>(
            EventTimeMs(chart.route, event, chart.offsetSeconds)) / 1000.0;
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
