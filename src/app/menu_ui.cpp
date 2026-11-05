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
    return {739.0f, 173.0f + 42.0f * index, 166.0f, 35.0f};
}
Rectangle SettingsVolumeMinus() { return {273, 192, 48, 38}; }
Rectangle SettingsVolumePlus() { return {339, 192, 48, 38}; }
Rectangle SettingsFlashButton() { return {272, 280, 115, 39}; }
Rectangle SettingsChordMinus() { return {273, 363, 48, 38}; }
Rectangle SettingsChordPlus() { return {339, 363, 48, 38}; }
Rectangle SettingsFullscreenButton() { return {273, 447, 114, 38}; }
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
    const char* names[] = {"PLAY", "EDIT", "SKIN", "SETTINGS"};
    const Color colors[] = {kMint, kViolet, kGold, Color{147, 195, 240, 255}};
    for (int i = 0; i < 4; ++i)
        Button(font, HomeMenuButton(i), names[i], colors[i]);

    Label(font, "YOUR MUSIC. YOUR ROUTES.", 456, 181, 25, kText);
    Label(font, "SONG LIBRARY", 457, 248, 17, kMuted);
    Label(font, std::to_string(library.Songs().size()) + " SONGS  /  " +
          std::to_string(library.ChartCount()) + " CHARTS", 456, 276, 22, kMint);
    Label(font, "SONGS STAY INSTALLED BETWEEN SESSIONS", 456, 316, 14, kMuted);
    Label(font, "PLAY TO BROWSE MUSIC AND DIFFICULTIES", 456, 343, 14, kMuted);
    Label(font, "EDIT OPENS THE EXISTING CHART EDITOR", 456, 370, 14, kMuted);
    Label(font, "P PLAY   E EDIT   K SKIN   S SETTINGS", 84, 484, 15, kMuted);
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

void DrawSkinMenu(Font font) {
    Header(font, "SKIN");
    Button(font, MenuBackButton(), "BACK", kMuted);
    Label(font, "VISUAL CUSTOMIZATION", 105, 190, 33, kViolet);
    Label(font, "SKINS WILL CONTROL:", 106, 260, 20, kText);
    Label(font, "MARKER  /  TRAJECTORY  /  HIT EFFECTS", 106, 310, 18, kMuted);
    Label(font, "GRID  /  JUNCTIONS  /  COLORS  /  UI", 106, 346, 18, kMuted);
    Label(font, "PLACEHOLDER ONLY - CURRENT SKIN IS UNCHANGED", 106, 413, 17, kGold);
    Label(font, "H / ESC  BACK TO HOME", 106, 481, 16, kMuted);
}

void DrawSettingsMenu(Font font, const GameSettings& settings,
                      const SettingsUi& ui) {
    Header(font, "SETTINGS");
    Button(font, MenuBackButton(), "BACK", kMuted);

    Label(font, "AUDIO  /  MASTER VOLUME", 69, 158, 18, kMuted);
    Label(font, TextFormat("%d%%", static_cast<int>(std::lround(settings.volume * 100))),
          77, 198, 26, kText);
    Button(font, SettingsVolumeMinus(), "-", kMint);
    Button(font, SettingsVolumePlus(), "+", kMint);

    Label(font, "VISUAL  /  HIT FLASH", 69, 247, 18, kMuted);
    Button(font, SettingsFlashButton(), settings.hitFlash ? "ON" : "OFF", kGold,
           settings.hitFlash);

    Label(font, "DIAGONAL CHORD WINDOW", 69, 332, 18, kMuted);
    Label(font, TextFormat("%d MS", settings.chordWindowMs), 77, 371, 21, kText);
    Button(font, SettingsChordMinus(), "-", kViolet);
    Button(font, SettingsChordPlus(), "+", kViolet);

    Label(font, "WINDOW  /  FULLSCREEN", 69, 418, 18, kMuted);
    Button(font, SettingsFullscreenButton(),
           settings.fullscreen ? "ON" : "OFF", kMuted, settings.fullscreen);

    Label(font, "GAMEPLAY KEY BINDINGS", 503, 143, 19, kMint);
    for (int i = 0; i < kGameActionCount; ++i) {
        const auto action = static_cast<GameAction>(i);
        Label(font, ActionName(action), 515, 181.0f + i * 42.0f, 17, kText);
        Button(font, SettingsBindingButton(i),
               ui.capturing == i ? "PRESS KEY..." :
               KeyboardKeyName(settings.Key(action)), kViolet,
               ui.capturing == i);
    }
    Label(font, "CLICK A KEY TO REBIND  /  ESC TO CANCEL", 508, 489, 13, kMuted);
    Label(font, Clip(ui.notice, 95), 66, 515, 14,
          ui.noticeError ? Color{243, 142, 145, 255} : kGold);
}

} // namespace locus::app
