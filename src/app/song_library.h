#pragma once

#include "core/chart_json.h"

#include <filesystem>
#include <string>
#include <vector>

namespace locus::app {

// LOCUS local user data survives executable rebuilds, working-directory
// changes and application restarts. On Windows: %LOCALAPPDATA%/LOCUS.
std::filesystem::path UserDataDirectory();

struct LibraryChart {
    std::filesystem::path file;
    core::LoadedChart data; // Parsed once per library refresh.
    std::filesystem::path audioFile; // Actual revision for this difficulty.
};

struct LibrarySong {
    std::filesystem::path audioFile; // Representative; play LibraryChart::audioFile.
    std::string title; // Original filename, without version metadata.
    std::vector<LibraryChart> charts; // Different authored difficulties.
    std::string id;             // Stable identity from Song Set metadata.
    std::string artist;         // Optional display metadata.
    std::filesystem::path folder; // Parent of all managed chart files.
    bool managed = false;       // False for legacy installed v1/v2 libraries.
    int previewStartMs = 0; // Song Set menu preview, independent of BPM/offset.
};

// On-disk Songs folders are authoritative. The index is kept in RAM and
// rescanned only at startup / import / explicit refresh, not every play.
// Audio is decoded lazily by LocalMusic when the user starts one difficulty.
class SongLibrary {
public:
    explicit SongLibrary(std::filesystem::path root) : root_(std::move(root)) {}

    bool Refresh(std::string& message);
    // Import stages immutable audio, then atomically saves the chart JSON.
    // Repeat imports update a difficulty without modifying any other chart's
    // audio; multiple revisions still appear under one logical song.
    bool Import(const std::filesystem::path& sourceChart,
                std::filesystem::path& installedChart, std::string& error);
    // Save a new difficulty directly to a Song Set. An optional target folder
    // adds the difficulty to an existing set after verifying shared audio.
    // Explicitly add a foreign chart to an existing Song Set. The audio
    // must be byte-identical to the Set's shared audio; never replace it.
    // Import/export portable .locuspack (ZIP store-only) bundles, without
    // overwriting any existing installed Song Set or difficulty.
    bool ImportPackage(const std::filesystem::path& archive,
                       std::filesystem::path& installedSet,
                       std::string& error);
    bool ExportPackage(const std::filesystem::path& setFolder,
                       const std::filesystem::path& archive,
                       std::string& error);
    bool ImportIntoSet(const std::filesystem::path& sourceChart,
                       const std::filesystem::path& targetSet,
                       std::filesystem::path& installedChart,
                       std::string& error);
    bool CreateDifficulty(const std::filesystem::path& sourceAudio,
                          const core::LoadedChart& chart,
                          std::filesystem::path& installedChart,
                          std::string& error,
                          const std::filesystem::path& targetSet = {});
    bool DuplicateDifficulty(const std::filesystem::path& original,
                             std::filesystem::path& copied,
                             std::string& error);
    bool TrashDifficulty(const std::filesystem::path& chart,
                         std::string& error);
    bool TrashSongSet(const std::filesystem::path& folder,
                      std::string& error);
    bool RestoreLastDeleted(std::string& error);
    bool SetPreviewStartMs(int songIndex, int timeMs, std::string& error);
    bool FindChart(const std::filesystem::path& path,
                   int& songIndex, int& chartIndex) const;

    const std::filesystem::path& Root() const noexcept { return root_; }
    const std::vector<LibrarySong>& Songs() const noexcept { return songs_; }
    std::size_t ChartCount() const noexcept;

private:
    std::filesystem::path root_;
    std::vector<LibrarySong> songs_;
};

} // namespace locus::app
