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
    Json result{
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
    result["hasReplay"] = r.hasReplay;
    if (r.statistics) {
        const auto& s = *r.statistics;
        result["statistics"] = {{"maxCombo", s.maxCombo},
                                {"timedHits", s.timedHits},
                                {"meanErrorMs", s.meanErrorMs},
                                {"unstableRate", s.unstableRate}};
        if (s.histogram) result["statistics"]["hitErrorHistogram"] = *s.histogram;
    }
    return result;
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
        r.hasReplay = obj.value("hasReplay", false);
        if (obj.contains("statistics")) {
            const auto& s = obj.at("statistics");
            if (!s.is_object() || !s.at("maxCombo").is_number_integer() ||
                !s.at("timedHits").is_number_integer()) return false;
            r.statistics = core::RunStatistics{
                s.at("maxCombo").get<int>(), s.at("timedHits").get<int>(),
                s.at("meanErrorMs").get<double>(), s.at("unstableRate").get<double>()};
            if (s.contains("hitErrorHistogram")) {
                const auto& bins = s.at("hitErrorHistogram");
                if (!bins.is_array() || bins.size()!=core::kHitErrorBinCount)
                    return false;
                core::HitErrorHistogram histogram{};
                for (std::size_t i=0;i<histogram.size();++i) {
                    if (!bins[i].is_number_integer()) return false;
                    const auto value = bins[i].get<std::int64_t>();
                    if (value<0 || value>2000000) return false;
                    histogram[i]=static_cast<int>(value);
                }
                r.statistics->histogram=histogram;
            }
        }
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
    if (r.statistics) {
        const auto& s = *r.statistics;
        const int hits = r.notesJudged - r.grades.miss;
        // At most miss+1 streaks; a nonempty run needs a nonempty best streak.
        const int minCombo = (hits + r.grades.miss) / (r.grades.miss + 1);
        constexpr double limitMs = core::kBadWindowSeconds * 1000.0;
        // Historical rules may have different windows; only apply the
        // current window bound to results with the current rules revision.
        if (s.timedHits != hits || s.maxCombo < minCombo || s.maxCombo > hits ||
            !std::isfinite(s.meanErrorMs) || !std::isfinite(s.unstableRate) ||
            s.unstableRate < 0.0 ||
            (s.timedHits == 0 && s.meanErrorMs != 0.0) ||
            (s.timedHits < 2 && s.unstableRate != 0.0) ||
            (r.judgementVersion == core::kJudgementRulesVersion &&
             (std::abs(s.meanErrorMs) > limitMs + 1e-6 ||
              s.unstableRate > 10.0 * limitMs + 1e-6))) return false;
        if (s.histogram) {
            std::int64_t sum=0;
            for (int count : *s.histogram) {
                if (count<0 || count>s.timedHits) return false;
                sum+=count;
            }
            if (sum!=s.timedHits) return false;
        }
    }
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
    record.statistics = completion.statistics;
    if (!store.Save(record, error, completion.replay)) return false;
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

bool RunResultsStore::Save(RunResult& r, std::string& error,
                           const core::ReplayTrace* replay) const {
    // Do not copy a reference to another attempt's replay. Serialize once
    // before allocating an ID. Failure to record must not lose a valid score.
    r.hasReplay = false;
    std::string replayBytes;
    if (replay && replay->judgementVersion == r.judgementVersion &&
        !replay->events.empty()) {
        try { core::EncodeReplay(*replay, replayBytes); }
        catch (...) { replayBytes.clear(); }
    }
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
        const fs::path replayPath = dir / (fileId + ".locus-replay");
        if (!replayBytes.empty()) {
            // Avoid briefly attaching a new sidecar to an already committed
            // score whose original sidecar was removed. Exclusive publication
            // remains authoritative for concurrent new writers.
            std::error_code probe;
            if (fs::exists(dest,probe)) continue;
            // Exclusive sidecar claim before summary publication.
            const auto replayStatus = AtomicCreateTextFile(
                replayPath, replayBytes, error);
            if (replayStatus == AtomicTextWriteStatus::AlreadyExists) continue;
            candidate.hasReplay = replayStatus == AtomicTextWriteStatus::Published;
        }
        // No exists()/ofstream gap: the CREATE_NEW/link publication is
        // exclusive even if two processes picked the same timestamp and ID.
        // Abandoned legacy .pending files must not block new attempts.
        const auto status = AtomicCreateTextFile(
            dest, Encode(candidate).dump(2), error);
        if (status != AtomicTextWriteStatus::Published) {
            if (candidate.hasReplay) {
                std::error_code cleanup;
                fs::remove(replayPath, cleanup); // only our exclusive claim
            }
            if (status == AtomicTextWriteStatus::AlreadyExists) continue;
            return false;
        }
        r.id = candidate.id;
        r.hasReplay = candidate.hasReplay;
        error.clear();
        return true;
    }
    error = "Could not allocate a unique result filename.";
    return false;
}

