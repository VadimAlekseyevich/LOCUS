#include "app/song_library.h"

#include "app/chart_file.h"
#include "app/chart_filename.h"
#include "app/audio_revision.h"
#include "app/song_set_storage.h"
#include "app/song_timing.h"
#include "app/image_asset_file.h"
#include "app/song_archive.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace locus::app {
namespace {
namespace fs = std::filesystem;

bool Inside(const fs::path& root, const fs::path& file) {
    auto a = root.begin(), b = file.begin();
    for (; a != root.end(); ++a, ++b) {
        if (b == file.end()) return false;
#ifdef _WIN32
        if (_wcsicmp(a->c_str(), b->c_str()) != 0) return false;
#else
        if (*a != *b) return false;
#endif
    }
    return b != file.end();
}

std::uint64_t PathHash(const fs::path& path) {
    std::uint64_t value = 14695981039346656037ull;
    const std::string name = path.generic_u8string();
    for (unsigned char byte : name) {
        value ^= byte;
        value *= 1099511628211ull;
    }
    return value;
}

std::string HashText(const fs::path& path) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << PathHash(path);
    return stream.str();
}

std::string HashSetId(const std::string& id) {
    // Manifest IDs are untrusted data. Hash raw UTF-8 bytes rather than
    // interpreting an arbitrary ID as a filesystem path on Windows.
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : id) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << hash;
    return stream.str();
}

std::string FilenamePart(std::string raw) {
    std::string output;
    for (unsigned char c : raw) {
        if (std::isalnum(c) && c < 128) output += static_cast<char>(c);
        else if (c == '-' || c == '_') output += static_cast<char>(c);
        else if (!output.empty() && output.back() != '-') output += '-';
        if (output.size() >= 45) break;
    }
    while (!output.empty() && output.back() == '-') output.pop_back();
    return output.empty() ? "song" : output;
}

std::string SongDisplayTitle(const fs::path& audio) {
    const auto name = audio.stem().u8string();
    return name.empty() ? std::string("Unknown song") : name;
}

} // namespace

std::filesystem::path UserDataDirectory() {
#ifdef _WIN32
    const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (count > 1 && count < 32768) {
        std::wstring value(count, L'\0');
        const DWORD copied = GetEnvironmentVariableW(L"LOCALAPPDATA",
                                                     value.data(), count);
        if (copied > 0 && copied < count) {
            value.resize(copied);
            return fs::path(value) / L"LOCUS";
        }
    }
#endif
    // Portable fallback if LOCALAPPDATA is unavailable.
    std::error_code ec;
    const auto dir = fs::current_path(ec);
    return (ec ? fs::path(".") : dir) / "LOCUS_Data";
}

std::size_t SongLibrary::ChartCount() const noexcept {
    std::size_t count = 0;
    for (const auto& song : songs_) count += song.charts.size();
    return count;
}

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
        core::LoadedChart chart;
        std::wstring audio;
        std::string problem;
        if (!OpenChartWithAudioPath(path.wstring(), chart, audio, problem)) {
            ++skipped;
            continue;
        }
        const fs::path audioFile = fs::canonical(fs::path(audio), ec);
        if (ec || !Inside(root, audioFile)) {
            ++skipped;
            ec.clear();
            continue;
        }

        // Managed Song Sets are grouped by their folder, not by incidental
        // audio-revision paths. Existing legacy folders remain untouched.
        const fs::path manifestPath = path.parent_path() / L"set.locus.json";
        const bool managed = fs::exists(manifestPath, ec);
        if (ec) { ++skipped; ec.clear(); continue; }
        SongSetManifest metadata;
        if (managed) {
            if (!LoadSongSetManifest(manifestPath, metadata, problem)) {
                ++skipped;
                continue;
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
        next[index].charts.push_back({path, std::move(chart), audioFile});
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
                for (auto& difficulty : song.charts) {
                    if (!ApplySongTiming(difficulty.data, song.bpm,
                                         song.offsetMs, problem)) {
                        message = "Song Set " + song.title + ": " + problem;
                        return false;
                    }
                }
            }
        }
        for (auto& difficulty : song.charts)
            difficulty.difficulty = core::CalculateChartDifficulty(
                difficulty.data.route, difficulty.data.offsetSeconds);
        std::sort(song.charts.begin(), song.charts.end(),
                  [](const LibraryChart& a, const LibraryChart& b) {
                      if (a.data.title != b.data.title)
                          return a.data.title < b.data.title;
                      return a.file < b.file;
                  });
    }
    std::sort(next.begin(), next.end(),
              [](const LibrarySong& a, const LibrarySong& b) {
                  if (a.title != b.title) return a.title < b.title;
                  return a.audioFile < b.audioFile;
              });
    songs_ = std::move(next);
    message = "LIBRARY: " + std::to_string(songs_.size()) + " SONGS / " +
              std::to_string(ChartCount()) + " CHARTS";
    if (skipped) message += " (" + std::to_string(skipped) + " SKIPPED)";
    return true;
}

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

