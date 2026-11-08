#include "app/library_file_actions.h"
#include "app/library_browser.h"

#include <filesystem>
#include <string>

namespace locus::app {

void LibraryFileActions::Apply(const MenuEvents& menuEvents,
                               bool importFromHome, double& now) {
    SongLibrary& library = library_;
    SongSelectUi& libraryUi = ui_;
    std::string& menuStatus = menuStatus_;
    std::string& musicError = error_;
            if (menuEvents.refresh)
                locus::app::RefreshLibraryBrowser(library, libraryUi, menuStatus);
            if (menuEvents.import || importFromHome) {
                const auto selected = pickers_.chooseImport();
                now = pickers_.now();
                if (!selected.path.empty()) {
                    if (std::filesystem::path(selected.path).extension() ==
                            L".locuspack") {
                        std::filesystem::path installedSet;
                        if (!library.ImportPackage(
                                std::filesystem::path(selected.path),
                                installedSet, musicError)) {
                            libraryUi.notice = "PACKAGE IMPORT FAILED: " + musicError;
                        } else {
                            for (std::size_t i = 0; i < library.Songs().size(); ++i) {
                                if (library.Songs()[i].folder == installedSet) {
                                    libraryUi.SelectSong(
                                        static_cast<int>(i) + 1, library);
                                    break;
                                }
                            }
                            libraryUi.notice = "SONG SET PACKAGE IMPORTED";
                        }
                    } else if (menuEvents.importIntoSelected &&
                        libraryUi.selectedSong > 0 &&
                        libraryUi.selectedSong <=
                            static_cast<int>(library.Songs().size()) &&
                        library.Songs()[libraryUi.selectedSong - 1].managed) {
                        const auto setFolder =
                            library.Songs()[libraryUi.selectedSong - 1].folder;
                        locus::app::ImportLibraryChartToSet(
                            library, libraryUi,
                            std::filesystem::path(selected.path),
                            setFolder, menuStatus);
                    } else if (menuEvents.importIntoSelected) {
                        libraryUi.notice = "SELECT A MANAGED SONG SET FIRST";
                    } else {
                        locus::app::ImportLibraryChart(
                            library, libraryUi,
                            std::filesystem::path(selected.path), menuStatus);
                    }
                } else if (!selected.canceled) {
                    libraryUi.notice = "IMPORT FAILED: " + selected.error;
                }
            }
            if (menuEvents.exportSelected) {
                if (libraryUi.selectedSong > 0 &&
                    libraryUi.selectedSong <=
                        static_cast<int>(library.Songs().size())) {
                    const auto& set = library.Songs()[libraryUi.selectedSong - 1];
                    const auto suggestion =
                        (std::filesystem::current_path() /
                         (set.folder.filename().wstring() + L".locuspack")).wstring();
                    const auto chosen =
                        pickers_.chooseExport(suggestion);
                    now = pickers_.now();
                    if (!chosen.path.empty()) {
                        if (library.ExportPackage(set.folder,
                                std::filesystem::path(chosen.path), musicError))
                            libraryUi.notice = "SONG SET EXPORTED";
                        else
                            libraryUi.notice = "EXPORT FAILED: " + musicError;
                    } else if (!chosen.canceled) {
                        libraryUi.notice = "EXPORT FAILED: " + chosen.error;
                    }
                } else {
                    libraryUi.notice = "SELECT A SONG SET TO EXPORT";
                }
            }
            if (menuEvents.restoreDeleted) {
                if (!library.RestoreLastDeleted(musicError))
                    libraryUi.notice = "RESTORE FAILED: " + musicError;
                else
                    libraryUi.notice = "RESTORED FROM TRASH";
                libraryUi.SelectSong(libraryUi.selectedSong, library);
            }
            if (menuEvents.trashDifficulty) {
                if (const auto* selected = libraryUi.Selected(library)) {
                    const auto oldSong = libraryUi.selectedSong;
                    const auto oldChart = libraryUi.selectedChart;
                    const auto file = selected->file;
                    if (!library.TrashDifficulty(file, musicError))
                        libraryUi.notice = "DELETE FAILED: " + musicError;
                    else {
                        libraryUi.SelectSong(oldSong, library);
                        libraryUi.SelectChart(oldChart, library);
                        libraryUi.notice = "DIFFICULTY MOVED TO TRASH - U RESTORE";
                    }
                }
            }
            if (menuEvents.trashSongSet) {
                if (libraryUi.selectedSong > 0 &&
                    libraryUi.selectedSong <=
                        static_cast<int>(library.Songs().size())) {
                    const auto folder =
                        library.Songs()[libraryUi.selectedSong - 1].folder;
                    if (!library.TrashSongSet(folder, musicError))
                        libraryUi.notice = "DELETE FAILED: " + musicError;
                    else {
                        libraryUi.SelectSong(libraryUi.selectedSong, library);
                        libraryUi.notice = "SONG SET MOVED TO TRASH - U RESTORE";
                    }
                }
            }
            if (menuEvents.duplicateDifficulty) {
                if (const auto* selected = libraryUi.Selected(library)) {
                    const auto original = selected->file;
                    std::filesystem::path copy;
                    if (!library.DuplicateDifficulty(original, copy, musicError))
                        libraryUi.notice = "COPY FAILED: " + musicError;
                    else {
                        int s = -1, c = -1;
                        if (library.FindChart(copy, s, c)) {
                            libraryUi.SelectSong(s + 1, library);
                            libraryUi.SelectChart(c, library);
                        }
                        libraryUi.notice = "DIFFICULTY COPIED";
                    }
                }
            }

}

} // namespace locus::app
