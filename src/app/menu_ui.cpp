#include "app/menu_ui.h"
#include "core/chart_difficulty.h"
#include "core/utf8.h"
#include "app/ui_button.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace locus::app {
namespace {

Color kText{225, 232, 240, 255};
Color kMuted{159, 181, 201, 255};
Color kMint{130, 226, 215, 255};
Color kViolet{183, 166, 243, 255};
Color kGold{244, 210, 137, 255};

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

Rectangle HomeMenuButton(int index) {
    return {82.0f, 174.0f + 67.0f * index, 290.0f, 55.0f};
}
Rectangle LibrarySongRow(int row) {
    return {498.0f, 171.0f + 35.0f * row, 408.0f, 32.0f};
}
Rectangle LibraryChartRow(int row) {
    return {518.0f, 171.0f + 35.0f * row, 388.0f, 32.0f};
}
Rectangle LibraryRefreshButton() { return {618, 82, 147, 34}; }
Rectangle LibraryPlayButton() { return {501, 458, 125, 43}; }
Rectangle LibraryEditButton() { return {637, 458, 122, 43}; }
Rectangle SettingsBindingButton(int index) {
    return {736.0f, 192.0f + 39.0f * index, 167.0f, 33.0f};
}
Rectangle SettingsGameTab() { return {71, 158, 170, 31}; }
Rectangle SettingsDisplayTab() { return {251, 158, 190, 31}; }
Rectangle SettingsVolumeMinus() { return {300, 230, 43, 35}; }
Rectangle SettingsVolumePlus() { return {354, 230, 43, 35}; }
Rectangle SettingsFlashButton() { return {297, 303, 100, 35}; }
Rectangle SettingsVsyncButton() { return {297, 211, 100, 36}; }
Rectangle SettingsFpsMinus() { return {301, 286, 43, 35}; }
Rectangle SettingsFpsPlus() { return {354, 286, 43, 35}; }
Rectangle SettingsShowFpsButton() { return {297, 355, 100, 36}; }
Rectangle SettingsFullscreenButton() { return {297, 421, 100, 36}; }
Rectangle MenuBackButton() { return {794, 78, 110, 37}; }

std::vector<LibraryTreeEntry> LibraryTreeRows(const SongLibrary& songs,
                                               const SongSelectUi& ui) {
    std::vector<LibraryTreeEntry> entries;
    const int count = static_cast<int>(songs.Songs().size()) + 1;
    const int children = ui.selectedSong == 0 ? 1 :
        ui.selectedSong <= static_cast<int>(songs.Songs().size())
        ? static_cast<int>(songs.Songs()[
            static_cast<std::size_t>(ui.selectedSong - 1)].charts.size()) : 0;
    entries.reserve(static_cast<std::size_t>(count + children));
    for (int song = 0; song < count; ++song) {
        entries.push_back({song, -1});
        if (song == ui.selectedSong) {
            const int difficulties = song == 0 ? 1
                : static_cast<int>(songs.Songs()[
                    static_cast<std::size_t>(song - 1)].charts.size());
            for (int chart = 0; chart < difficulties; ++chart)
                entries.push_back({song, chart});
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

void DrawMainMenu(Font font, const SongLibrary& library,
                  const std::string& status) {
    Header(font, "FOLLOW THE RHYTHM");
    const char* names[] = {"PLAY", "EDIT", "SKIN", "SETTINGS", "EXIT"};
    const Color colors[] = {kMint, kViolet, kGold,
                            Color{147, 195, 240, 255},
                            Color{241, 162, 165, 255}};
    for (int i = 0; i < 5; ++i)
        Button(font, HomeMenuButton(i), names[i], colors[i]);

    Label(font, "YOUR MUSIC. YOUR ROUTES.", 456, 181, 25, kText);
    Label(font, "SONG LIBRARY", 457, 248, 17, kMuted);
    Label(font, std::to_string(library.Songs().size()) + " SONGS  /  " +
          std::to_string(library.ChartCount()) + " CHARTS", 456, 276, 22, kMint);
    Label(font, "SONGS STAY INSTALLED BETWEEN SESSIONS", 456, 316, 14, kMuted);
    Label(font, "PLAY TO BROWSE MUSIC AND DIFFICULTIES", 456, 343, 14, kMuted);
    Label(font, "EDIT OPENS THE EXISTING CHART EDITOR", 456, 370, 14, kMuted);
    Label(font, "P PLAY  E EDIT  K SKIN  S SETTINGS", 456, 413, 14, kMuted);
    Label(font, "ESC / EXIT  -  CONFIRM QUIT", 456, 442, 15, kGold);
    if (!status.empty()) Label(font, Clip(status, 96), 83, 514, 13, kGold);
}

void DrawSongSelectMenu(Font font, const SongLibrary& library,
                        const SongSelectUi& ui) {
    Header(font, "PLAY  /  SONG LIBRARY");
    Button(font, LibraryRefreshButton(), "RESCAN  F5", kViolet);
    Button(font, MenuBackButton(), "BACK", kMuted);

    // The right side is a single expanding tree: songs are parent rows,
    // and only the selected song exposes indented chart rows below it.
    Label(font, "SONGS / CHARTS", 500, 144, 19, kMint);
    const auto rows = LibraryTreeRows(library, ui);
    for (int row = 0; row < kLibraryTreeVisibleRows; ++row) {
        const int index = ui.treeScroll + row;
        if (index >= static_cast<int>(rows.size())) break;
        const auto item = rows[static_cast<std::size_t>(index)];
        const bool child = item.chart >= 0;
        const Rectangle rect = child ? LibraryChartRow(row)
                                     : LibrarySongRow(row);
        const bool current = item.song == ui.selectedSong &&
            (child ? ui.treeChartFocused && item.chart == ui.selectedChart
                   : !ui.treeChartFocused);
        const bool expanded = !child && item.song == ui.selectedSong;
        DrawRectangleRounded(rect, 0.12f, 5,
                             current ? Color{51, 83, 86, 255}
                                     : child ? Color{35, 39, 57, 255}
                                             : Color{37, 46, 60, 255});
        DrawRectangleRoundedLinesEx(rect, 0.12f, 5,
                                    current ? 2.0f : 1.0f,
                                    current ? kMint : child ? kViolet : kMuted);
        std::string label;
        Color color;
        if (child) {
            const auto* song = item.song > 0
                ? &library.Songs()[static_cast<std::size_t>(item.song - 1)]
                : nullptr;
            label = "  |- " + (song
                ? song->charts[static_cast<std::size_t>(item.chart)].data.title
                : std::string("ORIGINAL / 120 BPM"));
            color = kViolet;
        } else {
            const auto* song = item.song > 0
                ? &library.Songs()[static_cast<std::size_t>(item.song - 1)]
                : nullptr;
            label = (expanded ? "v  " : ">  ") +
                    (song ? song->title : std::string("BUILT-IN DEMO"));
            color = item.song == 0 ? kGold : kMint;
        }
        Label(font, Clip(label, child ? 29 : 40),
              rect.x + 12, rect.y + 6, 16, current ? kText : color);
        if (child) {
            const core::ChartDifficulty score = item.song > 0
                ? library.Songs()[static_cast<std::size_t>(item.song - 1)]
                      .charts[static_cast<std::size_t>(item.chart)].difficulty
                : core::CalculateChartDifficulty(core::MakeDemoChart());
            Label(font, TextFormat("%.1f", score.overall),
                  rect.x + rect.width - 12.0f -
                      MeasureTextEx(font, TextFormat("%.1f", score.overall),
                                    15.0f, 1.0f).x,
                  rect.y + 7, 15, kGold);
        }
    }
    Label(font, TextFormat("SCROLL  %d-%d / %d",
          rows.empty() ? 0 : ui.treeScroll + 1,
          std::min(ui.treeScroll + kLibraryTreeVisibleRows,
                   static_cast<int>(rows.size())),
          static_cast<int>(rows.size())), 500, 421, 13, kMuted);

    Label(font, "SELECTED SONG", 54, 154, 18, kMuted);
    const LibrarySong* song = ui.selectedSong > 0 &&
        ui.selectedSong <= static_cast<int>(library.Songs().size())
        ? &library.Songs()[static_cast<std::size_t>(ui.selectedSong - 1)]
        : nullptr;
    Label(font, Clip(song ? song->title : "BUILT-IN DEMO", 29),
          54, 189, 21, kMint);
    if (song) {
        Label(font, Clip(song->artist.empty() ? "ARTIST: UNSPECIFIED"
                                      : "ARTIST: " + song->artist, 39),
              54, 230, 14, kMuted);
        Label(font, TextFormat("%d CHARTS  /  SONG %.3f BPM",
              static_cast<int>(song->charts.size()), song->bpm),
              54, 260, 16, kText);
        Label(font, TextFormat("SONG OFFSET %d MS", song->offsetMs),
              54, 286, 16, kMuted);
    } else {
        Label(font, "ORIGINAL CLICK TRACK / 120 BPM", 54, 241, 16, kMuted);
    }
    if (const auto* selected = ui.Selected(library)) {
        Label(font, "SELECTED DIFFICULTY", 54, 313, 14, kMuted);
        Label(font, Clip(selected->data.title, 30), 54, 332, 18, kViolet);
        const auto& score = selected->difficulty;
        Label(font, TextFormat("DIFFICULTY V2   %.1f", score.overall),
              54, 356, 20, kGold);
        Label(font, TextFormat("SPEED %.1f   RHYTHM %.1f   KEYS %.1f",
              score.speed, score.rhythm, score.coordination),
              54, 384, 13, kText);
        Label(font, TextFormat("READ %.1f   STAMINA %.1f   PEAK %.1f",
              score.reading, score.stamina, score.peak),
              54, 405, 13, kMuted);
    } else if (!song) {
        Label(font, "ORIGINAL / CLASSIC ROUTE", 54, 355, 15, kViolet);
    }
    Button(font, LibraryPlayButton(), "PLAY", kMint);
    Button(font, LibraryEditButton(), "EDIT", kViolet);
    Label(font, "UP/DOWN TREE  LEFT/RIGHT DIFFICULTY", 54, 445, 12, kMuted);
    Label(font, "EDIT > SET: SONGS / ARTWORK / DELETE", 54, 466, 13, kMuted);
    Label(font, Clip(ui.notice, 80), 54, 517, 13,
          ui.notice.find("FAILED") == std::string::npos ? kGold
              : Color{240, 138, 142, 255});
    if (ui.pendingDelete != LibraryDeletePending::None) {
        DrawRectangleRounded({164, 200, 636, 167}, 0.08f, 8,
                             Color{22, 27, 40, 252});
        DrawRectangleRoundedLinesEx({164, 200, 636, 167}, 0.08f, 8,
                                    2.5f, Color{240, 138, 142, 255});
        Label(font, ui.pendingDelete == LibraryDeletePending::SongSet
              ? "MOVE ENTIRE SONG SET TO TRASH?"
              : "MOVE THIS DIFFICULTY TO TRASH?",
              195, 234, 23, kGold);
        Label(font, "Y CONFIRM     N / ESC CANCEL", 225, 305, 20, kText);
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
    const Color panel{26, 30, 42, 255};
    const Color edge{51, 62, 78, 255};
    DrawRectangleRounded(leftPanel, 0.055f, 6, panel);
    DrawRectangleRoundedLinesEx(leftPanel, 0.055f, 6, 1.0f, edge);
    DrawRectangleRounded(rightPanel, 0.055f, 6, panel);
    DrawRectangleRoundedLinesEx(rightPanel, 0.055f, 6, 1.0f, edge);

    Button(font, SettingsGameTab(), "GAME", kMint,
           ui.page == SettingsPage::Game);
    Button(font, SettingsDisplayTab(), "DISPLAY", kViolet,
           ui.page == SettingsPage::Display);

    if (ui.page == SettingsPage::Game) {
        Label(font, "AUDIO  /  MASTER VOLUME", 73, 202, 16, kMuted);
        Label(font, TextFormat("%d%%",
              static_cast<int>(std::lround(settings.volume * 100))),
              85, 236, 23, kText);
        Button(font, SettingsVolumeMinus(), "-", kMint);
        Button(font, SettingsVolumePlus(), "+", kMint);
        DrawLine(72, 276, 441, 276, edge);

        Label(font, "VISUAL  /  HIT FLASH", 73, 296, 16, kMuted);
        Button(font, SettingsFlashButton(), settings.hitFlash ? "ON" : "OFF",
               kGold, settings.hitFlash);
        DrawLine(72, 347, 441, 347, edge);

        Label(font, "DIAGONAL DIRECTIONS", 73, 367, 16, kMuted);
        Label(font, "TWO INDEPENDENT ARROWS", 84, 395, 16, kText);
        Label(font, "EACH HAS ITS OWN HIT WINDOW", 73, 451, 12, kMuted);
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

    Label(font, "GAME + EDITOR KEY BINDINGS", 508, 161, 18, kMint);
    Label(font, "DIRECTIONS ALSO PLACE NOTES IN ROUTE", 508, 181, 12, kMuted);
    for (int i = 0; i < kGameActionCount; ++i) {
        const auto action = static_cast<GameAction>(i);
        Label(font, ActionName(action), 506, 200.0f + i * 39.0f, 17, kText);
        Button(font, SettingsBindingButton(i),
               ui.capturing == i ? "PRESS KEY..." :
               KeyboardKeyName(settings.Key(action)), kViolet,
               ui.capturing == i);
    }

    // Footer notices are outside the two settings panels, not over controls.
    Label(font, Clip(ui.notice, 49), 64, 495, 13,
          ui.noticeError ? Color{243, 142, 145, 255} : kGold);
    Label(font, "DISPLAY CHANGES APPLY IMMEDIATELY  /  ESC BACK",
          492, 495, 12, kMuted);
}

} // namespace locus::app