bool SongLibrary::TrashDifficulty(const fs::path& chart,
                                  std::string& error) {
    int song = -1, index = -1;
    if (!FindChart(chart, song, index)) {
        error = "Difficulty is not in the library.";
        return false;
    }
    const auto set = songs_[static_cast<std::size_t>(song)];
    if (!set.managed) {
        error = "Cannot delete unmanaged legacy chart through the library.";
        return false;
    }
    if (set.charts.size() <= 1) {
        error = "This is the final difficulty; delete the Song Set instead.";
        return false;
    }
    const fs::path root = fs::canonical(root_);
    FolderWriteLock libraryLock, setLock;
    if (!libraryLock.Acquire(root, error) ||
        !setLock.Acquire(set.folder, error)) return false;
    if (!TrashLibraryItem(root, chart, error)) return false;
    std::string notice;
    if (!Refresh(notice)) { error = notice; return false; }
    error.clear();
    return true;
}

bool SongLibrary::TrashSongSet(const fs::path& folder,
                               std::string& error) {
    const auto set = std::find_if(songs_.begin(), songs_.end(),
        [&](const LibrarySong& song) {
            return song.folder == folder && song.managed;
        });
    if (set == songs_.end()) {
        error = "Selected Song Set is not managed by LOCUS.";
        return false;
    }
    std::error_code ec;
    const auto root = fs::canonical(root_, ec);
    const auto target = fs::canonical(folder, ec);
    if (ec || !Inside(root, target)) {
        error = "Song Set is outside the library.";
        return false;
    }
    FolderWriteLock libraryLock;
    if (!libraryLock.Acquire(root, error)) return false;
    if (!TrashLibraryItem(root, target, error)) return false;
    std::string notice;
    if (!Refresh(notice)) { error = notice; return false; }
    error.clear();
    return true;
}

bool SongLibrary::RestoreLastDeleted(std::string& error) {
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec) { error = "Cannot access Songs library."; return false; }
    const auto root = fs::canonical(root_, ec);
    if (ec) { error = "Cannot resolve Songs folder."; return false; }
    FolderWriteLock lock;
    if (!lock.Acquire(root, error)) return false;
    if (!RestoreLatestTrashItem(root, error)) return false;
    std::string notice;
    if (!Refresh(notice)) { error = notice; return false; }
    error.clear();
    return true;
}

bool SongLibrary::FindChart(const fs::path& path,
                            int& songIndex, int& chartIndex) const {
    for (std::size_t s = 0; s < songs_.size(); ++s)
        for (std::size_t c = 0; c < songs_[s].charts.size(); ++c) {
            if (songs_[s].charts[c].file == path) {
                songIndex = static_cast<int>(s);
                chartIndex = static_cast<int>(c);
                return true;
            }
        }
    return false;
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
