#pragma once

#include "app/game_settings.h"
#include "app/home_menu.h"
#include "app/home_brand_assets.h"
#include "app/global_offset_calibrator.h"
#include "app/song_library.h"
#include "app/run_results.h"
#include "raylib.h"

#include <string>
#include <cstdint>
#include <limits>
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

struct SongSelectUi {
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

    void SelectSong(int index, const SongLibrary& songs);
    void MoveSong(int delta, const SongLibrary& songs);
    void SelectChart(int index, const SongLibrary& songs);
    void MoveChart(int delta, const SongLibrary& songs);
    void MoveTree(int delta, const SongLibrary& songs);
    void ScrollTree(int delta, const SongLibrary& songs);
    const LibraryChart* Selected(const SongLibrary& songs) const;
};

enum class SettingsPage { Game, Display };

struct SettingsUi {
    SettingsPage page = SettingsPage::Display;
    int bindingGroup = 0; // 0=PLAY, 1=MENUS/LISTEN, 2=EDITOR.
    int firstBinding = 0; // Scroll within the selected group.
    int capturing = -1; // Global index: 0..gameplay + shortcut actions.
    bool editingGlobalOffset = false;
    std::string globalOffsetBuffer;
    bool calibrationOpen = false;
    bool calibrationExpired = false;
    GlobalOffsetCalibrator calibration;
    std::string notice;
    bool noticeError = false;
};
constexpr int kVisibleSettingBindings = 6;
int SettingsBindingCount(int group) noexcept;
int SettingsBindingIndex(const SettingsUi& ui, int visibleRow) noexcept;

// These layout helpers are shared by drawing and hit-testing. Coordinates
// are in the same virtual 960x540 space as the existing editor.
Rectangle LibrarySongRow(int row);
Rectangle LibraryChartRow(int row);
// Flat visible rows contain the complete song list and child charts of only
// the selected song. All drawing, mouse and key hit-testing share these rows.
constexpr int kLibraryTreeVisibleRows = 8;
constexpr int kLibraryResultVisibleRows = 4;
Rectangle LibraryResultRow(int visibleRow);
Rectangle LibraryResultPrevButton();
Rectangle LibraryResultNextButton();
// Borrowed until the next structural projection change. Copy an individual
// row before changing selection/search/catalog, never keep a row reference.
const std::vector<LibraryTreeEntry>& LibraryTreeRows(const SongLibrary& songs,
                                                     const SongSelectUi& ui);
// Returns rows.size() when the identity is absent.
std::size_t FindLibraryTreeRow(const std::vector<LibraryTreeEntry>& rows,
                               int song, int chart) noexcept;
Rectangle LibraryRefreshButton();
Rectangle LibraryBackButton();
Rectangle LibrarySearchField();
Rectangle LibraryPlayButton();
Rectangle LibraryEditButton();
Rectangle SettingsBindingButton(int visibleRow);
Rectangle SettingsBindingGroupButton(int group);
Rectangle SettingsBindingsPreviousButton();
Rectangle SettingsBindingsNextButton();
Rectangle SettingsGameTab();
Rectangle SettingsDisplayTab();
Rectangle SettingsVsyncButton();
Rectangle SettingsFpsMinus();
Rectangle SettingsFpsPlus();
Rectangle SettingsShowFpsButton();
Rectangle SettingsOffsetField();
Rectangle SettingsCalibrateButton();
Rectangle SettingsCalibrationApplyButton();
Rectangle SettingsCalibrationRetryButton();
Rectangle SettingsCalibrationCancelButton();
Rectangle SettingsVolumeMinus();
Rectangle SettingsVolumePlus();
Rectangle SettingsFlashButton();
Rectangle SettingsFullscreenButton();
Rectangle MenuBackButton();

void DrawMainMenu(Font font, const std::string& status,
                  const HomeMenuUi& ui, const HomeBrandAssets& brand,
                  double now);
void DrawSongSelectMenu(Font font, const SongLibrary& library,
                        const SongSelectUi& ui);
void DrawSettingsMenu(Font font, const GameSettings& settings,
                      const SettingsUi& ui);

} // namespace locus::app
