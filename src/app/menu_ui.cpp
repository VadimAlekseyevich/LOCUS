#include "app/menu_ui.h"
#include "core/chart_difficulty.h"
#include "core/utf8.h"
#include "app/ui_button.h"
#include "app/ui_widgets.h"
#include "app/ui_text.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>

namespace locus::app {
namespace {

const Color kText = ApprovedUiTheme().foreground;
const Color kMuted = ApprovedUiTheme().secondaryText;
const Color kMint = ApprovedUiTheme().softBlue;
const Color kViolet = ApprovedUiTheme().rose;
const Color kGold = UiMix(ApprovedUiTheme().softBlue,
                          ApprovedUiTheme().rose, 0.45f);

void Label(Font font, const std::string& str, float x, float y,
           float size, Color color = kText) {
    DrawTextEx(font, str.c_str(), {x, y}, size, 1.0f, color);
}

std::string Clip(const std::string& input, std::size_t length = 34) {
    return core::utf8::Truncate(input, length);
}

void Button(Font font, Rectangle area, const std::string& label,
            Color accent, bool selected = false) {
    DrawUiButton(font, area, label.c_str(), MenuButtonStyle(accent), selected);
}

void Header(Font font, const char* section) {
    // The app frame already draws the LOCUS wordmark.
    Label(font, section, 52.0f, 88.0f, 30.0f, kMint);
}

} // namespace

Rectangle LibrarySongRow(int row) {
    return {395.0f, 154.0f + 39.0f * row, 538.0f, 35.0f};
}
Rectangle LibraryChartRow(int row) {
    return {414.0f, 154.0f + 39.0f * row, 519.0f, 35.0f};
}
Rectangle LibraryRefreshButton() { return {807, 480, 126, 37}; }
Rectangle LibraryBackButton() { return {14, 9, 100, 39}; }
Rectangle LibrarySearchField() { return {395, 109, 538, 35}; }
Rectangle LibraryPlayButton() { return {27, 473, 154, 41}; }
Rectangle LibraryEditButton() { return {193, 473, 155, 41}; }
Rectangle SettingsBindingButton(int row) {
    return {737.0f, 217.0f + 37.0f * row, 165.0f, 31.0f};
}
Rectangle SettingsBindingGroupButton(int group) {
    return {504.0f + group * 132.0f, 180.0f, 124.0f, 29.0f};
}
Rectangle SettingsBindingsPreviousButton() { return {642, 445, 100, 28}; }
Rectangle SettingsBindingsNextButton() { return {800, 445, 100, 28}; }

int SettingsBindingCount(int group) noexcept {
    return group == 0 ? kGameActionCount
         : group == 1 ? kNavigationShortcutCount
         : group == 2 ? kShortcutActionCount - kNavigationShortcutCount : 0;
}
int SettingsBindingIndex(const SettingsUi& ui, int visibleRow) noexcept {
    if (visibleRow < 0 || visibleRow >= kVisibleSettingBindings ||
        ui.firstBinding + visibleRow >= SettingsBindingCount(ui.bindingGroup))
        return -1;
    const int index = ui.firstBinding + visibleRow;
    return ui.bindingGroup == 0 ? index
         : ui.bindingGroup == 1 ? kGameActionCount + index
         : kGameActionCount + kNavigationShortcutCount + index;
}
Rectangle SettingsGameTab() { return {251, 158, 190, 34}; } // AUDIO
Rectangle SettingsDisplayTab() { return {71, 158, 170, 34}; }
Rectangle SettingsOffsetField() { return {78, 331, 135, 40}; }
Rectangle SettingsCalibrateButton() { return {230, 331, 188, 40}; }
Rectangle SettingsCalibrationApplyButton() { return {273, 387, 171, 44}; }
Rectangle SettingsCalibrationRetryButton() { return {454, 387, 145, 44}; }
Rectangle SettingsCalibrationCancelButton() { return {609, 387, 137, 44}; }
Rectangle SettingsVolumeMinus() { return {300, 230, 43, 35}; }
Rectangle SettingsVolumePlus() { return {354, 230, 43, 35}; }
Rectangle SettingsFlashButton() { return {297, 303, 100, 35}; }
Rectangle SettingsVsyncButton() { return {297, 211, 100, 36}; }
Rectangle SettingsFpsMinus() { return {301, 286, 43, 35}; }
Rectangle SettingsFpsPlus() { return {354, 286, 43, 35}; }
Rectangle SettingsShowFpsButton() { return {297, 355, 100, 36}; }
Rectangle SettingsFullscreenButton() { return {297, 421, 100, 36}; }
Rectangle MenuBackButton() { return {794, 78, 110, 37}; }

namespace {
bool SearchMatches(const std::string& text, const std::string& query) {
    if (query.empty()) return true;
    std::string value = text;
    std::string needle = query;
    // ASCII case-insensitivity; non-ASCII UTF-8 is preserved byte for byte.
    const auto fold = [](std::string& str) {
        for (char& c : str) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
    };
    fold(value);
    fold(needle);
    return value.find(needle) != std::string::npos;
}
bool SongMatches(const SongLibrary& library, int index,
                 const std::string& query) {
    if (query.empty()) return true;
    if (index == 0)
        return SearchMatches("BUILT-IN DEMO ORIGINAL", query);
    const auto& song = library.Songs()[static_cast<std::size_t>(index - 1)];
    if (SearchMatches(song.title, query) ||
        SearchMatches(song.artist, query) ||
        SearchMatches(song.mapper, query)) return true;
    for (const auto& tag : song.tags)
        if (SearchMatches(tag, query)) return true;
    for (const auto& chart : song.charts)
        if (SearchMatches(chart.data.title, query)) return true;
    return false;
}
} // namespace

std::vector<LibraryTreeEntry> LibraryTreeRows(const SongLibrary& songs,
                                               const SongSelectUi& ui) {
    std::vector<LibraryTreeEntry> entries;
    const int count = static_cast<int>(songs.Songs().size()) + 1;
    entries.reserve(static_cast<std::size_t>(count + 20));
    for (int song = 0; song < count; ++song) {
        if (!SongMatches(songs, song, ui.search)) continue;
        entries.push_back({song, -1});
        if (song == ui.selectedSong) {
            const int difficulties = song == 0 ? 1
                : static_cast<int>(songs.Songs()[
                    static_cast<std::size_t>(song - 1)].charts.size());
            const bool showAll = ui.search.empty() || song == 0 ||
                SearchMatches(songs.Songs()[static_cast<std::size_t>(song - 1)].title, ui.search) ||
                SearchMatches(songs.Songs()[static_cast<std::size_t>(song - 1)].artist, ui.search) ||
                SearchMatches(songs.Songs()[static_cast<std::size_t>(song - 1)].mapper, ui.search) ||
                std::any_of(songs.Songs()[static_cast<std::size_t>(song - 1)].tags.begin(),
                            songs.Songs()[static_cast<std::size_t>(song - 1)].tags.end(),
                            [&](const std::string& tag) {
                                return SearchMatches(tag, ui.search);
                            });
            for (int chart = 0; chart < difficulties; ++chart) {
                if (showAll || SearchMatches(
                    songs.Songs()[static_cast<std::size_t>(song - 1)]
                        .charts[static_cast<std::size_t>(chart)].data.title, ui.search))
                    entries.push_back({song, chart});
            }
        }
    }
    return entries;
}

namespace {
void KeepTreeSelectionVisible(SongSelectUi& ui, const SongLibrary& songs) {
    const auto rows = LibraryTreeRows(songs, ui);
    const int maxScroll = std::max(
        0, static_cast<int>(rows.size()) - kLibraryTreeVisibleRows);
    ui.treeScroll = std::clamp(ui.treeScroll, 0, maxScroll);
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (rows[i].song != ui.selectedSong ||
            rows[i].chart != (ui.treeChartFocused ? ui.selectedChart : -1))
            continue;
        if (i < ui.treeScroll) ui.treeScroll = i;
        if (i >= ui.treeScroll + kLibraryTreeVisibleRows)
            ui.treeScroll = i - kLibraryTreeVisibleRows + 1;
        break;
    }
}
} // namespace

