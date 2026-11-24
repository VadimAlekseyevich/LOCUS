#pragma once

#include "core/chart_json.h"
#include "core/chart_difficulty.h"
#include "core/tempo_map.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace locus::app {

// LOCUS local user data survives executable rebuilds, working-directory
// changes and application restarts. On Windows: %LOCALAPPDATA%/LOCUS.
std::filesystem::path UserDataDirectory();

struct LibraryChart {
    std::filesystem::path file;
    // Immutable authored gameplay content. Old and new catalog snapshots
    // share one allocation for unchanged difficulties, including note arrays.
    // Readers borrow the payload; a BPM edit publishes a new instance.
    std::shared_ptr<const core::LoadedChart> data =
        std::make_shared<const core::LoadedChart>();
    std::filesystem::path audioFile; // Actual revision for this difficulty.
    // Calculated once from validated gameplay, reused across library views.
    // Display title, file rename or metadata-only editor changes do not alter it.
    std::string gameplayRevision;
    core::ChartDifficulty difficulty; // Reused while file and timing are unchanged.
    // Transient index fingerprint, not serialized with charts or Song Sets.
    std::uintmax_t indexedFileSize = 0;
    std::filesystem::file_time_type indexedWriteTime{};
    std::uint64_t indexedContentFingerprint = 0;
    bool indexedFingerprintValid = false;
};

struct LibrarySong {
    std::filesystem::path audioFile; // Representative; play LibraryChart::audioFile.
    std::string title; // Original filename, without version metadata.
    std::vector<LibraryChart> charts; // Different authored difficulties.
    std::string id;             // Stable identity from Song Set metadata.
    std::string artist;         // Song Set artist.
    std::string mapper;         // Song Set-wide mapper.
    std::vector<std::string> tags; // Shared by all difficulties.
    std::filesystem::path folder; // Parent of all managed chart files.
    bool managed = false;       // True after a valid current Song Set manifest is indexed.
    int previewStartMs = 0; // Song Set menu preview, independent of BPM/offset.
    double bpm = 120.0; // Authoritative Song Set tempo.
    int offsetMs = 0; // Shared beat-zero position.
    std::vector<core::TempoMarker> tempoMap;
    std::filesystem::path backgroundImage; // Optional Song Set art (not chart timing).
};

// A refreshed index reuses parsed charts and ratings when filesystem
// signatures are unchanged. Manifest timing is checked independently, so
// external Song Set BPM changes invalidate only affected ratings.
struct LibraryRefreshStats {
    std::size_t parsedCharts = 0;
    std::size_t reusedCharts = 0;
    // Reused without copying the potentially large note vector. Only a
    // detached worker cache can transfer previously parsed chart ownership.
    std::size_t transferredCharts = 0;
    std::size_t calculatedDifficulties = 0;
    // OS canonicalization probes for shared audio during the chart walk.
    // Should scale with distinct audio files, not difficulty count.
    std::size_t resolvedAudioPaths = 0;
};

// On-disk Songs folders are authoritative. The index is kept in RAM and
// rescanned only at startup / import / explicit refresh, not every play.
// Audio is decoded lazily by LocalMusic when the user starts one difficulty.
class AsyncLibraryRefresh;

class SongLibrary {
    friend class AsyncLibraryRefresh;
public:
    explicit SongLibrary(std::filesystem::path root) : root_(std::move(root)) {}

    bool Refresh(std::string& message,
                 const std::atomic<bool>* cancelled = nullptr);
    // Publish an isolated scan only on the main thread, once its original
    // catalog generation was validated by the caller.
    bool PublishScannedIndex(SongLibrary&& scanned);
    // Import stages shared Song Set audio, then atomically saves the chart JSON.
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
    // Atomically update the whole Song Set's display fields, not charts.
    bool SetSongMetadata(int songIndex, const std::string& title,
                         const std::string& artist, const std::string& mapper,
                         const std::vector<std::string>& tags,
                         std::string& error);
    bool SetPreviewStartMs(int songIndex, int timeMs, std::string& error);
    // Validates all difficulties before publishing Song Set timing.
    bool SetSongTiming(int songIndex, double bpm, int offsetMs,
                       const core::LoadedChart* currentDraft, std::string& error);
    bool SetSongTempoMap(int songIndex,
                         const std::vector<core::TempoMarker>& markers,
                         const core::LoadedChart* currentDraft,
                         std::string& error);
    // Empty source removes art; otherwise installs an independent library copy.
    bool SetBackgroundImage(int songIndex, const std::filesystem::path& source,
                            std::string& error);
    bool FindChart(const std::filesystem::path& path,
                   int& songIndex, int& chartIndex) const;

    const std::filesystem::path& Root() const noexcept { return root_; }
    const std::vector<LibrarySong>& Songs() const noexcept { return songs_; }
    const LibraryRefreshStats& LastRefreshStats() const noexcept { return lastRefreshStats_; }
    // Monotonically changes whenever the in-memory library index is modified.
    // Presenters use this in place of inspecting all song/chart strings every frame.
    std::uint64_t Revision() const noexcept { return revision_; }
    // Separate from UI/index revision. Mark before any potential write, even
    // when a disk commit can succeed but the subsequent rescan fails.
    void MarkDiskMutation() noexcept { ++diskMutationEpoch_; }
    std::uint64_t DiskMutationEpoch() const noexcept { return diskMutationEpoch_; }
    std::size_t ChartCount() const noexcept;

private:
    // Never used on the UI-owned library: a cancelled scan may consume its
    // cache, so destructive transfer is allowed only in an isolated worker.
    bool RefreshDetached(std::string& message,
                         const std::atomic<bool>* cancelled);
    bool RefreshImpl(std::string& message,
                     const std::atomic<bool>* cancelled,
                     bool transferCache);
    std::filesystem::path root_;
    std::vector<LibrarySong> songs_;
    LibraryRefreshStats lastRefreshStats_{};
    std::uint64_t revision_ = 0;
    std::uint64_t diskMutationEpoch_ = 0;
};

} // namespace locus::app
