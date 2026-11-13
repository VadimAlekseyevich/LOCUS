#include "app/run_results.h"

#include "core/utf8.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>

namespace locus::app {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;

// Two independently seeded FNV-1a halves: opaque stable directory names,
// no user-controlled path segments or platform-dependent std::hash.
std::string PathToken(const RunKey& key) {
    const std::string source = key.songSetId + std::string(1, '\0') +
                               key.difficultyFile;
    const auto hash = [&](std::uint64_t seed) {
        std::uint64_t h = seed;
        for (unsigned char c : source) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        return h;
    };
    std::ostringstream out;
    out << std::hex << std::setfill('0') <<
        std::setw(16) << hash(14695981039346656037ULL) <<
        std::setw(16) << hash(1099511628211ULL);
    return out.str();
}

std::int64_t ClockMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

Json Encode(const RunResult& r) {
    return Json{
        {"version", 1}, {"id", r.id}, {"songSetId", r.songSetId},
        {"difficultyFile", r.difficultyFile},
        {"songTitle", r.songTitle}, {"chartTitle", r.chartTitle},
        {"completedAtMs", r.completedAtMs},
        {"perfect", r.grades.perfect}, {"good", r.grades.good},
        {"bad", r.grades.bad}, {"miss", r.grades.miss},
        {"notesJudged", r.notesJudged},
        {"accuracyPercent", r.accuracyPercent},
        {"practiceRate", r.practiceRate}
    };
}

bool Decode(const Json& obj, RunResult& out) {
    try {
        if (!obj.is_object() || obj.at("version").get<int>() != 1)
            return false;
        RunResult r;
        r.id = obj.at("id").get<std::string>();
        r.songSetId = obj.at("songSetId").get<std::string>();
        r.difficultyFile = obj.at("difficultyFile").get<std::string>();
        r.songTitle = obj.at("songTitle").get<std::string>();
        r.chartTitle = obj.at("chartTitle").get<std::string>();
        r.completedAtMs = obj.at("completedAtMs").get<std::int64_t>();
        r.grades.perfect = obj.at("perfect").get<int>();
        r.grades.good = obj.at("good").get<int>();
        r.grades.bad = obj.at("bad").get<int>();
        r.grades.miss = obj.at("miss").get<int>();
        r.notesJudged = obj.at("notesJudged").get<int>();
        r.accuracyPercent = obj.at("accuracyPercent").get<double>();
        r.practiceRate = obj.at("practiceRate").get<double>();
        if (!ValidRunResult(r)) return false;
        out = std::move(r);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}
} // namespace

bool ValidRunResult(const RunResult& r) noexcept {
    if (r.songSetId.empty() || r.songSetId.size() > 256 ||
        r.difficultyFile.empty() || r.difficultyFile.size() > 512 ||
        r.id.empty() || r.id.size() > 80 ||
        !core::utf8::ValidName(r.songTitle, 256, 1024) ||
        !core::utf8::ValidName(r.chartTitle, 256, 1024) ||
        r.id.find_first_not_of("0123456789-") != std::string::npos ||
        r.completedAtMs <= 0 ||
        r.grades.perfect < 0 || r.grades.good < 0 ||
        r.grades.bad < 0 || r.grades.miss < 0 ||
        r.notesJudged <= 0 || r.notesJudged > 2000000 ||
        r.grades.perfect > r.notesJudged ||
        r.grades.good > r.notesJudged ||
        r.grades.bad > r.notesJudged ||
        r.grades.miss > r.notesJudged ||
        r.grades.Resolved() != r.notesJudged ||
        !std::isfinite(r.accuracyPercent) ||
        !std::isfinite(r.practiceRate) ||
        r.practiceRate <= 0.0 || r.practiceRate > 1.0) return false;
    const double computed = r.grades.AccuracyPercent();
    return std::abs(r.accuracyPercent - computed) <= 0.011;
}

std::string FormatRunTimestamp(std::int64_t ms) {
    if (ms <= 0) return "--";
    const std::time_t sec = static_cast<std::time_t>(ms / 1000);
    std::tm utc{};
#ifdef _WIN32
    if (gmtime_s(&utc, &sec) != 0) return "--";
#else
    if (!gmtime_r(&sec, &utc)) return "--";
#endif
    std::ostringstream text;
    text << std::put_time(&utc, "%Y-%m-%d %H:%M") << " UTC";
    return text.str();
}

fs::path RunResultsStore::Directory(const RunKey& key) const {
    return root_ / PathToken(key);
}

bool RunResultsStore::Save(RunResult& r, std::string& error) const {
    if (r.completedAtMs == 0) r.completedAtMs = ClockMs();
    r.accuracyPercent = r.grades.AccuracyPercent();
    if (r.id.empty()) r.id = std::to_string(r.completedAtMs);
    if (!ValidRunResult(r)) {
        error = "Invalid completed result.";
        return false;
    }
    std::error_code ec;
    const fs::path dir = Directory({r.songSetId, r.difficultyFile});
    fs::create_directories(dir, ec);
    if (ec) {
        error = "Results directory could not be created: " + ec.message();
        return false;
    }
    for (int suffix = 0; suffix < 1000; ++suffix) {
        const std::string fileId = r.id + "-" + std::to_string(suffix);
        const fs::path dest = dir / (fileId + ".json");
        const fs::path temp = dir / (fileId + ".pending");
        if (fs::exists(dest, ec) || fs::exists(temp, ec)) continue;
        RunResult candidate = r;
        candidate.id = fileId;
        const std::string encoded = Encode(candidate).dump(2);
        {
            std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
            if (!stream) {
                error = "Could not write the result file.";
                return false;
            }
            stream.write(encoded.data(),
                         static_cast<std::streamsize>(encoded.size()));
            stream.flush();
            if (!stream) {
                stream.close();
                fs::remove(temp, ec);
                error = "Result file could not be flushed.";
                return false;
            }
        }
        ec.clear();
        fs::rename(temp, dest, ec);
        if (!ec) {
            r.id = candidate.id;
            error.clear();
            return true;
        }
        fs::remove(temp, ec);
        error = "Could not atomically finalize result file.";
        return false;
    }
    error = "Could not allocate a unique result filename.";
    return false;
}

std::vector<RunResult> RunResultsStore::Load(const RunKey& key,
                                             std::string& error) const {
    std::vector<RunResult> records;
    error.clear();
    if (key.Empty()) return records;
    const fs::path dir = Directory(key);
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        if (ec) error = "Could not access saved results.";
        return records;
    }
    fs::directory_iterator current(dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        error = "Could not list saved results.";
        return records;
    }
    for (const auto& entry : current) {
        if (!entry.is_regular_file(ec) ||
            entry.path().extension() != ".json" ||
            entry.file_size(ec) > 16384 || ec) {
            ec.clear();
            continue;
        }
        try {
            std::ifstream stream(entry.path(), std::ios::binary);
            if (!stream) continue;
            Json json = Json::parse(stream);
            RunResult record;
            if (!Decode(json, record) ||
                record.songSetId != key.songSetId ||
                record.difficultyFile != key.difficultyFile ||
                entry.path().stem().string() != record.id) continue;
            records.push_back(std::move(record));
        } catch (const std::exception&) {
            // A corrupt/partial old file never hides other completed runs.
        }
    }
    std::sort(records.begin(), records.end(),
              [](const RunResult& a, const RunResult& b) {
                  if (a.completedAtMs != b.completedAtMs)
                      return a.completedAtMs > b.completedAtMs;
                  return a.id > b.id;
              });
    return records;
}

} // namespace locus::app
