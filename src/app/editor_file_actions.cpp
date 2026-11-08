#include "app/editor_file_actions.h"

#include "app/chart_editor_ui.h"
#include "app/chart_playback.h"
#include "app/editor_document.h"
#include "app/local_music.h"
#include "app/menu_ui.h"
#include "app/song_library.h"
#include "core/chart_editor.h"
#include "core/chart_json.h"

#include <filesystem>
#include <string>
#include <utility>

namespace locus::app {

void EditorFileActions::NewDraft() {
    context_.preview.Stop();
    context_.music.Shutdown();
    context_.loadedAudioPath.clear();
    context_.editor.NewDraft();
    context_.ui = EditorUiState{};
    context_.chartPath.clear();
    context_.audioPath.clear();
    context_.previewStartBeat = 0;
    context_.status = "NEW CHART - CLICK LOAD SONG";
}

bool EditorFileActions::OpenFromLibrary(const std::wstring& chartFile,
                                        std::string& error) {
    context_.preview.Stop();
    if (!LoadDraftFromDisk(
            chartFile, context_.editor, context_.music,
            context_.chartPath, context_.audioPath, error))
        return false;
    context_.loadedAudioPath = context_.audioPath;
    context_.ui = EditorUiState{};
    context_.previewStartBeat = context_.editor.SelectedBeat();
    context_.status = "LIBRARY CHART READY - EDIT AND CTRL+S";
    return true;
}

void EditorFileActions::Open(double& now, std::string& error) {
    context_.preview.Stop();
    const auto picked = pickers_.chooseChart();
    now = pickers_.now();
    if (picked.path.empty()) {
        if (!picked.canceled)
            context_.status = "OPEN FAILED: " + picked.error;
        return;
    }
    if (!LoadDraftFromDisk(
            picked.path, context_.editor, context_.music,
            context_.chartPath, context_.audioPath, error)) {
        context_.status = "OPEN FAILED: " + error;
        return;
    }
    context_.ui = EditorUiState{};
    context_.previewStartBeat = 0;
    context_.loadedAudioPath = context_.audioPath;
    context_.status = "CHART LOADED - SPACE PREVIEW / T TEST";
}

void EditorFileActions::AttachAudio(double& now) {
    context_.preview.Stop();
    const auto picked = pickers_.chooseAudio();
    now = pickers_.now();
    if (picked.path.empty()) {
        if (!picked.canceled)
            context_.status = "SONG PICKER FAILED: " + picked.error;
        return;
    }

    // This operation must preserve the previous document if decoding or
    // timeline validation fails. Prepare everything in temporary objects.
    LocalMusic replacement;
    replacement.SetVolume(context_.music.Volume());
    replacement.SetSpeed(context_.music.Speed(), now);
    if (!replacement.LoadFromFile(picked.path)) {
        context_.status = "SONG FAILED: " + replacement.Error();
        return;
    }
    core::ChartEditor prepared = context_.editor;
    const std::string name =
        std::filesystem::path(picked.path).filename().u8string();
    core::LoadedChart validated = context_.editor.Data();
    std::string error;
    if (!prepared.SetSongDuration(replacement.DurationSeconds()) ||
        !core::PrepareChartForSong(validated, replacement.DurationSeconds(), error) ||
        !prepared.SetAudioPath(name)) {
        context_.status = "SONG NOT CHANGED: " +
            (error.empty() ? std::string("CHART DOES NOT FIT SELECTED AUDIO")
                           : error);
        return;
    }
    context_.music.SwapWith(replacement);
    context_.editor = std::move(prepared);
    context_.audioPath = picked.path;
    context_.loadedAudioPath = picked.path;
    context_.status = "SONG LOADED - TIMELINE FITS THE WHOLE TRACK";
}

void EditorFileActions::Save(bool saveAs, double& now) {
    if (context_.preview.Playing())
        context_.preview.Pause(now);
    std::string error;
    if (!SaveDraftToDisk(
            context_.editor, context_.music, context_.chartPath,
            context_.audioPath, saveAs, error, pickers_.chooseSave)) {
        if (!error.empty()) context_.status = "SAVE FAILED: " + error;
        now = pickers_.now();
        return;
    }
    now = pickers_.now();
    context_.status = "SAVED - SPACE PREVIEW / T TEST / CTRL+O OPEN";

    // A saved authoring file remains where the user chose, while a validated
    // permanent copy is installed independently in the song library.
    std::filesystem::path installed;
    std::string installError;
    if (context_.library.Import(
            std::filesystem::path(context_.chartPath), installed, installError)) {
        context_.libraryUi.notice = "SAVED CHART INSTALLED IN SONG LIBRARY";
        context_.menuStatus =
            "LIBRARY: " + std::to_string(context_.library.Songs().size()) +
            " SONGS / " + std::to_string(context_.library.ChartCount()) + " CHARTS";
    } else {
        context_.libraryUi.notice = "LIBRARY IMPORT FAILED: " + installError;
        context_.status = "SAVED (LIBRARY IMPORT FAILED - SEE PLAY)";
    }
}

} // namespace locus::app
