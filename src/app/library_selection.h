#pragma once

#include "app/song_library.h"
#include "app/run_results.h"
#include "core/play_mods.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace locus::app {

enum class LibraryDeletePending { None, Difficulty, SongSet };

struct LibraryTreeEntry {
    int song = 0; // 0 is the built-in demo.
    int chart = -1; // -1 is the parent Song row.
};

// Cached Unicode-normalized fields for the currently published catalog.
// The index is refreshed by generation, not rebuilt on every search keypress
// or drawing frame. It never owns a copy of the heavy chart note vectors.
struct LibrarySearchMatch {
    int song = 0;
    bool allCharts = false; // metadata (or demo) matched, not just a child title
};

struct LibrarySearchCache {
    const SongLibrary* source = nullptr;
    std::uint64_t sourceRevision = std::numeric_limits<std::uint64_t>::max();
    std::vector<std::string> metadata;
    std::vector<std::vector<std::string>> chartTitles;
    // Query projection is independent of the expanded Song Set. Only
    // matching parents are retained; child titles remain in the index.
    std::vector<LibrarySearchMatch> matches;
    std::string query;
    std::string normalizedQuery;
    bool queryValid = false;
    std::uint64_t matchGeneration = 0;
    void Refresh(const SongLibrary& library);
    const std::vector<LibrarySearchMatch>& Match(
        const SongLibrary& library, const std::string& text);
};

// Flattened rows are a projection of catalog revision, query and expanded
// song, not of the focused difficulty. All browser consumers borrow it.
struct LibraryTreeCache {
    const SongLibrary* source = nullptr;
    std::uint64_t sourceRevision = 0;
    std::uint64_t generation = 0;
    int expandedSong = -1;
    std::string query;
    std::vector<LibraryTreeEntry> rows;
};

// Only the selected difficulty is materialized for modified ratings. The
// catalog itself remains compact, and rate adjustment never reparses a file.
struct SelectedRateDifficultyCache {
    const SongLibrary* source = nullptr;
    std::uint64_t revision = 0;
    int song = -1, chart = -1;
    int rate = 0;
    core::ModFlags transforms=0;
    std::uint32_t seed=0;
    bool attemptedLoad = false;
    std::optional<core::LoadedChart> payload;
    std::optional<core::ChartDifficulty> rating;
};

struct SongSelectUi {
    core::PlayMods mods;
    bool modsOpen=false;
    int modSliderDrag=-1;
    void ToggleMod(core::PlayMod mod) noexcept;
    // 0 = the built-in demo, 1..N = permanent user Songs.
    int selectedSong = 0;
    int selectedChart = 0;
    int firstVisibleSong = 0;
    int firstVisibleChart = 0;
    int treeScroll = 0;
    bool treeChartFocused = false;
    std::string search;
    bool editingSearch = false;
    mutable LibrarySearchCache searchCache;
    mutable LibraryTreeCache treeCache;
    RunKey resultKey;
    std::string resultGameplayRevision;
    std::vector<RunResult> results;
    bool resultsLoading = false;
    std::uint64_t resultsRevision = 0;
    int resultScroll = 0;
    // The run history is replaced atomically on selection/load, never edited
    // per frame. This lets UI consumers detect changes in O(1) time.
    void ReplaceResults(RunKey key, std::vector<RunResult> loaded,
                        std::string gameplayRevision = {});
    std::string notice;
    LibraryDeletePending pendingDelete = LibraryDeletePending::None;
    std::string replayNotice; // explicit Results replay launch error

    void SelectSong(int index, const SongLibrary& songs);
    void MoveSong(int delta, const SongLibrary& songs);
    void SelectChart(int index, const SongLibrary& songs);
    void MoveChart(int delta, const SongLibrary& songs);
    void MoveTree(int delta, const SongLibrary& songs);
    void ScrollTree(int delta, const SongLibrary& songs);
    const LibraryChart* Selected(const SongLibrary& songs) const;
    std::optional<core::ChartDifficulty> SelectedDifficulty(const SongLibrary& songs) const;
    mutable SelectedRateDifficultyCache rateDifficulty;
};

// Legacy fallback scroll window; native presenters use measured viewports.
constexpr int kLibraryTreeVisibleRows = 8;

// Borrowed until the next structural projection change. Copy an individual
// row before changing selection/search/catalog, never keep a row reference.
const std::vector<LibraryTreeEntry>& LibraryTreeRows(const SongLibrary& songs,
                                                     const SongSelectUi& ui);
// Returns rows.size() when the identity is absent.
std::size_t FindLibraryTreeRow(const std::vector<LibraryTreeEntry>& rows,
                               int song, int chart) noexcept;

} // namespace locus::app
