#include "app/run_results.h"
#include "app/atomic_text_file.h"

#include "core/utf8.h"
#include "core/chart_json.h"
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
                               key.difficultyId;
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
        {"version", 2}, {"id", r.id}, {"songSetId", r.songSetId},
        {"difficultyId", r.difficultyId},
        {"gameplayRevision", r.gameplayRevision},
        {"judgementVersion", r.judgementVersion},
        {"songTitle", r.songTitle}, {"chartTitle", r.chartTitle},
        {"completedAtMs", r.completedAtMs},
        {"perfect", r.grades.perfect}, {"good", r.grades.good},
        {"bad", r.grades.bad}, {"miss", r.grades.miss},
        {"notesJudged", r.notesJudged},
        {"accuracyPercent", r.accuracyPercent},
        {"practiceRate", r.practiceRate},
        {"variablePracticeRate", r.variablePracticeRate}
    };
}

bool Decode(const Json& obj, RunResult& out) {
    try {
        if (!obj.is_object() || obj.at("version").get<int>() != 2)
            return false;
        RunResult r;
        r.id = obj.at("id").get<std::string>();
        r.songSetId = obj.at("songSetId").get<std::string>();
        r.difficultyId = obj.at("difficultyId").get<std::string>();
        r.gameplayRevision = obj.at("gameplayRevision").get<std::string>();
        r.judgementVersion = obj.at("judgementVersion").get<int>();
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
        // v2 records created before variable-rate tracking represented a
        // single nominal speed. New records always persist this explicit flag.
        r.variablePracticeRate = obj.value("variablePracticeRate", false);
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
        !core::ValidDifficultyId(r.difficultyId) ||
        !core::ValidDifficultyId(r.gameplayRevision) ||
        r.judgementVersion < 1 || r.judgementVersion > 1000 ||
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

bool CaptureCompletedRun(const RunResultsStore& store,
                         const CompletedRunContext& completion,
                         RunResult& saved, std::string& error) {
    error.clear();
    // A Results screen or a repeated frame is NOT a new completed attempt.
    // Never publish TEST, abandoned sessions, or a partially judged chart.
    if (!completion.completedThisFrame || completion.editorTest ||
        completion.launchedFromEditor || completion.key.Empty() ||
        completion.scorableCount == 0 ||
        completion.scorableCount >
            static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        completion.grades.Resolved() !=
            static_cast<int>(completion.scorableCount))
        return false;

    RunResult record;
    record.songSetId = completion.key.songSetId;
    record.difficultyId = completion.key.difficultyId;
    record.gameplayRevision = completion.gameplayRevision;
    record.songTitle = completion.songTitle;
    record.chartTitle = completion.chartTitle;
    record.grades = completion.grades;
    record.notesJudged = static_cast<int>(completion.scorableCount);
    record.practiceRate = completion.practiceRate;
    record.variablePracticeRate = completion.variablePracticeRate;
    if (!store.Save(record, error)) return false;
    saved = std::move(record);
    return true;
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
    const fs::path dir = Directory({r.songSetId, r.difficultyId});
    fs::create_directories(dir, ec);
    if (ec) {
        error = "Results directory could not be created: " + ec.message();
        return false;
    }
    for (int suffix = 0; suffix < 1000; ++suffix) {
        const std::string fileId = r.id + "-" + std::to_string(suffix);
        RunResult candidate = r;
        candidate.id = fileId;
        if (!ValidRunResult(candidate)) {
            error = "Result identifier exceeds the supported length.";
            return false;
        }
        const fs::path dest = dir / (fileId + ".json");
        // No exists()/ofstream gap: the CREATE_NEW/link publication is
        // exclusive even if two processes picked the same timestamp and ID.
        // Abandoned legacy .pending files must not block new attempts.
        const auto status = AtomicCreateTextFile(
            dest, Encode(candidate).dump(2), error);
        if (status == AtomicTextWriteStatus::AlreadyExists) continue;
        if (status != AtomicTextWriteStatus::Published) return false;
        r.id = candidate.id;
        error.clear();
        return true;
    }
    error = "Could not allocate a unique result filename.";
    return false;
}

std::vector<RunResult> RunResultsStore::Load(const RunKey& key,
                                             std::string& error,
                                             const std::atomic<bool>* cancelled) const {
    std::vector<RunResult> records;
    error.clear();
    if (key.Empty()) return records;
    const fs::path dir = Directory(key);
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        if (ec) error = "Could not access saved results.";
        return records;
    }
    // Missing history is normal; an unreadable folder or failed directory
    // traversal is NOT an empty history. Never publish a partial result list
    // when an iterator fails partway through enumeration.
    if (!fs::is_directory(dir, ec) || ec) {
        error = "Saved results path is not an accessible directory.";
        return records;
    }
    fs::directory_iterator current(dir, fs::directory_options::none, ec);
    if (ec) {
        error = "Could not list saved results: " + ec.message();
        return records;
    }
    const fs::directory_iterator end;
    while (current != end) {
        if (cancelled && cancelled->load(std::memory_order_relaxed)) {
            error = "History loading cancelled.";
            return {};
        }
        const fs::directory_entry entry = *current;
        current.increment(ec);
        if (ec) {
            error = "Saved results directory traversal failed: " + ec.message();
            return {};
        }
        if (entry.is_symlink(ec) || !entry.is_regular_file(ec) ||
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
                record.difficultyId != key.difficultyId ||
                entry.path().stem().string() != record.id) continue;
            records.push_back(std::move(record));
        } catch (const std::exception&) {
            // A corrupt or incomplete record never hides independent runs.
        }
    }
    if (cancelled && cancelled->load(std::memory_order_relaxed)) {
        error = "History loading cancelled.";
        return {};
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
