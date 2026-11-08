#include "app/library_browser.h"

#include <filesystem>
#include <string>

namespace locus::app {

bool RefreshLibraryBrowser(SongLibrary& library, SongSelectUi& ui,
                           std::string& menuStatus) {
    // The SongLibrary::Refresh() implementation replaces its index on
    // success. Save identity BEFORE the rescan; selected pointers are invalid
    // once the new index is published.
    std::filesystem::path selectedFile;
    if (const auto* selected = ui.Selected(library))
        selectedFile = selected->file;
    const int oldSong = ui.selectedSong;
    const int oldChart = ui.selectedChart;

    if (!library.Refresh(ui.notice)) {
        menuStatus = ui.notice;
        return false; // SongLibrary keeps its previous index on scan failure.
    }
    int song = -1, chart = -1;
    if (!selectedFile.empty() && library.FindChart(selectedFile, song, chart)) {
        ui.SelectSong(song + 1, library);
        ui.SelectChart(chart, library);
    } else {
        // Selected difficulty was removed; clamp to a valid neighboring row.
        // Selecting a song always resets its chart index, so restore it next.
        ui.SelectSong(oldSong, library);
        ui.SelectChart(oldChart, library);
    }
    menuStatus = ui.notice;
    return true;
}

bool ImportLibraryChart(SongLibrary& library, SongSelectUi& ui,
                        const std::filesystem::path& sourceChart,
                        std::string& menuStatus) {
    std::filesystem::path installed;
    std::string error;
    if (!library.Import(sourceChart, installed, error)) {
        ui.notice = "IMPORT FAILED: " + error;
        return false;
    }
    int song = -1, chart = -1;
    if (library.FindChart(installed, song, chart)) {
        ui.SelectSong(song + 1, library);
        ui.SelectChart(chart, library);
    }
    ui.notice = "IMPORTED - SONG AND CHART SAVED PERMANENTLY";
    menuStatus = "LIBRARY: " + std::to_string(library.Songs().size()) +
                 " SONGS / " + std::to_string(library.ChartCount()) +
                 " CHARTS";
    return true;
}

} // namespace locus::app