bool RunResultsStore::LoadReplay(const RunResult& run, core::ReplayTrace& replay,
                                 std::string& error) const {
    error.clear();
    if (!run.hasReplay || !ValidRunResult(run)) {
        error = "No replay for this attempt.";
        return false;
    }
    const fs::path path = Directory({run.songSetId,run.difficultyId}) /
                          (run.id + ".locus-replay");
    std::error_code ec;
    if (fs::symlink_status(path,ec).type() != fs::file_type::regular || ec) {
        error = "Replay is missing or inaccessible.";
        return false;
    }
    const auto size = fs::file_size(path,ec);
    if (ec || size < core::kReplayHeaderBytes || size > core::kMaxReplayBytes) {
        error = "Invalid replay file size.";
        return false;
    }
    try {
        std::ifstream in(path, std::ios::binary);
        std::string bytes(static_cast<std::size_t>(size),'\0');
        core::ReplayTrace decoded;
        if (!in.read(bytes.data(),static_cast<std::streamsize>(bytes.size())) ||
            in.peek() != std::char_traits<char>::eof() ||
            !core::DecodeReplay(bytes,decoded) ||
            decoded.judgementVersion != run.judgementVersion) {
            error = "Invalid or incompatible replay.";
            return false;
        }
        replay = std::move(decoded);
        return true;
    } catch (const std::exception&) {
        error = "Cannot read replay.";
        return false;
    }
}

bool VerifyRunReplay(const RunResult& run, const core::ReplayTrace& replay,
                     const core::Chart& chart,
                     const std::string& loadedGameplayRevision) {
    if (!ValidRunResult(run) ||
        CompareRunRevision(run, loadedGameplayRevision) != RunRevisionState::Current)
        return false;
    core::TurnJudge judge(chart);
    if (!core::RejudgeReplay(replay,judge) || !judge.AllScorableJudged())
        return false;
    const auto& a = judge.Counts();
    const auto& b = run.grades;
    if (a.perfect != b.perfect || a.good != b.good ||
        a.bad != b.bad || a.miss != b.miss ||
        a.Resolved() != run.notesJudged) return false;
    if (run.statistics) {
        const auto actual = judge.Statistics();
        const auto& expected = *run.statistics;
        if (actual.maxCombo != expected.maxCombo ||
            actual.timedHits != expected.timedHits ||
            std::abs(actual.meanErrorMs - expected.meanErrorMs) > 1e-6 ||
            std::abs(actual.unstableRate - expected.unstableRate) > 1e-6 ||
            (expected.histogram && actual.histogram != expected.histogram))
            return false;
    }
    return true;
}

std::vector<RunResult> RunResultsStore::Load(const RunKey& key,
                                             std::string& error,
                                             const std::atomic<bool>* cancelled) const {
    std::vector<RunResult> records;
    error.clear();
    if (key.Empty()) return records;
    const fs::path dir = Directory(key);
    std::error_code ec;
    // One non-following status probe distinguishes missing history from a
    // regular file, a symlink, or an inaccessible directory. Combining
    // exists() + is_directory() can lose the actual error between probes
    // on Windows and follows links before validating their location.
    const fs::file_status state = fs::symlink_status(dir, ec);
    if (state.type() == fs::file_type::not_found &&
        (!ec || ec == std::errc::no_such_file_or_directory))
        return records; // A difficulty with no attempts is ordinary.
    if (ec) {
        error = "Cannot inspect saved results: " + ec.message();
        return records;
    }
    if (state.type() != fs::file_type::directory) {
        error = "Saved results path is not an accessible directory.";
        return records;
    }
    // An unreadable folder or failed traversal is NOT an empty history.
    // Never publish an incomplete result list after an iterator failure.
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
