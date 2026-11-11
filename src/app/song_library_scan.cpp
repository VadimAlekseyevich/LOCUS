#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/chart_file.h"
#include "app/audio_revision.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <system_error>
#include <utility>
namespace locus::app {
namespace fs = std::filesystem;
using namespace detail;
bool SongLibrary::Refresh(std::string& message) {
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec) {
        message = "Cannot create Songs library: " + ec.message();
        return false;
    }
    const fs::path root = fs::canonical(root_, ec);
    if (ec) {
        message = "Cannot open Songs library: " + ec.message();
        return false;
    }

    // Keep the last published index untouched until this scan fully succeeds.
    // File identity is checked by both last-write timestamp and byte length;
    // Song Set timing is resolved separately from its authoritative manifest.
    std::map<fs::path, const LibraryChart*> previous;
    for (const auto& song : songs_)
        for (const auto& chart : song.charts)
            previous.emplace(chart.file, &chart);
    LibraryRefreshStats stats;
    std::set<fs::path> reusedFiles;
    std::map<fs::path, SongSetManifest> manifestCache;
    std::vector<LibrarySong> next;
    std::map<fs::path, std::size_t> byAudio; // Logical song, not specific revision.
    std::size_t skipped = 0;
    std::size_t scanned = 0;
    fs::recursive_directory_iterator entry(
        root, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    if (ec) {
        message = "Cannot enumerate Songs folder: " + ec.message();
        return false;
    }
    while (entry != end && scanned < 8192) {
        const fs::directory_entry current = *entry;
        entry.increment(ec);
        if (ec) {
            ++skipped;
            ec.clear();
        }
        const fs::path path = current.path();
        if (path.extension() != L".json" ||
            path.filename() == L"set.locus.json") continue;
        ++scanned;
        if (current.is_symlink(ec) || !current.is_regular_file(ec)) {
            ++skipped;
            ec.clear();
            continue;
        }
        // A filesystem metadata probe is only a cache hint. If the probe
        // fails, still try parsing the chart normally instead of losing it.
        const auto timestamp = current.last_write_time(ec);
        const bool hasTimestamp = !ec;
        ec.clear();
        const auto bytes = current.file_size(ec);
        const bool hasFingerprint = hasTimestamp && !ec;
        ec.clear();

        LibraryChart indexed;
        const auto old = previous.find(path);
        const bool reuse = hasFingerprint && old != previous.end() &&
            old->second->indexedFingerprintValid &&
            old->second->indexedWriteTime == timestamp &&
            old->second->indexedFileSize == bytes;
        if (reuse) {
            indexed = *old->second;
            reusedFiles.insert(path);
            ++stats.reusedCharts;
        } else {
            core::LoadedChart chart;
            std::wstring audio;
            std::string problem;
            if (!OpenChartWithAudioPath(path.wstring(), chart, audio, problem)) {
                ++skipped;
                continue;
            }
            indexed.file = path;
            indexed.data = std::move(chart);
            indexed.audioFile = fs::path(audio);
            ++stats.parsedCharts;
        }
        indexed.indexedFingerprintValid = hasFingerprint;
        if (hasFingerprint) {
            indexed.indexedWriteTime = timestamp;
            indexed.indexedFileSize = bytes;
        }
        const fs::path audioFile = fs::canonical(indexed.audioFile, ec);
        if (ec || !Inside(root, audioFile)) {
            ++skipped;
            ec.clear();
            reusedFiles.erase(path);
            continue;
        }
        indexed.audioFile = audioFile;

        // Managed Song Sets are grouped by their folder, not by incidental
        // audio-revision paths. Existing legacy folders remain untouched.
        const fs::path manifestPath = path.parent_path() / L"set.locus.json";
        const bool managed = fs::exists(manifestPath, ec);
        if (ec) { ++skipped; ec.clear(); continue; }
        SongSetManifest metadata;
        if (managed) {
            const auto manifest = manifestCache.find(manifestPath);
            if (manifest == manifestCache.end()) {
                std::string problem;
                if (!LoadSongSetManifest(manifestPath, metadata, problem)) {
                    ++skipped;
                    continue;
                }
                manifestCache.emplace(manifestPath, metadata);
            } else {
                metadata = manifest->second;
            }
            const fs::path expected = path.parent_path() /
                fs::u8path(metadata.audioFile);
            if (!fs::equivalent(expected, audioFile, ec) || ec) {
                ++skipped;
                ec.clear();
                continue;
            }
        }
        const fs::path logicalAudio = managed
            ? path.parent_path() : LogicalSongAudioPath(audioFile);
        const auto found = byAudio.find(logicalAudio);
        std::size_t index = 0;
        if (found == byAudio.end()) {
            index = next.size();
            byAudio.emplace(logicalAudio, index);
            const std::string title = managed
                ? (metadata.artist.empty() ? metadata.title
                    : metadata.artist + " - " + metadata.title)
                : SongDisplayTitle(logicalAudio);
            next.push_back({audioFile, title, {}});
            next.back().id = managed ? metadata.id : HashText(logicalAudio);
            next.back().artist = managed ? metadata.artist : "";
            next.back().folder = path.parent_path();
            next.back().managed = managed;
            next.back().previewStartMs = managed ? metadata.previewStartMs : 0;
            next.back().backgroundImage = managed && !metadata.backgroundFile.empty()
                ? path.parent_path() / fs::u8path(metadata.backgroundFile) : fs::path{};
        } else {
            index = found->second;
        }
        next[index].charts.push_back(std::move(indexed));
    }
    if (scanned >= 8192) ++skipped;
    for (auto& song : next) {
        if (song.managed) {
            SongSetManifest timing;
            std::string problem;
            if (!LoadSongSetManifest(song.folder / L"set.locus.json",
                                     timing, problem)) {
                message = "Song Set timing could not be read: " + problem;
                return false;
            }
            // For older sets, choose a stable source chart by filename.
            // Existing chart files and their absolute timestamps are intact.
            const auto first = std::min_element(song.charts.begin(),
                song.charts.end(), [](const LibraryChart& a, const LibraryChart& b) {
                    return a.file < b.file;
                });
            if (first != song.charts.end()) {
                song.bpm = timing.bpm.value_or(first->data.route.bpm);
                song.offsetMs = timing.offsetMs.value_or(
                    static_cast<int>(std::llround(
                        first->data.offsetSeconds * 1000.0)));
                song.tempoMap = !timing.tempoMap.empty()
                    ? timing.tempoMap
                    : std::vector<core::TempoMarker>{{song.offsetMs, song.bpm}};
                for (auto& difficulty : song.charts) {
                    if (!ApplySongTiming(difficulty.data, song.tempoMap,
                                         problem)) {
                        message = "Song Set " + song.title + ": " + problem;
                        return false;
                    }
                }
            }
        }
        for (auto& difficulty : song.charts) {
            // The cached difficulty belongs to a particular audio timing,
            // not just to the original JSON bytes. A changed manifest BPM
            // or offset may reproject every note of this Song Set.
            // ApplySongTiming above has already overwritten the in-memory
            // values, so compare with the pre-scan cached version instead.
            const auto previousChart = previous.find(difficulty.file);
            const bool oldTimingMatches = !song.managed ||
                (previousChart != previous.end() &&
                 core::EffectiveTempoMap(
                     previousChart->second->data.route,
                     previousChart->second->data.offsetSeconds) ==
                     song.tempoMap);
            // Old in-memory indices may contain the zero-rating result
            // produced by the former audio-duration-dependent calculator.
            // Re-evaluate just those entries on Refresh, while preserving
            // the fast path for valid unchanged files and song timing.
            const bool invalidCachedRating = !difficulty.data.route.turns.empty() &&
                (difficulty.difficulty.notes != difficulty.data.route.turns.size() ||
                 !std::isfinite(difficulty.difficulty.overall) ||
                 difficulty.difficulty.overall <= 0.0);
            if (!oldTimingMatches || reusedFiles.count(difficulty.file) == 0 ||
                invalidCachedRating) {
                difficulty.difficulty = core::CalculateChartDifficulty(
                    difficulty.data.route, difficulty.data.offsetSeconds);
                ++stats.calculatedDifficulties;
            }
        }
        SortChartDifficulties(song.charts);
    }
    std::sort(next.begin(), next.end(),
              [](const LibrarySong& a, const LibrarySong& b) {
                  if (a.title != b.title) return a.title < b.title;
                  return a.audioFile < b.audioFile;
              });
    songs_ = std::move(next);
    lastRefreshStats_ = stats;
    message = "LIBRARY: " + std::to_string(songs_.size()) + " SONGS / " +
              std::to_string(ChartCount()) + " CHARTS";
    if (skipped) message += " (" + std::to_string(skipped) + " SKIPPED)";
    return true;
}

} // namespace locus::app
