#include "app/menu_ui.h"

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
    return input.size() <= length ? input
        : input.substr(0, length - 3) + "...";
}

void Button(Font font, Rectangle area, const std::string& label,
            Color accent, bool selected = false) {
    DrawRectangleRounded(area, 0.10f, 5,
                         selected ? Color{42, 75, 83, 255}
                                  : Color{36, 42, 58, 255});
    DrawRectangleRoundedLinesEx(area, 0.10f, 5, selected ? 2.5f : 1.6f, accent);
    const float fontSize = 17.0f;
    const Vector2 measure = MeasureTextEx(font, label.c_str(), fontSize, 1.0f);
    Label(font, label, area.x + (area.width - measure.x) / 2.0f,
          area.y + (area.height - measure.y) / 2.0f,
          fontSize, kText);
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
    return {54.0f, 177.0f + 42.0f * row, 389.0f, 37.0f};
}
Rectangle LibraryChartRow(int row) {
    return {501.0f, 177.0f + 42.0f * row, 404.0f, 37.0f};
}
Rectangle LibraryImportButton() { return {465, 82, 142, 34}; }
Rectangle LibraryRefreshButton() { return {618, 82, 147, 34}; }
Rectangle LibraryPlayButton() { return {504, 458, 192, 43}; }
Rectangle LibraryEditButton() { return {706, 458, 199, 43}; }
Rectangle SettingsBindingButton(int index) {
    return {736.0f, 192.0f + 39.0f * index, 167.0f, 33.0f};
}
Rectangle SettingsGameTab() { return {71, 158, 170, 31}; }
Rectangle SettingsDisplayTab() { return {251, 158, 190, 31}; }
Rectangle SettingsVolumeMinus() { return {300, 230, 43, 35}; }
Rectangle SettingsVolumePlus() { return {354, 230, 43, 35}; }
Rectangle SettingsFlashButton() { return {297, 303, 100, 35}; }
Rectangle SettingsChordMinus() { return {300, 386, 43, 35}; }
Rectangle SettingsChordPlus() { return {354, 386, 43, 35}; }
Rectangle SettingsVsyncButton() { return {297, 211, 100, 36}; }
Rectangle SettingsFpsMinus() { return {301, 286, 43, 35}; }
Rectangle SettingsFpsPlus() { return {354, 286, 43, 35}; }
Rectangle SettingsShowFpsButton() { return {297, 355, 100, 36}; }
Rectangle SettingsFullscreenButton() { return {297, 421, 100, 36}; }
Rectangle MenuBackButton() { return {794, 78, 110, 37}; }

void SongSelectUi::SelectSong(int index, const SongLibrary& songs) {
    const int maximum = static_cast<int>(songs.Songs().size());
    selectedSong = std::clamp(index, 0, maximum);
    selectedChart = 0;
    firstVisibleChart = 0;
    if (selectedSong < firstVisibleSong) firstVisibleSong = selectedSong;
    if (selectedSong >= firstVisibleSong + 6) firstVisibleSong = selectedSong - 5;
}
void SongSelectUi::MoveSong(int delta, const SongLibrary& songs) {
    SelectSong(selectedSong + delta, songs);
}
void SongSelectUi::SelectChart(int index, const SongLibrary& songs) {
    const auto* song = selectedSong > 0 &&
                       selectedSong <= static_cast<int>(songs.Songs().size())
        ? &songs.Songs()[static_cast<std::size_t>(selectedSong - 1)] : nullptr;
    const int maximum = song ? static_cast<int>(song->charts.size()) : 1;
    selectedChart = std::clamp(index, 0, std::max(0, maximum - 1));
    if (selectedChart < firstVisibleChart) firstVisibleChart = selectedChart;
    if (selectedChart >= firstVisibleChart + 5)
        firstVisibleChart = selectedChart - 4;
}
void SongSelectUi::MoveChart(int delta, const SongLibrary& songs) {
    SelectChart(selectedChart + delta, songs);
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
    Button(font, LibraryImportButton(), "IMPORT  C", kMint);
    Button(font, LibraryRefreshButton(), "RESCAN  F5", kViolet);
    Button(font, MenuBackButton(), "BACK", kMuted);
    Label(font, "SONGS", 54, 145, 18, kMuted);
    Label(font, "CHARTS  /  DIFFICULTIES", 500, 145, 18, kMuted);

    const int total = static_cast<int>(library.Songs().size()) + 1;
    for (int row = 0; row < 6; ++row) {
        const int index = ui.firstVisibleSong + row;
        if (index >= total) break;
        const std::string title = index == 0
            ? "BUILT-IN DEMO" : library.Songs()[index - 1].title;
        Button(font, LibrarySongRow(row),
               Clip(title, 29), index == 0 ? kGold : kMint,
               index == ui.selectedSong);
    }
    const LibrarySong* song = ui.selectedSong > 0 &&
        ui.selectedSong <= static_cast<int>(library.Songs().size())
        ? &library.Songs()[ui.selectedSong - 1] : nullptr;
    const int count = song ? static_cast<int>(song->charts.size()) : 1;
    for (int row = 0; row < 5; ++row) {
        const int index = ui.firstVisibleChart + row;
        if (index >= count) break;
        const std::string title = song
            ? song->charts[index].data.title : "ORIGINAL / 120 BPM";
        Button(font, LibraryChartRow(row), Clip(title, 32), kViolet,
               index == ui.selectedChart);
    }
    if (const auto* difficulty = ui.Selected(library)) {
        const auto& chart = difficulty->data.route;
        Label(font, Clip(difficulty->data.title, 37), 501, 399, 17, kText);
        Label(font, TextFormat("%.1f BPM / %d x %d / %d TURNS",
              chart.bpm, chart.gridWidth, chart.gridHeight,
              static_cast<int>(chart.turns.size())), 501, 427, 15, kMuted);
    } else {
        Label(font, "BUILT-IN CLICK TRACK / CLASSIC ROUTE", 501, 402, 16, kMuted);
    }
    Button(font, LibraryPlayButton(), "PLAY  ENTER", kMint);
    Button(font, LibraryEditButton(), "EDIT CHART  E", kViolet);
    Label(font, "UP/DOWN SONGS  LEFT/RIGHT CHARTS  H/ESC BACK",
          54, 463, 14, kMuted);
    Label(font, Clip(ui.notice, 100), 54, 517, 13,
          ui.notice.find("FAILED") == std::string::npos ? kGold
              : Color{240, 138, 142, 255});
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

        Label(font, "DIAGONAL CHORD WINDOW", 73, 367, 16, kMuted);
        Label(font, TextFormat("%d MS", settings.chordWindowMs),
              84, 393, 21, kText);
        Button(font, SettingsChordMinus(), "-", kViolet);
        Button(font, SettingsChordPlus(), "+", kViolet);
        Label(font, "AFFECTS TWO-KEY DIRECTION INPUT", 73, 451, 12, kMuted);
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
