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
#include <string>
#include <system_error>
#include <utility>
namespace locus::app {
namespace fs = std::filesystem;
using namespace detail;
bool SongLibrary::SetSongTiming(int songIndex, double bpm, int offsetMs,
                                const core::LoadedChart* currentDraft,
                                std::string& error) {
    if (songIndex < 0 || songIndex >= static_cast<int>(songs_.size()) ||
        !songs_[static_cast<std::size_t>(songIndex)].managed) {
        error = "Choose a managed Song Set before changing BPM/offset.";
        return false;
    }
    auto& song = songs_[static_cast<std::size_t>(songIndex)];
    if (song.bpm == bpm && song.offsetMs == offsetMs) {
        error.clear();
        return true;
    }
    std::vector<core::LoadedChart> adjusted;
    adjusted.reserve(song.charts.size());
    for (const auto& difficulty : song.charts) {
        core::LoadedChart candidate = difficulty.data;
        if (!ApplySongTiming(candidate, bpm, offsetMs, error))
            return false;
        adjusted.push_back(std::move(candidate));
    }
    if (currentDraft) {
        core::LoadedChart candidate = *currentDraft;
        if (!ApplySongTiming(candidate, bpm, offsetMs, error)) return false;
    }
    FolderWriteLock lock;
    if (!lock.Acquire(song.folder, error)) return false;
    SongSetManifest metadata;
    const auto manifestFile = song.folder / L"set.locus.json";
    if (!LoadSongSetManifest(manifestFile, metadata, error)) return false;
    if (metadata.id != song.id) {
        error = "Song Set changed on disk; refresh the library.";
        return false;
    }
    metadata.bpm = bpm;
    metadata.offsetMs = offsetMs;
    if (!ReplaceSongSetManifest(manifestFile, metadata, error)) return false;
    song.bpm = bpm;
    song.offsetMs = offsetMs;
    for (std::size_t i = 0; i < adjusted.size(); ++i) {
        song.charts[i].data = std::move(adjusted[i]);
        song.charts[i].difficulty = core::CalculateChartDifficulty(
            song.charts[i].data.route, song.charts[i].data.offsetSeconds);
    }
    SortChartDifficulties(song.charts); // Also keep order after shared timing edits.
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
