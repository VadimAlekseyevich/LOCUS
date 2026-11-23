#include "app/library_browser.h"

#include <filesystem>
#include <string>
#include <utility>

namespace locus::app {

LibrarySelectionBookmark CaptureLibrarySelection(
    const SongLibrary& library, const SongSelectUi& ui) {
    LibrarySelectionBookmark result;
    if (const auto* selected = ui.Selected(library)) result.file = selected->file;
    result.song = ui.selectedSong;
    result.chart = ui.selectedChart;
    return result;
}

void RestoreLibrarySelection(SongLibrary& library, SongSelectUi& ui,
    const LibrarySelectionBookmark& bookmark, std::string notice,
    std::string& menuStatus) {
    int song = -1, chart = -1;
    if (!bookmark.file.empty() &&
        library.FindChart(bookmark.file, song, chart)) {
        ui.SelectSong(song + 1, library);
        ui.SelectChart(chart, library);
    } else {
        ui.SelectSong(bookmark.song, library);
        ui.SelectChart(bookmark.chart, library);
    }
    ui.notice = std::move(notice);
    menuStatus = ui.notice;
}

bool RefreshLibraryBrowser(SongLibrary& library, SongSelectUi& ui,
                           std::string& menuStatus) {
    // The SongLibrary::Refresh() implementation replaces its index on
    // success. Save identity BEFORE the rescan; selected pointers are invalid
    // once the new index is published.
    const auto bookmark = CaptureLibrarySelection(library, ui);
    std::string notice;
    if (!library.Refresh(notice)) {
        ui.notice = notice;
        menuStatus = notice;
        return false;
    }
    RestoreLibrarySelection(library, ui, bookmark, std::move(notice), menuStatus);
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

bool ImportLibraryChartToSet(SongLibrary& library, SongSelectUi& ui,
                             const std::filesystem::path& sourceChart,
                             const std::filesystem::path& setFolder,
                             std::string& menuStatus) {
    std::filesystem::path installed;
    std::string error;
    if (!library.ImportIntoSet(sourceChart, setFolder, installed, error)) {
        ui.notice = "IMPORT INTO SONG SET FAILED: " + error;
        return false;
    }
    int song = -1, chart = -1;
    if (library.FindChart(installed, song, chart)) {
        ui.SelectSong(song + 1, library);
        ui.SelectChart(chart, library);
    }
    ui.notice = "DIFFICULTY ADDED TO SONG SET";
    menuStatus = "LIBRARY: " + std::to_string(library.Songs().size()) +
                 " SONGS / " + std::to_string(library.ChartCount()) + " CHARTS";
    return true;
}

} // namespace locus::app
