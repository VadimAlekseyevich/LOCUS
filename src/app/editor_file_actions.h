#pragma once

#include "app/music_picker.h"

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
    void Save(bool saveAs, double& now);

    // The library already selected a specific installed difficulty. No
    // native dialog; caller decides whether to navigate to Editor.
    bool OpenFromLibrary(const std::wstring& chartFile, std::string& error);

private:
    EditorFileContext context_;
    EditorFilePickers pickers_;
};

} // namespace locus::app
