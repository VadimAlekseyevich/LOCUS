#include "app/library_file_actions.h"
#include "app/library_browser.h"

#include <filesystem>
#include <string>

namespace locus::app {
namespace {
namespace fs = std::filesystem;
}

void LibraryFileActions::Apply(const MenuEvents& event,
                               bool importFromHome, double& now) {
    if (event.refresh)
        RefreshLibraryBrowser(library_, ui_, menuStatus_);
    if (event.import || importFromHome)
        Import(event.importIntoSelected, now);
    if (event.exportSelected)
        Export(now);
    if (event.restoreDeleted)
        Restore();
    if (event.trashDifficulty)
        TrashDifficulty();
    if (event.trashSongSet)
        TrashSongSet();
    if (event.duplicateDifficulty)
        DuplicateDifficulty();
}

void LibraryFileActions::PickerReturned(double& now) const {
    // A native dialog can keep the app blocked for seconds. Never use an
    // old timestamp for the next audio or preview update.
    if (pickers_.now) now = pickers_.now();
}

void LibraryFileActions::Import(bool intoSelectedSet, double& now) {
    if (!pickers_.chooseImport) {
        ui_.notice = "IMPORT FAILED: FILE PICKER UNAVAILABLE";
        return;
    }
    const MusicPickResult selected = pickers_.chooseImport();
    PickerReturned(now);
    if (selected.path.empty()) {
        if (!selected.canceled)
            ui_.notice = "IMPORT FAILED: " + selected.error;
        return;
    }

    const fs::path chosen(selected.path);
    if (chosen.extension() == L".locuspack") {
        fs::path installedSet;
        if (!library_.ImportPackage(chosen, installedSet, error_)) {
            ui_.notice = "PACKAGE IMPORT FAILED: " + error_;
            return;
        }
        // The library was rescanned. Never retain stale LibrarySong pointers.
        for (std::size_t i = 0; i < library_.Songs().size(); ++i) {
            if (library_.Songs()[i].folder == installedSet) {
                ui_.SelectSong(static_cast<int>(i) + 1, library_);
                break;
            }
        }
        ui_.notice = "SONG SET PACKAGE IMPORTED";
        return;
    }

    if (intoSelectedSet) {
        if (ui_.selectedSong <= 0 ||
            ui_.selectedSong > static_cast<int>(library_.Songs().size()) ||
            !library_.Songs()[ui_.selectedSong - 1].managed) {
            ui_.notice = "SELECT A MANAGED SONG SET FIRST";
            return;
        }
        // Capture a path value: ImportIntoSet replaces the cached index.
        const fs::path folder = library_.Songs()[ui_.selectedSong - 1].folder;
        ImportLibraryChartToSet(library_, ui_, chosen, folder, menuStatus_);
        return;
    }
    ImportLibraryChart(library_, ui_, chosen, menuStatus_);
}

void LibraryFileActions::Export(double& now) {
    if (ui_.selectedSong <= 0 ||
        ui_.selectedSong > static_cast<int>(library_.Songs().size())) {
        ui_.notice = "SELECT A SONG SET TO EXPORT";
        return;
    }
    if (!pickers_.chooseExport) {
        ui_.notice = "EXPORT FAILED: FILE PICKER UNAVAILABLE";
        return;
    }

    const fs::path folder = library_.Songs()[ui_.selectedSong - 1].folder;
    const std::wstring suggested =
        (fs::current_path() /
         (folder.filename().wstring() + L".locuspack")).wstring();
    const MusicPickResult chosen = pickers_.chooseExport(suggested);
    PickerReturned(now);
    if (chosen.path.empty()) {
        if (!chosen.canceled) ui_.notice = "EXPORT FAILED: " + chosen.error;
        return;
    }
    ui_.notice = library_.ExportPackage(folder, fs::path(chosen.path), error_)
        ? "SONG SET EXPORTED"
        : "EXPORT FAILED: " + error_;
}

void LibraryFileActions::Restore() {
    if (!library_.RestoreLastDeleted(error_))
        ui_.notice = "RESTORE FAILED: " + error_;
    else
        ui_.notice = "RESTORED FROM TRASH";
    ui_.SelectSong(ui_.selectedSong, library_);
}

void LibraryFileActions::TrashDifficulty() {
    const LibraryChart* selected = ui_.Selected(library_);
    if (!selected) return;
    const int oldSong = ui_.selectedSong;
    const int oldChart = ui_.selectedChart;
    const fs::path file = selected->file;
    if (!library_.TrashDifficulty(file, error_)) {
        ui_.notice = "DELETE FAILED: " + error_;
        return;
    }
    ui_.SelectSong(oldSong, library_);
    ui_.SelectChart(oldChart, library_);
    ui_.notice = "DIFFICULTY MOVED TO TRASH - U RESTORE";
}

void LibraryFileActions::TrashSongSet() {
    if (ui_.selectedSong <= 0 ||
        ui_.selectedSong > static_cast<int>(library_.Songs().size()))
        return;
    const fs::path folder = library_.Songs()[ui_.selectedSong - 1].folder;
    if (!library_.TrashSongSet(folder, error_)) {
        ui_.notice = "DELETE FAILED: " + error_;
        return;
    }
    ui_.SelectSong(ui_.selectedSong, library_);
    ui_.notice = "SONG SET MOVED TO TRASH - U RESTORE";
}

void LibraryFileActions::DuplicateDifficulty() {
    const LibraryChart* selected = ui_.Selected(library_);
    if (!selected) return;
    const fs::path original = selected->file;
    fs::path copy;
    if (!library_.DuplicateDifficulty(original, copy, error_)) {
        ui_.notice = "COPY FAILED: " + error_;
        return;
    }
    int song = -1, chart = -1;
    if (library_.FindChart(copy, song, chart)) {
        ui_.SelectSong(song + 1, library_);
        ui_.SelectChart(chart, library_);
    }
    ui_.notice = "DIFFICULTY COPIED";
}

} // namespace locus::app
