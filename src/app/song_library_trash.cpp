#include "app/song_library.h"
#include "app/song_library_internal.h"
#include "app/song_set_storage.h"
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
bool SongLibrary::TrashDifficulty(const fs::path& chart,
                                  std::string& error) {
    int song = -1, index = -1;
    if (!FindChart(chart, song, index)) {
        error = "Difficulty is not in the library.";
        return false;
    }
    // Keep this as a reference: copying every parsed chart merely to
    // inspect the set's metadata is expensive on large song libraries.
    const auto& set = songs_[static_cast<std::size_t>(song)];
    if (set.charts.size() <= 1) {
        error = "This is the final difficulty; delete the Song Set instead.";
        return false;
    }
    std::error_code ec;
    const fs::path root = fs::canonical(root_, ec);
    if (ec || !Inside(root, chart)) {
        error = "Cannot resolve the selected difficulty inside Songs.";
        return false;
    }
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

} // namespace locus::app
