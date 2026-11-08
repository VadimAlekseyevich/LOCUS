#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace locus::app {

struct SongSetManifest {
    std::string id;
    std::string title;
    std::string artist;
    std::string audioFile;
};

// An OS-owned, nonblocking per-directory file lock. A crash releases it
// automatically; the harmless .locus-lock placeholder may remain on disk.
class FolderWriteLock {
public:
    FolderWriteLock() = default;
    ~FolderWriteLock();
    FolderWriteLock(const FolderWriteLock&) = delete;
    FolderWriteLock& operator=(const FolderWriteLock&) = delete;
    bool Acquire(const std::filesystem::path& folder, std::string& error);
private:
#ifdef _WIN32
    void* handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};

bool LoadSongSetManifest(const std::filesystem::path& path,
                         SongSetManifest& manifest, std::string& error);
bool WriteNewSongSetManifest(const std::filesystem::path& path,
                             const SongSetManifest& manifest, std::string& error);
// Reusing the same bytes succeeds; changing shared audio requires an explicit
// separate operation and cannot silently alter existing difficulties.
bool InstallSharedSongAudio(const std::filesystem::path& source,
                            const std::filesystem::path& destination,
                            std::string& error);
bool SameFileBytes(const std::filesystem::path& a,
                   const std::filesystem::path& b, bool& identical,
                   std::string& error);
// Called while holding FolderWriteLock; backup failure blocks the overwrite.
bool BackUpChartBeforeReplace(const std::filesystem::path& chart,
                              std::string& error);
// Native Windows Recycle Bin, preserving the user's ability to recover files.
bool RecycleSongFile(const std::filesystem::path& file, std::string& error);

} // namespace locus::app
