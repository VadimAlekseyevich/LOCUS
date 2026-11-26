#pragma once

#include "app/game_settings.h"
#include "app/home_menu.h"
#include "app/home_brand_assets.h"
#include "app/global_offset_calibrator.h"
#include "app/library_selection.h"
#include "raylib.h"

#include <string>
#include <cstdint>
#include <limits>
#include <vector>

namespace locus::app {

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
constexpr int kLibraryResultVisibleRows = 6;
Rectangle LibraryResultRow(int visibleRow);
Rectangle LibraryResultPrevButton();
Rectangle LibraryResultNextButton();
Rectangle LibraryRefreshButton();
Rectangle LibraryBackButton();
Rectangle LibrarySearchField();
Rectangle LibraryModsButton();
Rectangle LibraryImportButton();
Rectangle LibraryRateSlider(int index);
Rectangle LibraryModsClose();
Rectangle LibraryModsReset();
Rectangle LibraryModButton(int index);
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
Rectangle SettingsHitSoundMinus();
Rectangle SettingsHitSoundPlus();
Rectangle SettingsMetronomeMinus();
Rectangle SettingsMetronomePlus();
Rectangle SettingsFlashButton();
Rectangle SettingsFullscreenButton();
Rectangle SettingsDimMinus();
Rectangle SettingsDimPlus();
Rectangle MenuBackButton();

void DrawMainMenu(Font font, const std::string& status,
                  const HomeMenuUi& ui, const HomeBrandAssets& brand,
                  double now);
void DrawSongSelectMenu(Font font, const SongLibrary& library,
                        const SongSelectUi& ui);
void DrawSettingsMenu(Font font, const GameSettings& settings,
                      const SettingsUi& ui);

} // namespace locus::app
