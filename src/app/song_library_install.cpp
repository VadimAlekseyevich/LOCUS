#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/chart_file.h"
#include "app/chart_filename.h"
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
#include <string>
#include <system_error>
#include <utility>
namespace locus::app {
namespace fs = std::filesystem;
using namespace detail;
bool SongLibrary::ImportIntoSet(const fs::path& sourceChart,
                                 const fs::path& targetSet,
                                 fs::path& installedChart,
                                 std::string& error) {
    core::LoadedChart source;
    std::wstring audio;
    if (!OpenChartWithAudioPath(sourceChart.wstring(), source, audio, error))
        return false;
    return CreateDifficulty(fs::path(audio), source, installedChart,
                            error, targetSet);
}

bool SongLibrary::DuplicateDifficulty(const fs::path& original,
                                      fs::path& copied, std::string& error) {
    int song = -1, index = -1;
    if (!FindChart(original, song, index)) {
        error = "Difficulty is not in the library.";
        return false;
    }
    const auto& set = songs_[static_cast<std::size_t>(song)];
    if (!set.managed) {
        error = "Legacy folders must be reimported as a managed Song Set.";
        return false;
    }
    const auto& selected = set.charts[static_cast<std::size_t>(index)];
    core::LoadedChart copy = selected.data;
    copy.title += " Copy";
    return CreateDifficulty(selected.audioFile, copy, copied, error, set.folder);
}

bool SongLibrary::CreateDifficulty(const fs::path& sourceAudio,
                                   const core::LoadedChart& chart,
                                   fs::path& installedChart,
                                   std::string& error,
                                   const fs::path& targetSet) {
    std::error_code ec;
    const fs::path source = fs::canonical(sourceAudio, ec);
    if (ec || !fs::is_regular_file(source, ec)) {
        error = "Cannot read the selected song audio.";
        return false;
    }
    fs::create_directories(root_, ec);
    if (ec) { error = "Cannot create the song library."; return false; }
    const fs::path root = fs::canonical(root_, ec);
    if (ec) { error = "Cannot access the song library."; return false; }
    FolderWriteLock libraryLock;
    if (!libraryLock.Acquire(root, error)) return false;
    fs::path folder;
    if (targetSet.empty()) {
        folder = root / (FilenamePart(source.stem().u8string()) + "-" +
                         HashText(source));
        fs::create_directories(folder, ec);
        if (ec) { error = "Cannot create the Song Set folder."; return false; }
    } else {
        folder = fs::canonical(targetSet, ec);
        if (ec || !Inside(root, folder)) {
            error = "Selected Song Set is outside the installed library.";
            return false;
        }
    }
    fs::path result;
    {
        FolderWriteLock lock;
        if (!lock.Acquire(folder, error)) return false;
        const fs::path manifestFile = folder / L"set.locus.json";
        const bool exists = fs::exists(manifestFile, ec);
        if (ec) { error = "Cannot inspect Song Set metadata."; return false; }
        SongSetManifest metadata;
        if (exists) {
            if (!LoadSongSetManifest(manifestFile, metadata, error)) return false;
        } else {
            if (!targetSet.empty()) {
                error = "The selected folder has no managed Song Set.";
                return false;
            }
            // Make previously installed v1/v2 folders read-only: there can
            // be several different audio revisions in the old layout.
            if (fs::exists(folder / L".locus-versions", ec)) {
                error = "This legacy library folder cannot accept new difficulties.";
                return false;
            }
            metadata.id = HashText(source) + HashText(folder);
            metadata.title = SongDisplayTitle(source);
            metadata.artist.clear();
            metadata.audioFile = source.filename().u8string();
            metadata.bpm = chart.route.bpm;
            metadata.offsetMs = static_cast<int>(std::llround(
                chart.offsetSeconds * 1000.0));
        }
        core::LoadedChart installed = chart;
        if (metadata.bpm) {
            if (!ApplySongTiming(installed, *metadata.bpm,
                                 *metadata.offsetMs, error)) return false;
        } else {
            const auto other = std::find_if(songs_.begin(), songs_.end(),
                [&](const LibrarySong& s) {
                    return s.managed && s.folder == folder;
                });
            if (other != songs_.end() &&
                !ApplySongTiming(installed, other->bpm,
                                 other->offsetMs, error)) return false;
        }
        const fs::path audio = folder / fs::u8path(metadata.audioFile);
        if (!InstallSharedSongAudio(source, audio, error)) return false;

        const fs::path suggestion = SuggestedChartFileName(chart.title);
        // Never overwrite another difficulty, even when titles are the same.
        result = folder / suggestion;
        const std::wstring stem = suggestion.stem().wstring();
        for (int suffix = 2; fs::exists(result, ec) && suffix <= 10000; ++suffix) {
            result = folder / (stem + L"-" + std::to_wstring(suffix) +
                               L".locus.json");
        }
        if (ec || fs::exists(result, ec)) {
            error = "Cannot allocate a unique difficulty filename.";
            return false;
        }
        if (!SaveChartWithAudioPath(result.wstring(), audio.wstring(),
                                    installed, error, true)) return false;
        if (!exists && !WriteNewSongSetManifest(manifestFile, metadata, error))
            return false;
    }
    // Publish the in-memory library only after the complete operation.
    std::string notice;
    if (!Refresh(notice)) { error = notice; return false; }
    installedChart = result;
    error.clear();
    return true;
}

bool SongLibrary::Import(const fs::path& sourceChart,
                         fs::path& installedChart, std::string& error) {
    core::LoadedChart source;
    std::wstring audio;
    if (!OpenChartWithAudioPath(sourceChart.wstring(), source, audio, error))
        return false;
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec) { error = "Cannot create Songs folder: " + ec.message(); return false; }
    const auto root = fs::canonical(root_, ec);
    if (ec) { error = "Cannot access Songs folder."; return false; }
    FolderWriteLock libraryLock;
    if (!libraryLock.Acquire(root, error)) return false;
    const auto actualChart = fs::canonical(sourceChart, ec);
    if (ec) { error = "Cannot resolve source chart path."; return false; }
    if (Inside(root, actualChart)) {
        std::string notice;
        if (!Refresh(notice)) { error = notice; return false; }
        int s = -1, c = -1;
        if (!FindChart(actualChart, s, c)) {
            error = "Existing library chart could not be indexed.";
            return false;
        }
        installedChart = actualChart;
        error.clear();
        return true;
    }

