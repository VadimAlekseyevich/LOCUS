#pragma once

#include "app/game_settings.h"
#include "app/song_library.h"
#include "raylib.h"

#include <string>

namespace locus::app {

struct SongSelectUi {
    // 0 = the built-in demo, 1..N = permanent user Songs.
    int selectedSong = 0;
    int selectedChart = 0;
    int firstVisibleSong = 0;
    int firstVisibleChart = 0;
    std::string notice;

    void SelectSong(int index, const SongLibrary& songs);
    void MoveSong(int delta, const SongLibrary& songs);
    void SelectChart(int index, const SongLibrary& songs);
    void MoveChart(int delta, const SongLibrary& songs);
    const LibraryChart* Selected(const SongLibrary& songs) const;
};

enum class SettingsPage { Game, Display };

struct SettingsUi {
    SettingsPage page = SettingsPage::Game;
    int capturing = -1; // GameAction to remap; -1 = normal menu.
    std::string notice;
    bool noticeError = false;
};

// These layout helpers are shared by drawing and hit-testing. Coordinates
// are in the same virtual 960x540 space as the existing editor.
Rectangle HomeMenuButton(int index);
Rectangle LibrarySongRow(int row);
Rectangle LibraryChartRow(int row);
Rectangle LibraryImportButton();
Rectangle LibraryRefreshButton();
Rectangle LibraryPlayButton();
Rectangle LibraryEditButton();
Rectangle SettingsBindingButton(int action);
Rectangle SettingsGameTab();
Rectangle SettingsDisplayTab();
Rectangle SettingsVsyncButton();
Rectangle SettingsFpsMinus();
Rectangle SettingsFpsPlus();
Rectangle SettingsShowFpsButton();
Rectangle SettingsVolumeMinus();
Rectangle SettingsVolumePlus();
Rectangle SettingsFlashButton();
Rectangle SettingsChordMinus();
Rectangle SettingsChordPlus();
Rectangle SettingsFullscreenButton();
Rectangle MenuBackButton();

void DrawMainMenu(Font font, const SongLibrary& library,
                  const std::string& status);
void DrawSongSelectMenu(Font font, const SongLibrary& library,
                        const SongSelectUi& ui);
void DrawSettingsMenu(Font font, const GameSettings& settings,
                      const SettingsUi& ui);

} // namespace locus::app
