#pragma once

#include "app/game_settings.h"
#include "app/song_library.h"
#include "raylib.h"

#include <string>

namespace locus::app {

enum class LibraryDeletePending { None, Difficulty, SongSet };

struct LibraryTreeEntry {
    int song = 0; // 0 is the built-in demo.
    int chart = -1; // -1 is the parent Song row.
};

struct SongSelectUi {
    // 0 = the built-in demo, 1..N = permanent user Songs.
    int selectedSong = 0;
    int selectedChart = 0;
    int firstVisibleSong = 0;
    int firstVisibleChart = 0;
    int treeScroll = 0;
    bool treeChartFocused = false;
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
    SettingsPage page = SettingsPage::Game;
    int bindingGroup = 0; // 0=PLAY, 1=MENUS/LISTEN, 2=EDITOR.
    int firstBinding = 0; // Scroll within the selected group.
    int capturing = -1; // Global index: 0..gameplay + shortcut actions.
    std::string notice;
    bool noticeError = false;
};
constexpr int kVisibleSettingBindings = 6;
int SettingsBindingCount(int group) noexcept;
int SettingsBindingIndex(const SettingsUi& ui, int visibleRow) noexcept;

// These layout helpers are shared by drawing and hit-testing. Coordinates
// are in the same virtual 960x540 space as the existing editor.
Rectangle HomeMenuButton(int index);
Rectangle LibrarySongRow(int row);
Rectangle LibraryChartRow(int row);
// Flat visible rows contain the complete song list and child charts of only
// the selected song. All drawing, mouse and key hit-testing share these rows.
constexpr int kLibraryTreeVisibleRows = 7;
std::vector<LibraryTreeEntry> LibraryTreeRows(const SongLibrary& songs,
                                               const SongSelectUi& ui);
Rectangle LibraryRefreshButton();
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
Rectangle SettingsVolumeMinus();
Rectangle SettingsVolumePlus();
Rectangle SettingsFlashButton();
Rectangle SettingsFullscreenButton();
Rectangle MenuBackButton();

void DrawMainMenu(Font font, const SongLibrary& library,
                  const std::string& status);
void DrawSongSelectMenu(Font font, const SongLibrary& library,
                        const SongSelectUi& ui);
void DrawSettingsMenu(Font font, const GameSettings& settings,
                      const SettingsUi& ui);

} // namespace locus::app
