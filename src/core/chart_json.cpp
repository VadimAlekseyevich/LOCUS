#include "core/chart_json.h"

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
    if (output.find_first_not_of(" \r\t\n") == std::string::npos ||
        output.size() > 1024) {
        error = field + ": expected nonempty text of at most 1024 UTF-8 bytes";
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
        // offsetMs is the preferred v1 spelling. Previously authored
        // offsetSeconds charts remain readable, but both fields are forbidden
        // together so the beat-zero position is never ambiguous.
        const bool hasMs = document.is_object() && document.contains("offsetMs");
        const bool hasSeconds = document.is_object() && document.contains("offsetSeconds");
        if (hasMs == hasSeconds) {
            error = "offsetMs: provide exactly one of offsetMs or legacy offsetSeconds";
            return false;
        }
        if (hasMs) {
            if (!HasExactMembers(document,
                    {"formatVersion", "title", "audioPath", "bpm", "offsetMs",
                     "grid", "start", "previewLeadBeats", "turns"},
                    "chart", error)) {
                // Repeat only when the documented optional length is present;
                // never let extra/unknown JSON members slip through.
                if (!document.contains("lengthBeats") ||
                    !HasExactMembers(document,
                        {"formatVersion", "title", "audioPath", "bpm", "offsetMs",
                         "grid", "start", "previewLeadBeats", "turns", "lengthBeats"},
                        "chart", error)) return false;
            }
        } else if (!HasExactMembers(document,
                    {"formatVersion", "title", "audioPath", "bpm", "offsetSeconds",
                     "grid", "start", "previewLeadBeats", "turns"},
                    "chart", error)) {
            if (!document.contains("lengthBeats") ||
                !HasExactMembers(document,
                    {"formatVersion", "title", "audioPath", "bpm", "offsetSeconds",
                     "grid", "start", "previewLeadBeats", "turns", "lengthBeats"},
                    "chart", error)) return false;
        }

        int version = 0;
        if (!IntegerInRange(document["formatVersion"], 1, 1, version,
                            "formatVersion", error)) return false;

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
        double offset = 0.0;
        if (!Number(document[hasMs ? "offsetMs" : "offsetSeconds"], offset,
                    hasMs ? "offsetMs" : "offsetSeconds", false, error))
            return false;
        parsed.offsetSeconds = hasMs ? offset / 1000.0 : offset;

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
            error = "start.heading: expected up/right/down/left";
            return false;
        }
        const std::string name = heading.get<std::string>();
        if (name == "up") parsed.route.initialHeading = Heading::Up;
        else if (name == "right") parsed.route.initialHeading = Heading::Right;
        else if (name == "down") parsed.route.initialHeading = Heading::Down;
        else if (name == "left") parsed.route.initialHeading = Heading::Left;
        else {
            error = "start.heading: expected up/right/down/left";
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
        int prior = -1;
        for (std::size_t i = 0; i < turns.size(); ++i) {
            const std::string where = "turns[" + std::to_string(i) + "]";
            if (!HasExactMembers(turns[i], {"beat", "turn"}, where, error))
                return false;
            int beat = 0;
            if (!IntegerInRange(turns[i]["beat"], 0, 65535, beat,
                                where + ".beat", error)) return false;
            if (beat <= prior) {
                error = where + ".beat: events must be strictly increasing (no duplicates)";
                return false;
            }
            prior = beat;
            if (!turns[i]["turn"].is_string()) {
                error = where + ".turn: expected left or right";
                return false;
            }
            const std::string direction = turns[i]["turn"].get<std::string>();
            if (direction != "left" && direction != "right") {
                error = where + ".turn: expected left or right";
                return false;
            }
            parsed.route.turns.push_back({beat, direction == "left"
                                                     ? Turn::Left : Turn::Right});
        }
        parsed.route.lengthBeats = 1; // Replaced by PrepareChartForSong().
        if (document.contains("lengthBeats")) {
            int authored = 0;
            if (!IntegerInRange(document["lengthBeats"], 1, 65536, authored,
                                "lengthBeats", error)) return false;
            if (!parsed.route.turns.empty() &&
                parsed.route.turns.back().beat >= authored) {
                error = "lengthBeats: must be greater than the final turn beat";
                return false;
            }
            parsed.authoredLengthBeats = authored;
        }
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
        document["formatVersion"] = 1;
        document["title"] = chart.title;
        document["audioPath"] = chart.audioPath;
        document["bpm"] = chart.route.bpm;
        // No loss of fractional milliseconds from older second-based charts.
        document["offsetMs"] = chart.offsetSeconds * 1000.0;
        document["grid"] = {{"width", chart.route.gridWidth},
                             {"height", chart.route.gridHeight}};
        const char* headings[] = {"up", "right", "down", "left"};
        const int heading = static_cast<int>(chart.route.initialHeading);
        if (heading < 0 || heading > 3) {
            error = "start.heading: invalid heading";
            return false;
        }
        document["start"] = {{"x", chart.route.start.x},
                             {"y", chart.route.start.y},
                             {"heading", headings[heading]}};
        document["previewLeadBeats"] = chart.previewLeadBeats;
        if (chart.authoredLengthBeats) {
            document["lengthBeats"] = *chart.authoredLengthBeats;
        }
        document["turns"] = Ordered::array();
        for (const auto& event : chart.route.turns) {
            document["turns"].push_back({
                {"beat", event.beat},
                {"turn", event.direction == Turn::Left ? "left" : "right"}
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

bool PrepareChartForSong(LoadedChart& chart, double songDurationSeconds,
                         std::string& error) {
    if (!std::isfinite(songDurationSeconds) || songDurationSeconds <= 0.0) {
        error = "audioPath: music duration is invalid";
        return false;
    }
    const double beatCount = (songDurationSeconds - chart.offsetSeconds) *
                             chart.route.bpm / 60.0;
    if (!std::isfinite(beatCount) || beatCount <= 0.0 || beatCount > 65536.0) {
        error = "bpm/offsetSeconds: song chart must contain 1 to 65536 beats";
        return false;
    }
    if (chart.authoredLengthBeats) {
        if (*chart.authoredLengthBeats > static_cast<int>(std::ceil(beatCount))) {
            error = "lengthBeats: chart is longer than the selected audio";
            return false;
        }
    }
    for (std::size_t i = 0; i < chart.route.turns.size(); ++i) {
        const auto& event = chart.route.turns[i];
        const double targetSeconds =
            chart.offsetSeconds + event.beat * 60.0 / chart.route.bpm;
        if (!std::isfinite(targetSeconds) || targetSeconds >= songDurationSeconds) {
            error = "turns[" + std::to_string(i) +
                    "].beat: event occurs after song end";
            return false;
        }
    }
    chart.route.lengthBeats = chart.authoredLengthBeats.value_or(
        static_cast<int>(std::ceil(beatCount)));
    error.clear();
    return true;
}

} // namespace locus::core