void SongSelectUi::SelectSong(int index, const SongLibrary& songs) {
    selectedSong = std::clamp(index, 0,
                             static_cast<int>(songs.Songs().size()));
    selectedChart = 0;
    firstVisibleChart = 0;
    treeChartFocused = false;
    KeepTreeSelectionVisible(*this, songs);
}
void SongSelectUi::MoveSong(int delta, const SongLibrary& songs) {
    SelectSong(selectedSong + delta, songs);
}
void SongSelectUi::SelectChart(int index, const SongLibrary& songs) {
    const auto* song = selectedSong > 0 &&
                       selectedSong <= static_cast<int>(songs.Songs().size())
        ? &songs.Songs()[static_cast<std::size_t>(selectedSong - 1)] : nullptr;
    const int count = song ? static_cast<int>(song->charts.size()) : 1;
    selectedChart = std::clamp(index, 0, std::max(0, count - 1));
    treeChartFocused = true;
    KeepTreeSelectionVisible(*this, songs);
}
void SongSelectUi::MoveChart(int delta, const SongLibrary& songs) {
    SelectChart(selectedChart + delta, songs);
}
void SongSelectUi::MoveTree(int delta, const SongLibrary& songs) {
    const auto rows = LibraryTreeRows(songs, *this);
    if (rows.empty()) return;
    int position = 0;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (rows[i].song == selectedSong &&
            rows[i].chart == (treeChartFocused ? selectedChart : -1)) {
            position = i;
            break;
        }
    }
    const auto target = rows[static_cast<std::size_t>(std::clamp(
        position + delta, 0, static_cast<int>(rows.size()) - 1))];
    if (target.chart < 0) SelectSong(target.song, songs);
    else SelectChart(target.chart, songs);
}
void SongSelectUi::ScrollTree(int delta, const SongLibrary& songs) {
    const auto rows = LibraryTreeRows(songs, *this);
    treeScroll = std::clamp(treeScroll + delta, 0,
        std::max(0, static_cast<int>(rows.size()) - kLibraryTreeVisibleRows));
}
const LibraryChart* SongSelectUi::Selected(const SongLibrary& songs) const {
    if (selectedSong <= 0 ||
        selectedSong > static_cast<int>(songs.Songs().size())) return nullptr;
    const auto& charts = songs.Songs()[static_cast<std::size_t>(selectedSong - 1)].charts;
    if (selectedChart < 0 ||
        selectedChart >= static_cast<int>(charts.size())) return nullptr;
    return &charts[static_cast<std::size_t>(selectedChart)];
}

