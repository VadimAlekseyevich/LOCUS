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
    // Export is a read-only snapshot of the saved Song Set, not a
    // mutation based on cached chart metadata. Enumerate and validate
    // the authoritative files while holding the same locks as writers.
    const auto folder=fs::canonical(setFolder,ec);
    if (ec || !Inside(root,folder)) {
        error="Song Set resolves outside the library.";
        return false;
    }
    SongSetManifest published;
    if (!LoadSongSetManifest(folder / L"set.locus.json",published,error))
        return false;
    if (published.id!=selected->id) {
        error="Song Set identity changed on disk; refresh before exporting.";
        return false;
    }
    const auto sharedAudio=folder/fs::u8path(published.audioFile);
    std::set<std::string> exportedIds;
    std::vector<fs::path> charts;
    for (fs::directory_iterator it(folder,ec),end; !ec && it!=end; it.increment(ec)) {
        const auto file=it->path();
        if (file.extension()!=L".json" || file.filename()==L"set.locus.json") continue;
        if (it->is_symlink(ec) || !it->is_regular_file(ec) || ec) {
            error="Invalid difficulty file: "+file.filename().u8string();
            return false;
        }
        core::LoadedChart parsed;
        std::wstring audio;
        if (!OpenChartWithAudioPath(file.wstring(),parsed,audio,error)) {
            error=file.filename().u8string()+": "+error;
            return false;
        }
        if (!fs::equivalent(fs::path(audio),sharedAudio,ec) || ec) {
            error=file.filename().u8string()+": difficulty uses different audio.";
            return false;
        }
        if (!core::ValidDifficultyId(parsed.difficultyId) ||
            !exportedIds.insert(parsed.difficultyId).second) {
            error=file.filename().u8string()+": missing or duplicate difficulty ID.";
            return false;
        }
        if (!ApplySongTiming(parsed,published.tempoMap,error)) {
            error=file.filename().u8string()+": "+error;
            return false;
        }
        charts.push_back(file);
    }
    if (ec) {
        error="Cannot enumerate Song Set difficulties: "+ec.message();
        return false;
    }
    std::sort(charts.begin(),charts.end());
    return ExportSongSetArchive(folder,charts,archive,error);
}

bool SongLibrary::ImportPackage(const fs::path& archive,
                                fs::path& installedSet,
                                std::string& error) {
    LibraryImportBatch batch;
    batch.files.push_back({archive,{},true,false,{}});
    std::vector<std::vector<std::string>> ids(1);
    auto& item=batch.files.front();
    if (ImportPackageOnDisk(archive,item.installed,ids.front(),item.error))
        VerifyImports(batch,ids);
    installedSet=std::move(item.installed);
    error=std::move(item.error);
    return item.indexed;
}

bool SongLibrary::ImportPackageOnDisk(const fs::path& archive,
                                     fs::path& installedSet,
                                     std::vector<std::string>& incomingIds,
                                     std::string& error) {
    MarkDiskMutation();
    installedSet.clear();
    incomingIds.clear();
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
        // Validate the authoritative timing before either a new set is
        // published or an existing one is merged.
        auto projected = parsed;
        if (!ApplySongTiming(projected, metadata.tempoMap, error)) return fail();
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
    incomingIds.assign(incomingDifficultyIds.begin(),incomingDifficultyIds.end());
    error.clear();
    return true;
}

std::size_t LibraryImportBatch::ImportedCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(files.begin(),files.end(),
        [](const LibraryImportResult& item){ return item.indexed; }));
}

std::string LibraryImportBatch::Summary() const {
    const auto imported=ImportedCount();
    std::size_t saved=0;
    const LibraryImportResult* firstError=nullptr;
    for (const auto& item : files) if (!item.indexed) {
        if (!item.installed.empty()) ++saved;
        if (!firstError) firstError=&item;
    }
    std::string message="IMPORTED "+std::to_string(imported)+"/"+std::to_string(files.size());
    if (saved) message+=" / "+std::to_string(saved)+" SAVED, RESCAN REQUIRED";
    if (files.size()>imported+saved)
        message+=" / "+std::to_string(files.size()-imported-saved)+" FAILED";
    if (firstError) message+=": "+firstError->source.filename().u8string()+" - "+firstError->error;
    return message;
}

LibraryImportBatch SongLibrary::ImportFiles(const std::vector<fs::path>& sources,
                                           const fs::path& targetSet) {
    LibraryImportBatch batch;
    batch.files.reserve(sources.size());
    std::vector<std::vector<std::string>> ids(sources.size());
    for (std::size_t i=0;i<sources.size();++i) {
        const auto& source=sources[i];
        auto ext=source.extension().wstring();
        for (auto& ch : ext) if (ch>=L'A' && ch<=L'Z') ch+=L'a'-L'A';
        const bool package=ext==L".locuspack";
        auto& item=batch.files.emplace_back();
        item.source=source;
        item.package=package;
        if (package) ImportPackageOnDisk(source,item.installed,ids[i],item.error);
        else ImportChartOnDisk(source,targetSet,item.installed,item.error);
    }
    VerifyImports(batch,ids);
    return batch;
}

void SongLibrary::VerifyImports(LibraryImportBatch& batch,
                                const std::vector<std::vector<std::string>>& ids) {
    if (std::none_of(batch.files.begin(),batch.files.end(),[](const auto& item){
        return !item.installed.empty() && item.error.empty();
    })) return;
    std::string notice;
    const bool refreshed=Refresh(notice);
    std::map<fs::path,const LibrarySong*> sets;
    std::set<fs::path> charts;
    if (refreshed) for (const auto& song : songs_) {
        if (song.managed) sets.emplace(song.folder,&song);
        for (const auto& chart : song.charts) charts.insert(chart.file);
    }
    for (std::size_t i=0;i<batch.files.size();++i) {
        auto& item=batch.files[i];
        if (item.installed.empty() || !item.error.empty()) continue;
        if (!refreshed) {
            item.error="Saved on disk, but library refresh failed: "+notice;
            continue;
        }
        if (item.package) {
            const auto found=sets.find(item.installed);
            if (found!=sets.end()) {
                std::set<std::string> published;
                for (const auto& chart : found->second->charts)
                    published.insert(chart.metadata.difficultyId);
                item.indexed=std::all_of(ids[i].begin(),ids[i].end(),[&](const auto& id){
                    return published.count(id)!=0;
                });
            }
        } else item.indexed=charts.count(item.installed)!=0;
        if (!item.indexed) item.error="Saved on disk, but library index omitted imported data: "+notice;
    }
}

} // namespace locus::app
