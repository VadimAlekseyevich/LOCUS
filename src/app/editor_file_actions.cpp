#include "app/editor_file_actions.h"

#include "app/chart_editor_ui.h"
#include "app/chart_playback.h"
#include "app/chart_file.h"
#include "app/chart_filename.h"
#include "app/editor_document.h"
#include "app/local_music.h"
#include "app/menu_ui.h"
#include "app/song_library.h"
#include "app/song_set_storage.h"
#include "core/chart_editor.h"
#include "core/chart_json.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace locus::app {

void EditorFileActions::NewDraft() {
    newDifficultySet_.clear();
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
    int songIndex = -1, difficultyIndex = -1;
    const core::LoadedChart* canonical = nullptr;
    if (context_.library.FindChart(std::filesystem::path(chartFile),
                                   songIndex, difficultyIndex)) {
        canonical = &context_.library.Songs()[
            static_cast<std::size_t>(songIndex)].charts[
            static_cast<std::size_t>(difficultyIndex)].data;
    }
    // One commit point: failed audio or Song Set timing cannot erase a draft.
    context_.preview.Stop();
    if (!LoadDraftFromDisk(
            chartFile, context_.editor, context_.music,
            context_.chartPath, context_.audioPath, error, canonical))
        return false;
    newDifficultySet_.clear();
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
    int librarySong = -1, libraryChart = -1;
    if (context_.library.FindChart(std::filesystem::path(picked.path),
                                   librarySong, libraryChart)) {
        if (!OpenFromLibrary(picked.path, error)) {
            context_.status = "OPEN FAILED: " + error;
            return;
        }
        return;
    }
    if (!LoadDraftFromDisk(
            picked.path, context_.editor, context_.music,
            context_.chartPath, context_.audioPath, error)) {
        context_.status = "OPEN FAILED: " + error;
        return;
    }
    newDifficultySet_.clear();
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

bool EditorFileActions::NewDifficultyFromLibrary(
    const std::wstring& audioFile, const std::wstring& setFolder,
    std::string& error) {
    // Fully decode before discarding the editor's previous document.
    LocalMusic replacement;
    replacement.SetVolume(context_.music.Volume());
    replacement.SetSpeed(context_.music.Speed(), 0.0);
    if (!replacement.LoadFromFile(audioFile)) {
        error = replacement.Error();
        return false;
    }
    const auto shared = std::find_if(context_.library.Songs().begin(),
        context_.library.Songs().end(), [&](const LibrarySong& candidate) {
            return candidate.managed &&
                   candidate.folder == std::filesystem::path(setFolder);
        });
    std::vector<core::TempoMarker> timing;
    if (shared != context_.library.Songs().end()) {
        timing = shared->tempoMap.empty()
            ? std::vector<core::TempoMarker>{{shared->offsetMs, shared->bpm}}
            : shared->tempoMap;
    }
    core::ChartEditor draft;
    if (!PrepareNewDifficultyDraft(
            draft, replacement.DurationSeconds(),
            std::filesystem::path(audioFile).filename().u8string(),
            timing, error))
        return false;
    context_.preview.Stop();
    context_.music.SwapWith(replacement);
    context_.editor = std::move(draft);
    context_.ui = EditorUiState{};
    context_.chartPath.clear();
    context_.audioPath = audioFile;
    context_.loadedAudioPath = audioFile;
    context_.previewStartBeat = 0;
    newDifficultySet_ = setFolder;
    context_.status = "NEW DIFFICULTY - CTRL+S TO ADD TO SONG SET";
    error.clear();
    return true;
}

std::filesystem::path EditorFileActions::ActiveSetFolder() const {
    if (!newDifficultySet_.empty())
        return std::filesystem::path(newDifficultySet_);
    int song = -1, chart = -1;
    if (!context_.chartPath.empty() &&
        context_.library.FindChart(
            std::filesystem::path(context_.chartPath), song, chart) &&
        song >= 0 &&
        song < static_cast<int>(context_.library.Songs().size()) &&
        context_.library.Songs()[static_cast<std::size_t>(song)].managed)
        return context_.library.Songs()[static_cast<std::size_t>(song)].folder;
    return {};
}

void EditorFileActions::Save(bool saveAs, double& now) {
    if (context_.preview.Playing())
        context_.preview.Pause(now);
    if (!context_.music.Ready() || context_.audioPath.empty()) {
        context_.status = "SAVE FAILED: ATTACH AUDIO FIRST";
        return;
    }
    std::string error;
    core::LoadedChart checked = context_.editor.Data();
    if (!core::PrepareChartForSong(checked, context_.music.DurationSeconds(), error)) {
        context_.status = "SAVE FAILED: " + error;
        return;
    }
    const std::filesystem::path current(context_.chartPath);
    int songIndex = -1, chartIndex = -1;
    // Library paths are authoritative. Do not create a duplicate external
    // authoring file on every save, or an implicit second library import.
    const bool installed = !current.empty() &&
        context_.library.FindChart(current, songIndex, chartIndex);
    std::filesystem::path destination;
    if (installed && !saveAs) {
        // Hold both locks through the rename and atomic save.
        FolderWriteLock libraryLock, setLock;
        if (!libraryLock.Acquire(context_.library.Root(), error) ||
            !setLock.Acquire(current.parent_path(), error)) {
            context_.status = "SAVE FAILED: " + error;
            return;
        }
        destination = current;
        const auto& stored = context_.library.Songs()[
            static_cast<std::size_t>(songIndex)].charts[
            static_cast<std::size_t>(chartIndex)];
        std::error_code ec;
        if (checked.title != stored.data.title) {
            destination = RenamedChartFilePath(current, checked.title, ec);
            if (ec || destination.empty()) {
                context_.status = "SAVE FAILED: CANNOT ALLOCATE CHART FILENAME";
                return;
            }
        }
        const bool moved = destination != current;
        if (moved) {
            std::filesystem::rename(current, destination, ec);
            if (ec) {
                context_.status = "SAVE FAILED: CANNOT RENAME CHART: " + ec.message();
                return;
            }
        }
        std::wstring savedPath = destination.wstring();
        if (!SaveDraftToDisk(context_.editor, context_.music,
                             savedPath, context_.audioPath,
                             false, error, pickers_.chooseSave, true)) {
            if (moved) {
                std::error_code rollback;
                std::filesystem::rename(destination, current, rollback);
                if (rollback) error += " (rename rollback failed: " +
                                       rollback.message() + ")";
            }
            context_.status = "SAVE FAILED: " + error;
            return;
        }
        context_.chartPath = savedPath;
        std::string notice;
        if (!context_.library.Refresh(notice)) {
            context_.status = "SAVED (LIBRARY REFRESH FAILED: " + notice + ")";
            return;
        }
    } else {
        const std::filesystem::path target = !newDifficultySet_.empty()
            ? std::filesystem::path(newDifficultySet_)
            : installed ? current.parent_path() : std::filesystem::path{};
        if (!context_.library.CreateDifficulty(
                std::filesystem::path(context_.audioPath), checked,
                destination, error, target)) {
            context_.status = "SAVE FAILED: " + error;
            return;
        }
        core::LoadedChart reopened;
        std::wstring installedAudio;
        if (!OpenChartWithAudioPath(destination.wstring(),
                                     reopened, installedAudio, error) ||
            !core::PrepareChartForSong(reopened,
                                       context_.music.DurationSeconds(), error)) {
            context_.status = "LIBRARY SAVE OK, REOPEN FAILED: " + error;
            return;
        }
        context_.editor.ApplySavedChart(std::move(reopened));
        context_.chartPath = destination.wstring();
        context_.audioPath = installedAudio;
        context_.loadedAudioPath = installedAudio;
        newDifficultySet_.clear();
    }
    int s = -1, c = -1;
    if (context_.library.FindChart(destination, s, c)) {
        context_.ui.setSelected = -1; // Follow the saved chart after resorting.
        // Keep this headless file-action module independent of menu renderer.
        context_.libraryUi.selectedSong = s + 1;
        context_.libraryUi.selectedChart = c;
        context_.libraryUi.firstVisibleSong = std::max(0, s + 1 - 5);
        context_.libraryUi.firstVisibleChart = std::max(0, c - 4);
    }
    context_.libraryUi.notice = "SONG SET SAVED";
    context_.menuStatus = "LIBRARY: " +
        std::to_string(context_.library.Songs().size()) + " SONGS / " +
        std::to_string(context_.library.ChartCount()) + " CHARTS";
    context_.status = "SAVED IN SONG LIBRARY - CTRL+S UPDATE / CTRL+SHIFT+S COPY";
    now = pickers_.now();
}

} // namespace locus::app