void DrawMainMenu(Font font, const std::string& status,
                  const HomeMenuUi& ui, const HomeBrandAssets& brand,
                  double now) {
    const auto& theme = ApprovedUiTheme();
    brand.Draw(font);

    const char* labels[] = {"PLAY", "EDITOR", "SKINS", "SETTINGS", "EXIT"};
    constexpr UiButtonVariant variants[] = {
        UiButtonVariant::Rose, UiButtonVariant::Blue,
        UiButtonVariant::Cool, UiButtonVariant::Warm,
        UiButtonVariant::Quiet
    };
    for (int i = 0; i < kHomeMenuActionCount; ++i) {
        const Rectangle bounds = HomeMenuButton(i);
        UiInteraction state{};
        state.focused = ui.focus.Focused() == i;
        DrawUiThemedButton(font, bounds, labels[i], variants[i],
                           state, ui.motion[static_cast<std::size_t>(i)].Frame(now),
                           theme, i == 4 ? 16.0f : 18.0f);
    }

    // The approved Home composition intentionally has no stats/shortcut
    // clutter. Actual failure notices remain accessible.
    if (status.find("FAILED") != std::string::npos ||
        status.find("NOT SAVED") != std::string::npos ||
        status.find("UNAVAILABLE") != std::string::npos) {
        DrawUiToast(font, {77.0f, 465.0f, 700.0f, 42.0f},
                    status.c_str(), true, 1.0f, theme);
    }
}

