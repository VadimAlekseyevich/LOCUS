#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/song_archive.h"
#include "app/chart_file.h"
#include "app/song_set_storage.h"
#include "app/atomic_move.h"
#include "app/image_asset_file.h"
#include "app/song_difficulty_import.h"
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
    if (!SameSongSetFiles(*selected, error)) return false;
    // Never export a cached set identity against a changed manifest. The
    // authoritative timing and shared audio must match the indexed charts.
    SongSetManifest published;
    if (!LoadSongSetManifest(setFolder / L"set.locus.json", published, error))
        return false;
    if (published.id != selected->id ||
        published.audioFile != selected->audioFile.filename().u8string() ||
        published.tempoMap != selected->tempoMap) {
        error = "Song Set manifest changed on disk; refresh before exporting.";
        return false;
    }
    std::set<std::string> exportedIds;
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
        if (!core::ValidDifficultyId(parsed.difficultyId) ||
            parsed.difficultyId != chart.metadata.difficultyId ||
            !exportedIds.insert(parsed.difficultyId).second) {
            error = "Song Set chart IDs changed on disk; refresh before exporting.";
            return false;
        }
        charts.push_back(chart.file);
    }
    return ExportSongSetArchive(setFolder, charts, archive, error);
}

bool SongLibrary::ImportPackage(const fs::path& archive,
                                fs::path& installedSet,
                                std::string& error) {
    MarkDiskMutation();
    installedSet.clear();
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
    std::map<std::string, InstalledDifficulty> incomingCharts;
    std::set<std::string> incomingDifficultyIds;
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
        if (!core::ValidDifficultyId(parsed.difficultyId) ||
            !incomingDifficultyIds.insert(parsed.difficultyId).second) {
            error = "Package has a missing or duplicate difficultyId.";
            return fail();
        }
        const auto id = parsed.difficultyId;
        incomingCharts.emplace(id, InstalledDifficulty{path, std::move(parsed)});
    }
    if (ec || incomingCharts.empty()) {
        error = "Package contains no valid difficulties.";
        return fail();
    }
    fs::path existingSet;
    // Refresh indexes nested Song Sets as well as immediate children of
    // Songs/. Enforce exactly the same identity domain BEFORE publishing the
    // imported directory, even if the in-memory index is stale.
    for (fs::recursive_directory_iterator it(root, ec), end;
         !ec && it != end; it.increment(ec)) {
        const fs::path candidate = it->path();
        if (candidate.filename() == L".locus-history" &&
            it->is_directory(ec) && !ec) {
            it.disable_recursion_pending();
            continue;
        }
        if (candidate.filename() != L"set.locus.json") continue;
        if (it->is_symlink(ec)) {
            if (ec) break;
            error = "Existing Song Set manifest is a symlink; cannot verify IDs.";
            return fail();
        }
        if (!it->is_regular_file(ec)) {
            if (ec) break;
            continue;
        }
        SongSetManifest other;
        std::string ignored;
        if (!LoadSongSetManifest(candidate, other, ignored)) {
            error = "Cannot verify existing Song Set identity: " + ignored;
            return fail();
        }
        if (other.id == metadata.id) {
            if (!existingSet.empty()) {
                error = "Multiple installed Song Sets claim the same setId.";
                return fail();
            }
            existingSet = candidate.parent_path();
        }
    }
    if (ec) {
        error = "Cannot enumerate installed Song Set identities: " + ec.message();
        return fail();
    }
    const fs::path target = existingSet.empty() ? root /
        (FilenamePart(metadata.title) + "-pack-" + HashSetId(metadata.id))
        : existingSet;
    if (existingSet.empty()) {
        if (fs::exists(target, ec) || ec) {
            error = "Song Set destination already exists; nothing was overwritten.";
            return fail();
        }
        MovePathNoReplace(stage, target, ec);
        if (ec) {
            error = "Cannot publish Song Set package: " + ec.message();
            return fail();
        }
    } else {
        const auto resolvedTarget = fs::canonical(target, ec);
        if (ec || !Inside(root, resolvedTarget)) {
            error = "Installed Song Set resolves outside the library.";
            return fail();
        }
        FolderWriteLock setLock;
        if (!setLock.Acquire(target, error)) return fail();
        SongSetManifest installed;
        if (!LoadSongSetManifest(target / L"set.locus.json", installed, error))
            return fail();
        if (installed.id != metadata.id) {
            error = "Installed Song Set identity changed during import.";
            return fail();
        }
        const auto sharedAudio = target / fs::u8path(installed.audioFile);
        bool identical = false;
        if (!SameFileBytes(audio, sharedAudio, identical, error)) return fail();
        if (!identical) {
            error = "This Song Set already has different audio. Import was stopped "
                    "to protect other difficulties.";
            return fail();
        }
        std::vector<InstalledDifficulty> existing;
        if (!ReadInstalledDifficulties(target, sharedAudio, existing, error)) return fail();
        std::vector<DifficultyImportWrite> writes;
        for (const auto& item : incomingCharts) {
            const auto& entry = item.second;
            fs::path destination;
            if (!PrepareDifficultyImport(target, installed, existing,
                    target / entry.file.filename(), entry.chart, true,
                    writes, destination, error)) return fail();
        }
        if (!PublishDifficultyImports(writes, error)) return fail();
        fs::remove_all(stage, ec);
        ec.clear(); // An abandoned staging folder is outside the live library.
    }
    // Publication or merge completed. Keep the actual installed
    // location even if the in-memory index cannot be refreshed immediately.
    installedSet = target;
    std::string notice;
    if (!Refresh(notice)) {
        error = "Package installed on disk, but library refresh failed: " + notice;
        return false;
    }
    const auto present = std::find_if(songs_.begin(), songs_.end(),
        [&](const LibrarySong& song) {
            return song.managed && song.folder == target;
        });
    if (present == songs_.end() || present->charts.empty()) {
        error = "Package installed on disk, but library index omitted it: " + notice;
        return false;
    }
    error.clear();
    return true;
}

} // namespace locus::app
