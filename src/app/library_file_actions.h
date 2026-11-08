#pragma once

#include "app/menu_controller.h"
#include "app/menu_ui.h"
#include "app/music_picker.h"
#include "app/song_library.h"

#include <string>

namespace locus::app {

// File and library-index commands are separate from game-session navigation.
// Native pickers and the monotonic clock are injected for headless tests.
struct LibraryFilePickers {
    MusicPickResult (*chooseImport)() = nullptr;
    MusicPickResult (*chooseExport)(const std::wstring&) = nullptr;
    double (*now)() = nullptr;
    MusicPickResult (*chooseBackground)() = nullptr;
};

class LibraryFileActions {
public:
    LibraryFileActions(SongLibrary& library, SongSelectUi& ui,
                       std::string& menuStatus, std::string& error,
                       LibraryFilePickers pickers) noexcept
        : library_(library), ui_(ui), menuStatus_(menuStatus),
          error_(error), pickers_(pickers) {}

    void Apply(const MenuEvents& menuEvents, bool importFromHome, double& now);

private:
    void Import(bool intoSelectedSet, double& now);
    void Export(double& now);
    void Restore();
    void TrashDifficulty();
    void TrashSongSet();
    void DuplicateDifficulty();
    void SetBackground(double& now);
    void RemoveBackground();
    void PickerReturned(double& now) const;

    SongLibrary& library_;
    SongSelectUi& ui_;
    std::string& menuStatus_;
    std::string& error_;
    LibraryFilePickers pickers_;
};
} // namespace locus::app
