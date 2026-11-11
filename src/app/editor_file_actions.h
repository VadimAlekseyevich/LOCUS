#pragma once

#include "app/music_picker.h"
#include "app/chart_editor_ui.h"

#include <filesystem>
#include <map>
#include <string>

namespace locus::core { class ChartEditor; }
namespace locus::app {
struct EditorUiState;
class LocalMusic;
class ChartPlayback;
class SongLibrary;
struct SongSelectUi;

// All objects are owned by main; file actions only mutate an existing editor
// session AFTER the selected chart/audio is fully validated.
struct EditorFileContext {
    core::ChartEditor& editor;
    EditorUiState& ui;
    LocalMusic& music;
    ChartPlayback& preview;
    std::wstring& chartPath;
    std::wstring& audioPath;
    std::wstring& loadedAudioPath;
    int& previewStartBeat;
    std::string& status;
    SongLibrary& library;
    SongSelectUi& libraryUi;
    std::string& menuStatus;
};

// Native file picker and monotonic clock are provided by application
// composition. Tests inject deterministic pick results without a window.
struct EditorFilePickers {
    MusicPickResult (*chooseChart)();
    MusicPickResult (*chooseAudio)();
    MusicPickResult (*chooseSave)(const std::wstring&);
    double (*now)();
};

class EditorFileActions {
public:
    EditorFileActions(EditorFileContext context, EditorFilePickers pickers) noexcept
        : context_(context), pickers_(pickers) {}

    void NewDraft();
    void Open(double& now, std::string& error);
    void AttachAudio(double& now);
    bool Save(bool saveAs, double& now);

    // Same-song comparison panes can become editable without losing the
    // active draft. Every visited difficulty owns an independent Undo stack.
    bool FocusChart(const std::filesystem::path& file, std::string& error);
    const core::ChartEditor* ParkedEditor(const std::filesystem::path& file) const;
    bool AnyDirty() const noexcept;
    bool RequestCloseComparison(const std::filesystem::path& file);
    const std::filesystem::path& PendingClose() const noexcept { return pendingClose_; }
    bool ResolveClose(bool save, bool discard, double& now);
    bool SaveAll(double& now);


    // Managed set of the current installed chart, or a new unsaved difficulty.
    // Empty for an unrelated external chart / an unattached new song.
    std::filesystem::path ActiveSetFolder() const;

    // The library already selected a specific installed difficulty. No
    // native dialog; caller decides whether to navigate to Editor.
    bool OpenFromLibrary(const std::wstring& chartFile, std::string& error);
    // Start a blank difficulty using an installed Song Set's shared audio.
    bool NewDifficultyFromLibrary(const std::wstring& audioFile,
                                  const std::wstring& setFolder,
                                  std::string& error);

private:
    EditorFileContext context_;
    EditorFilePickers pickers_;
    std::wstring newDifficultySet_; // Empty means create/find a set from attached audio.
    struct ParkedDocument {
        core::ChartEditor editor;
        EditorUiState ui;
        std::wstring chartPath, audioPath, loadedAudioPath, newDifficultySet;
        int previewBeat = 0;
    };
    std::map<std::filesystem::path, ParkedDocument> parked_;
    std::filesystem::path pendingClose_;

    bool SaveParked(const std::filesystem::path& file, double& now);
    void DiscardParked() noexcept;

};

} // namespace locus::app
