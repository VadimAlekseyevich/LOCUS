#pragma once

#include "core/tempo_map.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <optional>
#include <vector>

namespace locus::app {

struct SongSetManifest {
    std::string id;
    std::string title;
    std::string artist;
    std::string mapper; // Authorship credit for the whole Song Set.
    std::vector<std::string> tags; // Set-wide, not chart metadata.
    std::string audioFile;
    int previewStartMs = 0; // Song menu playback only; not chart timing.
    std::optional<double> bpm; // Authoritative Song Set tempo.
    std::optional<int> offsetMs; // First tempo-map marker.
    std::vector<core::TempoMarker> tempoMap; // Authoritative shared timing.
    std::string backgroundFile; // Optional flat PNG/JPEG filename.
};
inline constexpr int kMaxSongPreviewStartMs = 3600000;

// Optional UTF-8 pack metadata, shared by all difficulties.
bool ValidSongSetMapper(const std::string& mapper) noexcept;
bool ValidSongSetTags(const std::vector<std::string>& tags);

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
bool ReplaceSongSetManifest(const std::filesystem::path& path,
                            const SongSetManifest& manifest, std::string& error);
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
// Atomic same-volume move to LOCUS/Trash with recoverable original path.
// The caller holds the library root write lock and validates the source.
bool TrashLibraryItem(const std::filesystem::path& libraryRoot,
                      const std::filesystem::path& item,
                      std::string& error);
// Restore newest recoverable Trash entry without overwriting existing files.
bool RestoreLatestTrashItem(const std::filesystem::path& libraryRoot,
                            std::string& error);

} // namespace locus::app