    const auto sourceAudio = fs::canonical(fs::path(audio), ec);
    if (ec || !fs::is_regular_file(sourceAudio, ec)) {
        error = "Cannot access song audio for importing.";
        return false;
    }
    const auto folder = root /
        (FilenamePart(sourceAudio.stem().u8string()) + "-" + HashText(sourceAudio));
    fs::create_directories(folder, ec);
    if (ec) { error = "Cannot create song folder: " + ec.message(); return false; }

    FolderWriteLock lock;
    if (!lock.Acquire(folder, error)) return false;

    const fs::path manifestPath = folder / L"set.locus.json";
    const bool existingSet = fs::exists(manifestPath, ec);
    if (ec) { error = "Cannot inspect Song Set metadata."; return false; }
    // An older installed library (which may contain multiple audio versions)
    // must stay readable and must never be silently converted to one audio.
    const bool legacy = !existingSet &&
        fs::exists(folder / L".locus-versions", ec);
    if (ec) { error = "Cannot inspect legacy audio versions."; return false; }

    fs::path destinationAudio;
    SongSetManifest metadata;
    if (legacy) {
        if (!InstallAudioRevision(sourceAudio, folder, destinationAudio, error))
            return false;
    } else {
        if (existingSet) {
            if (!LoadSongSetManifest(manifestPath, metadata, error)) return false;
        } else {
            metadata.id = HashText(sourceAudio) + HashText(folder);
            metadata.title = SongDisplayTitle(sourceAudio);
            metadata.artist.clear();
            metadata.audioFile = sourceAudio.filename().u8string();
            metadata.bpm = source.route.bpm;
            metadata.offsetMs = static_cast<int>(std::llround(
                source.offsetSeconds * 1000.0));
        }
        if (metadata.audioFile != sourceAudio.filename().u8string()) {
            error = "Existing Song Set uses a different audio filename. "
                    "Import as a new set rather than changing other difficulties.";
            return false;
        }
        destinationAudio = folder / fs::u8path(metadata.audioFile);
        if (!InstallSharedSongAudio(sourceAudio, destinationAudio, error))
            return false;
    }

    // File identity is based on its source path, not a mutable difficulty name.
    const std::string difficulty = FilenamePart(actualChart.stem().u8string());
    installedChart = folder /
        (difficulty + "-" + HashText(actualChart) + ".locus.json");
    if (!legacy && existingSet) {
        const auto existing = std::find_if(songs_.begin(), songs_.end(),
            [&](const LibrarySong& s) { return s.managed && s.folder == folder; });
        if (metadata.bpm) {
            if (!ApplySongTiming(source, *metadata.bpm,
                                 *metadata.offsetMs, error)) return false;
        } else if (existing != songs_.end()) {
            if (!ApplySongTiming(source, existing->bpm,
                                 existing->offsetMs, error)) return false;
        }
    }
    if (!SaveChartWithAudioPath(installedChart.wstring(),
                                destinationAudio.wstring(), source, error, true))
        return false;
    // Metadata is published last. Interrupted imports still leave a valid
    // independently readable chart and never damage older difficulties.
    if (!legacy && !existingSet &&
        !WriteNewSongSetManifest(manifestPath, metadata, error)) return false;
    std::string notice;
    if (!Refresh(notice)) { error = notice; return false; }
    error.clear();
    return true;
}

} // namespace locus::app
