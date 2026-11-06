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
