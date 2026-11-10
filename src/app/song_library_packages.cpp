#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/song_archive.h"
#include "app/chart_file.h"
#include "app/song_set_storage.h"
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
bool SongLibrary::ExportPackage(const fs::path& setFolder,
                                const fs::path& archive,
                                std::string& error) {
    const auto selected = std::find_if(songs_.begin(), songs_.end(),
        [&](const LibrarySong& song) {
            return song.managed && song.folder == setFolder;
        });
    if (selected == songs_.end()) {
        error = "Choose an installed managed Song Set to export.";
        return false;
    }
    std::error_code ec;
    const fs::path root = fs::canonical(root_, ec);
    if (ec) { error = "Cannot access Songs folder."; return false; }
    FolderWriteLock libraryLock, setLock;
    if (!libraryLock.Acquire(root, error) ||
        !setLock.Acquire(setFolder, error)) return false;
    std::vector<fs::path> charts;
    for (const auto& chart : selected->charts) {
        core::LoadedChart parsed;
        std::wstring audio;
        if (!OpenChartWithAudioPath(chart.file.wstring(), parsed, audio, error))
            return false;
        if (!fs::equivalent(fs::path(audio), selected->audioFile, ec) || ec) {
            error = "Song Set has a difficulty using different audio.";
            return false;
        }
        charts.push_back(chart.file);
    }
    return ExportSongSetArchive(setFolder, charts, archive, error);
}

bool SongLibrary::ImportPackage(const fs::path& archive,
                                fs::path& installedSet,
                                std::string& error) {
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec) { error = "Cannot create Songs folder."; return false; }
    const fs::path root = fs::canonical(root_, ec);
    if (ec) { error = "Cannot open Songs folder."; return false; }
    FolderWriteLock libraryLock;
    if (!libraryLock.Acquire(root, error)) return false;

    // Untrusted archives are extracted OUTSIDE the indexed Songs directory
    // and are only published after all entries have been checked.
    const fs::path incoming = root.parent_path() / L"Incoming";
    fs::create_directories(incoming, ec);
    if (ec) { error = "Cannot stage incoming Song Set."; return false; }
    const fs::path stage = incoming /
        (L"import-" + std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!fs::create_directory(stage, ec) || ec) {
        error = "Cannot allocate incoming Song Set staging directory.";
        return false;
    }
    const auto fail = [&]() {
        std::error_code cleanup;
        fs::remove_all(stage, cleanup);
        return false;
    };
    if (!ExtractSongSetArchive(archive, stage, error)) return fail();
    SongSetManifest metadata;
    if (!LoadSongSetManifest(stage / L"set.locus.json", metadata, error))
        return fail();
    const fs::path audio = stage / fs::u8path(metadata.audioFile);
    if (!metadata.backgroundFile.empty()) {
        const fs::path art = stage / fs::u8path(metadata.backgroundFile);
        if (!ValidateImageAssetFile(art, error)) return fail();
    }
    if (!fs::is_regular_file(audio, ec) || ec) {
        error = "Package is missing the shared audio file.";
        return fail();
    }
    std::size_t difficultyCount = 0;
    for (fs::directory_iterator it(stage, ec), end; !ec && it != end;
         it.increment(ec)) {
        const fs::path path = it->path();
        if (!it->is_regular_file(ec) || ec) {
            error = "Package contains an unexpected directory or file type.";
            return fail();
        }
        if (path.filename() == L"set.locus.json" ||
            fs::equivalent(path, audio, ec)) continue;
        if (!metadata.backgroundFile.empty() &&
            path.filename() == fs::u8path(metadata.backgroundFile)) continue;
        if (path.extension() != L".json") {
            error = "Package contains unsupported files.";
            return fail();
        }
        core::LoadedChart parsed;
        std::wstring chartAudio;
        if (!OpenChartWithAudioPath(path.wstring(), parsed, chartAudio, error))
            return fail();
        if (!fs::equivalent(fs::path(chartAudio), audio, ec) || ec) {
            error = "Package difficulty does not use shared audio.";
            return fail();
        }
        ++difficultyCount;
    }
    if (ec || difficultyCount == 0) {
        error = "Package contains no valid difficulties.";
        return fail();
    }
    // Same setId means the source is already installed. Never replace it.
    for (const auto& installed : songs_) {
        if (installed.managed && installed.id == metadata.id) {
            error = "This Song Set is already installed; import a difficulty instead.";
            return fail();
        }
    }
    const fs::path target = root /
        (FilenamePart(metadata.title) + "-pack-" +
         HashSetId(metadata.id));
    if (fs::exists(target, ec) || ec) {
        error = "Song Set destination already exists; nothing was overwritten.";
        return fail();
    }
    fs::rename(stage, target, ec);
    if (ec) {
        error = "Cannot publish Song Set package: " + ec.message();
        return fail();
    }
    std::string notice;
    if (!Refresh(notice)) {
        error = notice;
        return false; // Files remain safely installed on disk for next rescan.
    }
    installedSet = target;
    error.clear();
    return true;
}

} // namespace locus::app
