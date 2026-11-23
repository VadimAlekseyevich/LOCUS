#include "app/library_browser.h"

#include <filesystem>
#include <string>
#include <utility>

namespace locus::app {

LibrarySelectionBookmark CaptureLibrarySelection(
    const SongLibrary& library, const SongSelectUi& ui) {
    LibrarySelectionBookmark result;
    if (const auto* selected = ui.Selected(library)) {
        result.file = selected->file;
        result.difficultyId = selected->data.difficultyId;
        if (ui.selectedSong > 0 &&
            ui.selectedSong <= static_cast<int>(library.Songs().size()))
            result.songSetId = library.Songs()[
                static_cast<std::size_t>(ui.selectedSong - 1)].id;
    }
    result.song = ui.selectedSong;
    result.chart = ui.selectedChart;
    return result;
}

void RestoreLibrarySelection(SongLibrary& library, SongSelectUi& ui,
    const LibrarySelectionBookmark& bookmark, std::string notice,
    std::string& menuStatus) {
    int song = -1, chart = -1;
    // The durable chart identity survives a real filesystem rename. Match
    // that first; matching the old path is only a fallback for legacy drafts.
    if (!bookmark.songSetId.empty() && !bookmark.difficultyId.empty()) {
        for (std::size_t s = 0; s < library.Songs().size(); ++s) {
            const auto& set = library.Songs()[s];
            if (set.id != bookmark.songSetId) continue;
            for (std::size_t c = 0; c < set.charts.size(); ++c) {
                if (set.charts[c].data.difficultyId == bookmark.difficultyId) {
                    song = static_cast<int>(s);
                    chart = static_cast<int>(c);
                    break;
                }
            }
            break;
        }
    }
    if (song < 0 && !bookmark.file.empty())
        library.FindChart(bookmark.file, song, chart);
    if (song >= 0 && chart >= 0) {
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
