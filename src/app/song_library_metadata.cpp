#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"
#include "app/image_asset_file.h"
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

namespace {
// Recheck the authoritative on-disk difficulties under the Song Set lock
// before publishing new shared BPM/offset. No cached validation may be
// applied to a chart replaced by another editor since the last refresh.
bool SameSongSetFiles(const LibrarySong& song, std::string& error) {
    const auto stale = [&]() {
        error = "Song Set files changed on disk; refresh the library before editing timing.";
        return false;
    };
    std::set<fs::path> indexed;
    std::error_code ec;
    for (const auto& chart : song.charts) {
        indexed.insert(chart.file);
        if (!chart.indexedFingerprintValid) return stale();
        const auto size = fs::file_size(chart.file, ec);
        if (ec || size != chart.indexedFileSize) return stale();
        const auto time = fs::last_write_time(chart.file, ec);
        if (ec || time != chart.indexedWriteTime) return stale();
    }

    // A new difficulty could have been added while our in-memory catalog
    // was stale. It must also be included in the timing compatibility check.
    fs::directory_iterator it(song.folder, ec), end;
    if (ec) return stale();
    for (; it != end; it.increment(ec)) {
        if (ec) return stale();
        const fs::path file = it->path();
        if (file.extension() != L".json" ||
            file.filename() == L"set.locus.json") continue;
        if (it->is_symlink(ec) || !it->is_regular_file(ec) ||
            indexed.count(file) == 0 || ec) return stale();
    }
    if (ec) return stale();
    return true;
}
} // namespace

bool SongLibrary::SetSongTiming(int songIndex, double bpm, int offsetMs,
                                const core::LoadedChart* currentDraft,
                                std::string& error) {
    return SetSongTempoMap(songIndex,
        {{static_cast<std::int64_t>(offsetMs), bpm}}, currentDraft, error);
}

bool SongLibrary::SetSongTempoMap(
    int songIndex, const std::vector<core::TempoMarker>& markers,
    const core::LoadedChart* currentDraft, std::string& error) {
    if (songIndex < 0 || songIndex >= static_cast<int>(songs_.size()) ||
        !songs_[static_cast<std::size_t>(songIndex)].managed) {
        error = "Choose a managed Song Set before editing BPM markers.";
        return false;
    }
    if (!core::ValidTempoMap(markers, error)) return false;
    auto& song = songs_[static_cast<std::size_t>(songIndex)];
    if (song.tempoMap == markers) {
        error.clear();
        return true;
    }
    std::vector<core::LoadedChart> adjusted;
    adjusted.reserve(song.charts.size());
    for (const auto& difficulty : song.charts) {
        core::LoadedChart candidate = difficulty.data;
        if (!ApplySongTiming(candidate, markers, error))
            return false;
        adjusted.push_back(std::move(candidate));
    }
    if (currentDraft) {
        core::LoadedChart candidate = *currentDraft;
        if (!ApplySongTiming(candidate, markers, error)) return false;
    }
    FolderWriteLock lock;
    if (!lock.Acquire(song.folder, error)) return false;
    if (!SameSongSetFiles(song, error)) return false;

    SongSetManifest manifest;
    const auto file = song.folder / L"set.locus.json";
    if (!LoadSongSetManifest(file, manifest, error)) return false;
    const auto diskMap = !manifest.tempoMap.empty()
        ? manifest.tempoMap
        : std::vector<core::TempoMarker>{{song.offsetMs, song.bpm}};
    if (manifest.id != song.id || diskMap != song.tempoMap) {
        error = "Song Set timing changed on disk; refresh the library.";
        return false;
    }
    manifest.bpm = markers.front().bpm;
    manifest.offsetMs = static_cast<int>(markers.front().timeMs);
    manifest.tempoMap = markers;
    if (!ReplaceSongSetManifest(file, manifest, error)) return false;
    song.tempoMap = markers;
    song.bpm = markers.front().bpm;
    song.offsetMs = static_cast<int>(markers.front().timeMs);
    for (std::size_t i = 0; i < adjusted.size(); ++i) {
        song.charts[i].data = std::move(adjusted[i]);
        song.charts[i].difficulty = core::CalculateChartDifficulty(
            song.charts[i].data.route, song.charts[i].data.offsetSeconds);
    }
    SortChartDifficulties(song.charts);
    error.clear();
    return true;
}

bool SongLibrary::SetPreviewStartMs(int songIndex, int timeMs,
                                    std::string& error) {
    if (songIndex < 0 || songIndex >= static_cast<int>(songs_.size()) ||
        !songs_[static_cast<std::size_t>(songIndex)].managed ||
        timeMs < 0 || timeMs > kMaxSongPreviewStartMs) {
        error = "Select a managed Song Set and a valid preview position.";
        return false;
    }
    auto& song = songs_[static_cast<std::size_t>(songIndex)];
    if (timeMs == song.previewStartMs) { error.clear(); return true; }
    FolderWriteLock lock;
    if (!lock.Acquire(song.folder, error)) return false;
    SongSetManifest manifest;
    const fs::path path = song.folder / L"set.locus.json";
    if (!LoadSongSetManifest(path, manifest, error)) return false;
    if (manifest.id != song.id) {
        error = "Song Set changed on disk; refresh the library."; return false;
    }
    manifest.previewStartMs = timeMs;
    if (!ReplaceSongSetManifest(path, manifest, error)) return false;
    song.previewStartMs = timeMs;
    return true;
}

bool SongLibrary::SetBackgroundImage(int songIndex, const fs::path& source,
                                     std::string& error) {
    if (songIndex < 0 || songIndex >= static_cast<int>(songs_.size()) ||
        !songs_[static_cast<std::size_t>(songIndex)].managed) {
        error = "Choose a managed Song Set before setting artwork.";
        return false;
    }
    if (!source.empty() && !ValidateImageAssetFile(source, error)) return false;
    auto& song = songs_[static_cast<std::size_t>(songIndex)];
    FolderWriteLock lock;
    if (!lock.Acquire(song.folder, error)) return false;
    SongSetManifest manifest;
    const fs::path metadataPath = song.folder / L"set.locus.json";
    if (!LoadSongSetManifest(metadataPath, manifest, error)) return false;
    if (manifest.id != song.id) {
        error = "Song Set changed on disk; refresh the library.";
        return false;
    }
    const std::string oldFile = manifest.backgroundFile;
    fs::path published;
    if (!source.empty()) {
        const auto stamp = std::chrono::steady_clock::now().
            time_since_epoch().count();
        std::string extension = source.extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        manifest.backgroundFile = "background-" +
            std::to_string(stamp) + extension;
        published = song.folder / fs::u8path(manifest.backgroundFile);
        std::error_code copyError;
        if (!fs::copy_file(source, published, fs::copy_options::none,
                           copyError) || copyError ||
            !ValidateImageAssetFile(published, error)) {
            std::error_code cleanup;
            fs::remove(published, cleanup);
            if (error.empty()) error = "Cannot copy Song Set background.";
            return false;
        }
    } else manifest.backgroundFile.clear();

    if (!ReplaceSongSetManifest(metadataPath, manifest, error)) {
        if (!published.empty()) {
            std::error_code cleanup;
            fs::remove(published, cleanup);
        }
        return false;
    }
    song.backgroundImage = published;
    // Delete only artwork that this Song Set owned and no longer references.
    if (!oldFile.empty() && oldFile != manifest.backgroundFile) {
        std::error_code cleanup;
        fs::remove(song.folder / fs::u8path(oldFile), cleanup);
    }
    error.clear();
    return true;
}

} // namespace locus::app