void DrawSongSelectMenu(Font font, const SongLibrary& library,
                        const SongSelectUi& ui) {
    // Nightbloom phase-one migration. Every clickable rectangle below is
    // shared with HandleMenuInput through menu_ui.h, not a visual-only mock.
    const auto& theme = ApprovedUiTheme();
    const Color faint = theme.secondaryText;
    const Color sky = theme.softBlue;
    const Color lilac = theme.rose;
    const Color pale = UiMix(sky, lilac, 0.4f);
    const Color line = UiOpacity(lilac, 0.25f);
    const auto fit = [&](const std::string& str, float x, float y,
                         float width, float size, Color color) {
        const std::string text = UiFitText(font, str, width, size);
        Label(font, text, x, y, size, color);
    };
    const auto panel = [&](Rectangle bounds) {
        DrawRectangleRounded(bounds, UiRoundness(bounds, 14.0f), 10,
                             Color{28, 33, 51, 225});
        DrawRectangleRoundedLinesEx(bounds, UiRoundness(bounds, 14.0f),
                                    10, 1.0f, UiOpacity(lilac, 0.28f));
    };
    const auto action = [&](Rectangle rect, const char* caption,
                            UiButtonVariant variant) {
        DrawUiThemedButton(font, rect, caption, variant);
    };

    // The selected Song Set background is painted full-window by RenderFrame.
    // Layout: 352px info/attempts left, 564px scrollable song tree right.
    panel({14, 65, 350, 461});
    panel({382, 65, 564, 461});
    action(LibraryBackButton(), "BACK", UiButtonVariant::Quiet);
    Label(font, "PLAY", 139, 17, 22, sky);

    const LibrarySong* song = ui.selectedSong > 0 &&
        ui.selectedSong <= static_cast<int>(library.Songs().size())
        ? &library.Songs()[static_cast<std::size_t>(ui.selectedSong - 1)]
        : nullptr;
    const LibraryChart* selected = ui.Selected(library);
    Label(font, "SONG SET", 28, 77, 11, lilac);
    fit(song ? song->title : "BUILT-IN DEMO",
        28, 94, 320, 19, theme.foreground);
    fit(song ? (song->artist.empty() ? "UNKNOWN ARTIST" : song->artist)
             : "LOCUS", 28, 120, 315, 14, sky);
    Label(font, song ? TextFormat("%.2f BPM  |  %d CHARTS",
              song->bpm, static_cast<int>(song->charts.size()))
                : "120 BPM  |  1 CHART",
          28, 143, 12, faint);
    DrawLine(28, 166, 350, 166, line);

    Label(font, "DIFFICULTY", 28, 174, 11, lilac);
    fit(selected ? selected->data.title : "ORIGINAL",
        28, 190, 246, 17, theme.foreground);
    const auto score = selected
        ? selected->difficulty
        : core::CalculateChartDifficulty(core::MakeDemoChart());
    Label(font, TextFormat("%.1f", score.overall), 288, 184, 27, sky);
    const int turns = selected
        ? static_cast<int>(selected->data.route.turns.size())
        : static_cast<int>(core::MakeDemoChart().turns.size());
    Label(font, TextFormat("%d TURNS", turns), 28, 213, 11, faint);

    // Six real difficulty metrics take only two lines; Results gets the
    // remaining ~165px instead of being squeezed below explanatory copy.
    Label(font, TextFormat("SPD %.1f   RHY %.1f   COORD %.1f",
          score.speed, score.rhythm, score.coordination),
          28, 232, 12, theme.foreground);
    Label(font, TextFormat("READ %.1f   STA %.1f   PEAK %.1f",
          score.reading, score.stamina, score.peak),
          28, 250, 12, faint);
    DrawLine(28, 271, 350, 271, line);

    Label(font, "RESULTS", 28, 283, 12, sky);
    // No invented results or fake replay entries. Native per-chart attempt
    // persistence is a separate feature; the area is reserved for real data.
    Label(font, "NO SAVED RUNS YET", 28, 319, 14, faint);
    action(LibraryPlayButton(), "PLAY", UiButtonVariant::Blue);
    action(LibraryEditButton(), "EDIT", UiButtonVariant::Rose);

    // Enlarged scrollable right browser; song and chart rows share hitboxes
    // with the controller and use the same flattened hierarchy.
    Label(font, "SONGS / CHARTS", 397, 78, 17, sky);
    const Rectangle search = LibrarySearchField();
    DrawUiTextField(font, search, ui.search, "Search songs...",
                    ui.editingSearch, false, false, theme);

    const auto rows = LibraryTreeRows(library, ui);
    const int end = std::min(ui.treeScroll + kLibraryTreeVisibleRows,
                             static_cast<int>(rows.size()));
    for (int index = ui.treeScroll; index < end; ++index) {
        const int visible = index - ui.treeScroll;
        const auto item = rows[static_cast<std::size_t>(index)];
        const bool child = item.chart >= 0;
        const bool current = item.song == ui.selectedSong &&
            (child ? ui.treeChartFocused && item.chart == ui.selectedChart
                   : !ui.treeChartFocused);
        const Rectangle rect = child ? LibraryChartRow(visible)
                                     : LibrarySongRow(visible);
        DrawRectangleRounded(
            rect, UiRoundness(rect, 8.0f), 8,
            current ? UiMix(theme.raised, lilac, 0.38f)
                    : child ? Color{35, 42, 65, 235}
                            : Color{45, 50, 75, 236});
        DrawRectangleRoundedLinesEx(
            rect, UiRoundness(rect, 8.0f), 8,
            current ? 1.5f : 1.0f,
            current ? (child ? sky : lilac) : line);
        const std::string title = child
            ? (item.song > 0
                ? library.Songs()[static_cast<std::size_t>(item.song - 1)]
                      .charts[static_cast<std::size_t>(item.chart)].data.title
                : "ORIGINAL")
            : (item.song > 0
                ? library.Songs()[static_cast<std::size_t>(item.song - 1)].title
                : "BUILT-IN DEMO");
        fit(child ? "  " + title : (item.song == ui.selectedSong ? "v  " : ">  ") + title,
            rect.x + 9, rect.y + 8, rect.width - (child ? 75.0f : 24.0f),
            child ? 14.0f : 15.0f,
            child ? (current ? theme.foreground : sky) : theme.foreground);
        if (child) {
            const auto& rating = item.song > 0
                ? library.Songs()[static_cast<std::size_t>(item.song - 1)]
                      .charts[static_cast<std::size_t>(item.chart)].difficulty
                : score;
            const std::string value = TextFormat("%.1f", rating.overall);
            const float width = MeasureTextEx(font, value.c_str(), 15, 1).x;
            Label(font, value, rect.x + rect.width - width - 13,
                  rect.y + 8, 15, pale);
        }
    }
    if (rows.empty())
        Label(font, "NO MATCHING SONGS", 410, 176, 14, faint);
    Label(font, TextFormat("%d-%d / %d", rows.empty() ? 0 : ui.treeScroll + 1,
          end, static_cast<int>(rows.size())), 397, 476, 12, faint);
    action(LibraryRefreshButton(), "RESCAN", UiButtonVariant::Neutral);

    if (!ui.notice.empty() &&
        (ui.notice.find("FAILED") != std::string::npos ||
         ui.notice.find("UNSAVED") != std::string::npos)) {
        DrawUiToast(font, {25, 423, 330, 43}, ui.notice.c_str(), true);
    }

    // Preserve the original destructive confirmation logic and inputs.
    if (ui.pendingDelete != LibraryDeletePending::None) {
        DrawUiDialogSurface({177, 163, 606, 216},
            ui.pendingDelete == LibraryDeletePending::SongSet
              ? "MOVE ENTIRE SONG SET TO TRASH?"
              : "MOVE THIS DIFFICULTY TO TRASH?", font);
        Label(font, "Y CONFIRM       N / ESC CANCEL", 248, 274, 18,
              theme.foreground);
    }
}

