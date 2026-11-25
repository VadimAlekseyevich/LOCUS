#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/chart_file.h"
#include "app/chart_file_fingerprint.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"
#include "core/chart_revision.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <system_error>
#include <utility>
namespace locus::app {
namespace fs = std::filesystem;
using namespace detail;
namespace {
// Both inputs were canonicalized by the OS. Fast path for ordinary installed
// files: component prefix comparison is sufficient to establish containment
// without re-opening each ancestor directory for every difficulty. Unusual
// case/UNC spelling variants still use the existing identity-based fallback.
bool CanonicalChildOf(const fs::path& parent, const fs::path& child) {
    auto a = parent.begin(), b = child.begin();
    for (; a != parent.end(); ++a, ++b) {
        if (b == child.end() || *a != *b) return false;
    }
    return b != child.end();
}
}

bool SongLibrary::Refresh(std::string& message,
                          const std::atomic<bool>* cancelled) {
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
    // File reuse requires size, timestamp AND a content fingerprint, so an
    // external same-size / same-mtime edit cannot silently reuse stale notes.
    // Song Set timing remains authoritative in the manifest.
    struct PriorChart { const LibraryChart* chart; const LibrarySong* song; };
    std::unordered_map<fs::path,PriorChart> previous;
    std::size_t knownChartCount = 0;
    for (const auto& song : songs_) knownChartCount += song.charts.size();
    previous.reserve(knownChartCount);
    for (const auto& song : songs_)
        for (const auto& chart : song.charts)
            previous.emplace(chart.file,PriorChart{&chart,&song});
    LibraryRefreshStats stats;
    std::unordered_map<fs::path, SongSetManifest> manifestCache;
    std::unordered_map<fs::path, fs::path> canonicalAudioCache;
    std::vector<LibrarySong> next;
    std::unordered_map<fs::path, std::size_t> byAudio;
    std::unordered_map<std::string, fs::path> setIdOwners;
    // Expected constant-time duplicate-ID checks, even for very large sets.
    std::unordered_map<fs::path, std::unordered_set<std::string>> difficultyIds;
    manifestCache.reserve(songs_.size());
    canonicalAudioCache.reserve(songs_.size());
    next.reserve(songs_.size());
    byAudio.reserve(songs_.size());
    setIdOwners.reserve(songs_.size());
    difficultyIds.reserve(songs_.size());
    std::size_t skipped = 0;
    // An unreadable subtree is not an ignorable invalid chart: silently
    // skipping it would publish an incomplete library as authoritative.
    fs::recursive_directory_iterator entry(root, fs::directory_options::none, ec);
    const fs::recursive_directory_iterator end;
    if (ec) {
        message = "Cannot enumerate Songs folder: " + ec.message();
        return false;
    }
    // Never publish a truncated catalog as a successful refresh. Historical
    // backups are not playable difficulties and should not consume scan time.
    while (entry != end) {
        if (cancelled && cancelled->load(std::memory_order_relaxed)) {
            message = "Library scan cancelled.";
            return false;
        }
        const fs::directory_entry current = *entry;
        const fs::path path = current.path();
        if (path.filename() == L".locus-history" &&
            current.is_directory(ec) && !ec)
            entry.disable_recursion_pending();
        ec.clear();
        entry.increment(ec);
        if (ec) {
            message = "Songs directory traversal failed; previous library index "
                      "was preserved: " + ec.message();
            return false;
        }
        if (path.extension() != L".json" ||
            path.filename() == L"set.locus.json") continue;
        if (current.is_symlink(ec) || !current.is_regular_file(ec)) {
            ++skipped;
            ec.clear();
            continue;
        }
        const fs::path manifestPath = path.parent_path() / L"set.locus.json";
        // Unsupported folders are not part of the active library.
        // Check the manifest before expensive chart I/O or fingerprints.
        // Check manifest existence only on the first chart in a set; the
        // cached manifest is revalidated at the end of the full scan.
        if (manifestCache.find(manifestPath) == manifestCache.end()) {
            const bool managed = fs::exists(manifestPath, ec);
            if (ec || !managed) { ++skipped; ec.clear(); continue; }
        }
        // One manifest per Song Set: borrow the parsed tempo map and tags
        // rather than copying them for each difficulty in a large pack.
        auto manifest = manifestCache.find(manifestPath);
        if (manifest == manifestCache.end()) {
            SongSetManifest parsed;
            std::string problem;
            if (!LoadSongSetManifest(manifestPath, parsed, problem)) {
                ++skipped;
                continue;
            }
            manifest = manifestCache.emplace(
                manifestPath, std::move(parsed)).first;
        }
        const SongSetManifest& metadata = manifest->second;
        // Published metadata must identify exact bytes; failed probes
        // must not publish a playable but unverifiable difficulty.
        const auto timestamp = current.last_write_time(ec);
        const bool hasTimestamp = !ec;
        ec.clear();
        const auto bytes = current.file_size(ec);
        const bool hasMetadata = hasTimestamp && !ec;
        ec.clear();
        LibraryChart indexed;
        const auto old = previous.find(path);
        const auto* prior = old == previous.end() ? nullptr : old->second.chart;
        const bool reuseCandidate = hasMetadata && prior && prior->HasPayload() &&
            prior->indexedWriteTime == timestamp &&
            prior->indexedFileSize == bytes &&
            prior->songSetId == metadata.id &&
            old->second.song->tempoMap == metadata.tempoMap &&
            prior->difficulty.notes == prior->metadata.noteCount &&
            std::isfinite(prior->difficulty.overall) &&
            (prior->metadata.noteCount == 0 || prior->difficulty.overall > 0.0);
        std::uint64_t contentFingerprint = 0;
        std::uintmax_t fingerprintBytes = 0;
        // Only a potentially reusable entry needs a separate streaming
        // fingerprint. New/known-dirty charts are hashed in the parse buffer.
        const bool probed = reuseCandidate &&
            ChartFileFingerprint(path,contentFingerprint,&fingerprintBytes);
        stats.fingerprintBytes += fingerprintBytes;
        const bool reuse = probed && fingerprintBytes == bytes &&
            contentFingerprint == prior->indexedContentFingerprint;
        core::LoadedChart chart; // scoped to this one file, never put in next
        if (reuse) {
            indexed = *prior;
        } else {
            std::wstring audio;
            std::string problem;
            std::uint64_t parsedFingerprint = 0;
            std::size_t parsedBytes = 0;
            const bool opened = OpenChartWithAudioPath(path.wstring(),chart,audio,problem,
                nullptr,&parsedFingerprint,&parsedBytes);
            stats.parsedBytes += parsedBytes;
            // If a candidate changed between its streaming probe and
            // actual read, don't publish a mixed-generation index.
            if (probed && parsedBytes != 0 &&
                (parsedFingerprint != contentFingerprint || parsedBytes != fingerprintBytes)) {
                message = "Difficulty changed during scan; retry refresh.";
                return false;
            }
            if (!opened) {
                ++skipped;
                continue;
            }
            // Verify the parsed buffer belongs to the observed file
            // signature. Unlike directory_entry's potentially cached
            // metadata, these queries are made after the read.
            const auto finalBytes = fs::file_size(path,ec);
            const bool validSize = !ec && finalBytes == bytes && parsedBytes == bytes;
            ec.clear();
            const auto finalTimestamp = fs::last_write_time(path,ec);
            if (!hasMetadata || !validSize || ec || finalTimestamp != timestamp) {
                message = "Difficulty changed during scan; retry refresh.";
                return false;
            }
            contentFingerprint = parsedFingerprint;
            indexed.file = path;
            indexed.RefreshMetadata(chart);
            indexed.audioFile = fs::path(audio);
            ++stats.parsedCharts;
            stats.peakParsedNotes=std::max(stats.peakParsedNotes,chart.route.turns.size());
        }
        indexed.songSetId = metadata.id;
        indexed.indexedFingerprintValid = true;
        indexed.indexedWriteTime = timestamp;
        indexed.indexedFileSize = bytes;
        indexed.indexedContentFingerprint = contentFingerprint;
        // A Song Set can contain thousands of charts referencing the SAME
        // audio file. Canonicalizing it through the OS per chart dominates a
        // warm scan; resolve each distinct source path only once. Revalidate
        // the shared manifest audio at the end, guarding against an external
        // symlink/junction replacement during this potentially long scan.
        fs::path audioFile;
        const auto resolved = canonicalAudioCache.find(indexed.audioFile);
        if (resolved == canonicalAudioCache.end()) {
            ++stats.resolvedAudioPaths;
            audioFile = fs::canonical(indexed.audioFile, ec);
        } else {
            audioFile = resolved->second;
        }
        if (ec || (!CanonicalChildOf(root, audioFile) &&
                   !Inside(root, audioFile))) {
            ++skipped;
            ec.clear();
            continue;
        }
        if (resolved == canonicalAudioCache.end())
            canonicalAudioCache.emplace(indexed.audioFile, audioFile);
        indexed.audioFile = audioFile;

        const fs::path expected = path.parent_path() /
            fs::u8path(metadata.audioFile);
        // The usual relative chart audio path resolves to exactly the shared
        // filename. Keep equivalent() for hardlinks/alias edge cases only.
        if (expected.lexically_normal() != audioFile &&
            (!fs::equivalent(expected, audioFile, ec) || ec)) {
            ++skipped;
            ec.clear();
            continue;
        }
        // Missing IDs are pre-release charts which cannot safely join a
        // filename-independent results history. Do not index them as playable.
        if (!core::ValidDifficultyId(indexed.metadata.difficultyId)) {
            ++skipped;
            continue;
        }
        // Derive summaries and release authored notes at iteration end.
        if (!reuse) {
            std::string problem;
            if (!ApplySongTiming(chart,metadata.tempoMap,problem)) {
                message = "Song Set " + metadata.title + ": " + problem;
                return false;
            }
            indexed.difficulty=core::CalculateChartDifficulty(chart.route,chart.offsetSeconds);
            indexed.gameplayRevision=core::ChartGameplayRevision(chart);
            ++stats.calculatedDifficulties;
        } else ++stats.reusedCharts;
        const fs::path logicalAudio = path.parent_path();
        const auto found = byAudio.find(logicalAudio);
        std::size_t index = 0;
        if (found == byAudio.end()) {
            const auto owner = setIdOwners.emplace(metadata.id, logicalAudio);
            if (!owner.second && owner.first->second != logicalAudio) {
                message = "Duplicate Song Set ID in multiple folders: " + metadata.id;
                return false;
            }
            index = next.size();
            byAudio.emplace(logicalAudio, index);
            next.push_back({audioFile, metadata.title, {}});
            next.back().id = metadata.id;
            next.back().artist = metadata.artist;
            next.back().mapper = metadata.mapper;
            next.back().tags = metadata.tags;
            next.back().folder = path.parent_path();
            next.back().managed = true;
            next.back().previewStartMs = metadata.previewStartMs;
            next.back().bpm = *metadata.bpm;
            next.back().offsetMs = *metadata.offsetMs;
            next.back().tempoMap = metadata.tempoMap;
            next.back().backgroundImage = !metadata.backgroundFile.empty()
                ? path.parent_path() / fs::u8path(metadata.backgroundFile) : fs::path{};
        } else {
            index = found->second;
        }
        const auto& chartId = indexed.metadata.difficultyId;
        if (!difficultyIds[logicalAudio].insert(chartId).second) {
            message = "Multiple files in a Song Set claim the same difficultyId: " + chartId;
            return false;
        }
        next[index].charts.push_back(std::move(indexed));
    }
    for (auto& song : next) {
        if (cancelled && cancelled->load(std::memory_order_relaxed)) {
            message = "Library scan cancelled.";
            return false;
        }
        SongSetManifest timing;
        std::string problem;
        if (!LoadSongSetManifest(song.folder / L"set.locus.json",
                                 timing, problem)) {
            message = "Song Set timing could not be read: " + problem;
            return false;
        }
        const std::string artwork = song.backgroundImage.empty()
            ? std::string() : song.backgroundImage.filename().u8string();
        if (timing.id != song.id ||
            timing.audioFile != song.audioFile.filename().u8string() ||
            timing.title != song.title || timing.artist != song.artist ||
            timing.mapper != song.mapper || timing.tags != song.tags ||
            timing.previewStartMs != song.previewStartMs ||
            timing.tempoMap != song.tempoMap ||
            timing.backgroundFile != artwork) {
            message = "Song Set metadata changed during scan; retry refresh.";
            return false;
        }
        const fs::path expectedAudio = song.folder / fs::u8path(timing.audioFile);
        const fs::path currentAudio = fs::canonical(expectedAudio, ec);
        if (ec || (currentAudio != song.audioFile &&
                   (!fs::equivalent(currentAudio, song.audioFile, ec) || ec))) {
            message = "Song Set audio location changed during scan; retry refresh.";
            return false;
        }
        SortChartDifficulties(song.charts);
    }
    std::sort(next.begin(), next.end(),
              [](const LibrarySong& a, const LibrarySong& b) {
                  if (a.title != b.title) return a.title < b.title;
                  return a.audioFile < b.audioFile;
              });
    if (cancelled && cancelled->load(std::memory_order_relaxed)) {
        message = "Library scan cancelled.";
        return false;
    }
    songs_ = std::move(next);
    ++revision_;
    lastRefreshStats_ = stats;
    message = "LIBRARY: " + std::to_string(songs_.size()) + " SONGS / " +
              std::to_string(ChartCount()) + " CHARTS";
    if (skipped) message += " (" + std::to_string(skipped) + " SKIPPED)";
    return true;
}

} // namespace locus::app
