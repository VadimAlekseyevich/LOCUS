#pragma once

#include "app/menu_controller.h"
#include "app/menu_ui.h"
#include "app/session_transition.h"
#include "app/song_library.h"

#include <filesystem>
#include <string>

namespace locus::app {

class EditorFileActions;

// Pure selection decision: no renderer, audio device or file picker.
enum class LibraryLaunchKind {
    None, NewDifficulty, EditDraft, EditChart, PlayDemo, PlayChart
};
struct LibraryLaunchIntent {
    LibraryLaunchKind kind = LibraryLaunchKind::None;
    std::filesystem::path chart;
    std::filesystem::path audio;
    std::filesystem::path folder;
    bool managed = false;
};
inline LibraryLaunchIntent ChooseLibraryLaunch(
    const MenuEvents& event, const SongLibrary& library,
    const SongSelectUi& ui) {
    if (event.newDifficulty) {
        if (ui.selectedSong <= 0 ||
            ui.selectedSong > static_cast<int>(library.Songs().size()))
            return {LibraryLaunchKind::NewDifficulty};
        const auto& set = library.Songs()[
            static_cast<std::size_t>(ui.selectedSong - 1)];
        return {LibraryLaunchKind::NewDifficulty, {}, set.audioFile,
                set.folder, set.managed};
    }
    if (event.editSelected) {
        const auto* selected = ui.Selected(library);
        return selected ? LibraryLaunchIntent{
            LibraryLaunchKind::EditChart, selected->file}
            : LibraryLaunchIntent{LibraryLaunchKind::EditDraft};
    }
    if (event.playSelected) {
        if (ui.selectedSong == 0)
            return {LibraryLaunchKind::PlayDemo};
        if (const auto* selected = ui.Selected(library))
            return {LibraryLaunchKind::PlayChart, selected->file};
    }
    return {};
}

// SessionTransitionContext already owns the stable gameplay references.
// This focused adapter is only used when launching SongSelect entries.
struct LibraryLaunchContext {
    SongLibrary& library;
    SongSelectUi& ui;
    EditorFileActions& editorFiles;
    const SessionTransitionContext& session;
    std::wstring& loadedAudioPath;
    AppState& app;
    std::string& error;
};
bool LaunchLibrarySelection(const LibraryLaunchContext& context,
                            const MenuEvents& event, double now);

} // namespace locus::app