void DrawSettingsMenu(Font font, const GameSettings& settings,
                      const SettingsUi& ui) {
    Header(font, "SETTINGS");
    Button(font, MenuBackButton(), "BACK", kMuted);

    // Both columns have explicit boundaries and consistent row spacing;
    // controls no longer sink into a footer or overlap debugging text.
    const Rectangle leftPanel{52, 149, 408, 325};
    const Rectangle rightPanel{486, 149, 432, 325};
    const auto& theme = ApprovedUiTheme();
    const Color panel = theme.surface;
    const Color edge = theme.divider;
    DrawRectangleRounded(leftPanel, 0.055f, 6, panel);
    DrawRectangleRoundedLinesEx(leftPanel, 0.055f, 6, 1.0f, edge);
    DrawRectangleRounded(rightPanel, 0.055f, 6, panel);
    DrawRectangleRoundedLinesEx(rightPanel, 0.055f, 6, 1.0f, edge);

    Button(font, SettingsDisplayTab(), "DISPLAY", kMint,
           ui.page == SettingsPage::Display);
    Button(font, SettingsGameTab(), "AUDIO", kViolet,
           ui.page == SettingsPage::Game);

    if (ui.page == SettingsPage::Game) {
        Label(font, "MASTER VOLUME", 73, 202, 16, kMuted);
        Label(font, TextFormat("%d%%",
              static_cast<int>(std::lround(settings.volume * 100))),
              85, 236, 23, kText);
        Button(font, SettingsVolumeMinus(), "-", kMint);
        Button(font, SettingsVolumePlus(), "+", kMint);
        DrawLine(72, 276, 441, 276, edge);

        Label(font, "GLOBAL AUDIO OFFSET / MS", 73, 302, 16, kMuted);
        Button(font, SettingsOffsetField(),
               ui.editingGlobalOffset ? ui.globalOffsetBuffer + "_"
               : (settings.globalOffsetMs > 0 ? "+" : "") +
                    std::to_string(settings.globalOffsetMs) + " MS",
               kMint, ui.editingGlobalOffset);
        Button(font, SettingsCalibrateButton(), "CALIBRATE", kViolet);
    } else {
        Label(font, "VERTICAL SYNC", 73, 214, 16, kMuted);
        Button(font, SettingsVsyncButton(), settings.vsync ? "ON" : "OFF",
               kMint, settings.vsync);
        DrawLine(72, 264, 441, 264, edge);

        Label(font, "FRAME RATE LIMIT", 73, 282, 16, kMuted);
        Label(font, settings.FpsLimitLabel(), 85, 302, 19, kText);
        Button(font, SettingsFpsMinus(), "-", kViolet);
        Button(font, SettingsFpsPlus(), "+", kViolet);
        DrawLine(72, 339, 441, 339, edge);

        Label(font, "SHOW FPS COUNTER", 73, 363, 16, kMuted);
        Button(font, SettingsShowFpsButton(),
               settings.showFps ? "ON" : "OFF", kGold, settings.showFps);
        DrawLine(72, 405, 441, 405, edge);

        Label(font, "FULLSCREEN (F11)", 73, 428, 16, kMuted);
        Button(font, SettingsFullscreenButton(),
               settings.fullscreen ? "ON" : "OFF", kMuted, settings.fullscreen);
    }

    Label(font, "KEY BINDINGS", 508, 156, 17, kMint);
    const char* groups[] = {"PLAY", "MENUS", "EDITOR"};
    for (int i = 0; i < 3; ++i)
        Button(font, SettingsBindingGroupButton(i), groups[i], kViolet,
               ui.bindingGroup == i);
    for (int row = 0; row < kVisibleSettingBindings; ++row) {
        const int index = SettingsBindingIndex(ui, row);
        if (index < 0) continue;
        const bool game = index < kGameActionCount;
        const std::string actionName = game
            ? ActionName(static_cast<GameAction>(index))
            : ShortcutName(static_cast<ShortcutAction>(
                index - kGameActionCount));
        const std::string key = ui.capturing == index ? "PRESS KEY..." :
            game ? KeyboardKeyName(settings.Key(static_cast<GameAction>(index)))
                 : KeyboardChordName(settings.Shortcut(
                       static_cast<ShortcutAction>(index - kGameActionCount)));
        Label(font, Clip(actionName, 22), 505, 224.0f + row * 37.0f, 15, kText);
        Button(font, SettingsBindingButton(row), key, kViolet,
               ui.capturing == index);
    }
    const int total = SettingsBindingCount(ui.bindingGroup);
    Button(font, SettingsBindingsPreviousButton(), "PREV", kMuted);
    Button(font, SettingsBindingsNextButton(), "NEXT", kMuted);
    Label(font, TextFormat("%d-%d / %d",
          ui.firstBinding + 1,
          std::min(total, ui.firstBinding + kVisibleSettingBindings), total),
          515, 452, 13, kMuted);

    // Footer notices are outside the two settings panels, not over controls.
    Label(font, Clip(ui.notice, 49), 64, 495, 13,
          ui.noticeError ? Color{243, 142, 145, 255} : kGold);
    // Exclusive calibration overlay. The music is synthesized locally and
    // the samples are scored against its raw (unshifted) audio-time clock.
    if (ui.calibrationOpen) {
        DrawRectangle(0, 0, 960, 540, Color{8, 12, 23, 246});
        const Rectangle panel{164, 104, 632, 357};
        DrawRectangleRounded(panel, 0.05f, 8, Color{26, 30, 42, 255});
        DrawRectangleRoundedLinesEx(panel, 0.05f, 8, 2.0f, kMint);
        Label(font, "GLOBAL OFFSET CALIBRATION", 208, 133, 25, kMint);
        Label(font, "METRONOME AT 100 BPM  /  EVERY CLICK COUNTS",
              208, 184, 16, kText);
        const std::string directions =
            KeyboardKeyName(settings.Key(GameAction::Up)) + " / " +
            KeyboardKeyName(settings.Key(GameAction::Right)) + " / " +
            KeyboardKeyName(settings.Key(GameAction::Down)) + " / " +
            KeyboardKeyName(settings.Key(GameAction::Left));
        Label(font, "TAP ANY BOUND DIRECTION: " + directions,
              208, 213, 14, kMuted);
        Label(font, "START ON ANY BEAT  /  TAP ON EVERY CLICK",
              208, 240, 15, kMuted);

        if (ui.calibration.Complete() && ui.calibration.Reliable()) {
            Label(font, TextFormat("SUGGESTED OFFSET: %+d MS",
                  ui.calibration.SuggestedMs()), 230, 292, 27, kGold);
            Label(font, TextFormat("TAP VARIATION: %d MS",
                  ui.calibration.JitterMs()), 280, 335, 15, kMuted);
            Button(font, SettingsCalibrationApplyButton(), "APPLY", kMint);
        } else if (ui.calibration.Complete() || ui.calibrationExpired) {
            Label(font, "INCONSISTENT / NOT ENOUGH TAPS",
                  228, 296, 21, kGold);
            Label(font, "RETRY IN A QUIET ROOM WITH HEADPHONES",
                  243, 335, 14, kMuted);
        } else {
            Label(font, TextFormat("VALID ACCENTS  %d / %d",
                  ui.calibration.Count(),
                  GlobalOffsetCalibrator::kRequiredTaps),
                  299, 298, 23, kGold);
            Label(font, "KEEP TAPPING EVERY BEAT UNTIL THE COUNT IS FULL",
                  214, 334, 14, kMuted);
        }
        Button(font, SettingsCalibrationRetryButton(), "RETRY", kViolet);
        Button(font, SettingsCalibrationCancelButton(), "CANCEL", kMuted);
    }
}

} // namespace locus::app
