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
#include <vector>

namespace locus::app {
namespace {
// A pattern is an editor-wide transfer buffer, not part of the document.
// Opening another difficulty must clear its selection and transient widgets,
// but not discard the pattern copied from the previous difficulty.
void ResetDocumentUi(EditorUiState& ui) {
    auto clipboard = std::move(ui.clipboard);
    ui = EditorUiState{};
    ui.clipboard = std::move(clipboard);
}
} // namespace

void EditorFileActions::NewDraft() {
    newDifficultySet_.clear();
    context_.preview.Stop();
    context_.music.Shutdown();
    context_.loadedAudioPath.clear();
    context_.editor.NewDraft();
    DiscardParked();
    ResetDocumentUi(context_.ui);
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
    DiscardParked();
    newDifficultySet_.clear();
    context_.loadedAudioPath = context_.audioPath;
    ResetDocumentUi(context_.ui);
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
    DiscardParked();
    newDifficultySet_.clear();
    ResetDocumentUi(context_.ui);
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
    DiscardParked();
    ResetDocumentUi(context_.ui);
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

bool EditorFileActions::Save(bool saveAs, double& now) {
    if (context_.preview.Playing())
        context_.preview.Pause(now);
    if (!context_.music.Ready() || context_.audioPath.empty()) {
        context_.status = "SAVE FAILED: ATTACH AUDIO FIRST";
        return false;
    }
    std::string error;
    core::LoadedChart checked = context_.editor.Data();
    if (!core::PrepareChartForSong(checked, context_.music.DurationSeconds(), error)) {
        context_.status = "SAVE FAILED: " + error;
        return false;
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
            return false;
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
                return false;
            }
        }
        const bool moved = destination != current;
        if (moved) {
            std::filesystem::rename(current, destination, ec);
            if (ec) {
                context_.status = "SAVE FAILED: CANNOT RENAME CHART: " + ec.message();
                return false;
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
            return false;
        }
        context_.chartPath = savedPath;
        std::string notice;
        if (!context_.library.Refresh(notice)) {
            context_.status = "SAVED (LIBRARY REFRESH FAILED: " + notice + ")";
            return false;
        }
    } else {
        const std::filesystem::path target = !newDifficultySet_.empty()
            ? std::filesystem::path(newDifficultySet_)
            : installed ? current.parent_path() : std::filesystem::path{};
        if (!context_.library.CreateDifficulty(
                std::filesystem::path(context_.audioPath), checked,
                destination, error, target)) {
            context_.status = "SAVE FAILED: " + error;
            return false;
        }
        core::LoadedChart reopened;
        std::wstring installedAudio;
        if (!OpenChartWithAudioPath(destination.wstring(),
                                     reopened, installedAudio, error) ||
            !core::PrepareChartForSong(reopened,
                                       context_.music.DurationSeconds(), error)) {
            context_.status = "LIBRARY SAVE OK, REOPEN FAILED: " + error;
            return false;
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
    return true;
}


void EditorFileActions::DiscardParked() noexcept {
    parked_.clear();
    pendingClose_.clear();
}

const core::ChartEditor* EditorFileActions::ParkedEditor(
    const std::filesystem::path& file) const {
    const auto it = parked_.find(file);
    return it == parked_.end() ? nullptr : &it->second.editor;
}

bool EditorFileActions::AnyDirty() const noexcept {
    if (context_.editor.Dirty()) return true;
    for (const auto& entry : parked_)
        if (entry.second.editor.Dirty()) return true;
    return false;
}

bool EditorFileActions::FocusChart(const std::filesystem::path& file,
                                   std::string& error) {
    namespace fs = std::filesystem;
    const fs::path current(context_.chartPath);
    if (file.empty() || current.empty()) {
        error = "Save the new difficulty before switching focus.";
        return false;
    }
    if (current == file) { error.clear(); return true; }

    // Comparison editing shares exactly one decoder and one Song Set.
    int fromSong = -1, fromChart = -1, toSong = -1, toChart = -1;
    if (!context_.library.FindChart(current, fromSong, fromChart) ||
        !context_.library.FindChart(file, toSong, toChart) ||
        fromSong < 0 || fromSong != toSong) {
        error = "Comparison editing requires difficulties in one installed Song Set.";
        return false;
    }

    auto references = context_.ui.comparisonFiles;
    references.erase(std::remove(references.begin(), references.end(), file),
                     references.end());
    if (std::find(references.begin(), references.end(), current) ==
        references.end()) {
        if (references.size() >= 3) {
            error = "MAX FOUR OPEN CHARTS - CLOSE A REFERENCE FIRST";
            return false;
        }
        references.push_back(current);
    }

    // Never abandon a dirty editor until the target has been fully validated.
    ParkedDocument target;
    const auto existing = parked_.find(file);
    if (existing == parked_.end()) {
        const auto& canonical = context_.library.Songs()[
            static_cast<std::size_t>(toSong)].charts[
            static_cast<std::size_t>(toChart)].data;
        context_.preview.Stop();
        if (!LoadDraftFromDisk(file.wstring(), target.editor, context_.music,
                               target.chartPath, target.audioPath, error,
                               &canonical))
            return false;
        target.loadedAudioPath = target.audioPath;
        target.previewBeat = target.editor.SelectedBeat();
    } else {
        context_.preview.Stop();
        target = std::move(existing->second);
        parked_.erase(existing);
    }

    ParkedDocument old;
    old.editor = std::move(context_.editor);
    old.ui = std::move(context_.ui);
    old.chartPath = std::move(context_.chartPath);
    old.audioPath = std::move(context_.audioPath);
    old.loadedAudioPath = std::move(context_.loadedAudioPath);
    old.previewBeat = context_.previewStartBeat;
    old.newDifficultySet = std::move(newDifficultySet_);

    // Clipboard and comparison membership are workspace-level values.
    auto clipboard = std::move(old.ui.clipboard);
    parked_.insert_or_assign(current, std::move(old));
    context_.editor = std::move(target.editor);
    context_.ui = std::move(target.ui);
    context_.chartPath = std::move(target.chartPath);
    context_.audioPath = std::move(target.audioPath);
    context_.loadedAudioPath = std::move(target.loadedAudioPath);
    context_.previewStartBeat = target.previewBeat;
    newDifficultySet_ = std::move(target.newDifficultySet);
    context_.ui.clipboard = std::move(clipboard);
    context_.ui.comparisonFiles = std::move(references);
    context_.ui.comparisonPickerOpen = false;
    context_.ui.tab = EditorTab::Route;
    context_.status = "FOCUS: " + context_.editor.Data().title +
        (context_.editor.Dirty() ? " (UNSAVED)" : "") +
        " - CTRL+S SAVES THIS CHART";
    error.clear();
    return true;
}

bool EditorFileActions::HasDirtyParked() const noexcept {
    for (const auto& entry : parked_)
        if (entry.second.editor.Dirty()) return true;
    return false;
}

void EditorFileActions::DropCleanParked() noexcept {
    // A Song Set BPM/offset change invalidates the grid projections in
    // cached drafts. Unsaved parked editors must be handled before publish.
    for (auto it = parked_.begin(); it != parked_.end();) {
        if (!it->second.editor.Dirty()) it = parked_.erase(it);
        else ++it;
    }
}

void EditorFileActions::ForgetChart(
    const std::filesystem::path& file) noexcept {
    parked_.erase(file);
    auto& comparisons = context_.ui.comparisonFiles;
    comparisons.erase(std::remove(comparisons.begin(), comparisons.end(),
                                  file), comparisons.end());
    if (pendingClose_ == file) pendingClose_.clear();
}

bool EditorFileActions::RequestCloseComparison(
    const std::filesystem::path& file) {
    const auto found = std::find(context_.ui.comparisonFiles.begin(),
        context_.ui.comparisonFiles.end(), file);
    if (found == context_.ui.comparisonFiles.end() || !pendingClose_.empty())
        return false;
    const auto parked = parked_.find(file);
    if (parked != parked_.end() && parked->second.editor.Dirty()) {
        pendingClose_ = file;
        return false;
    }
    parked_.erase(file);
    context_.ui.comparisonFiles.erase(found);
    return true;
}

bool EditorFileActions::SaveParked(const std::filesystem::path& file,
                                   double& now, std::filesystem::path* updatedFile) {
    const std::filesystem::path original(context_.chartPath);
    std::string error;
    if (!FocusChart(file, error)) {
        context_.status = "SAVE FAILED: " + error;
        return false;
    }
    const bool saved = Save(false, now);
    const std::filesystem::path savedPath(context_.chartPath);
    std::string restoreError;
    if (!FocusChart(original, restoreError)) {
        context_.status = "FOCUS RESTORE FAILED: " + restoreError;
        return false;
    }
    if (updatedFile) *updatedFile = savedPath;
    return saved;
}

bool EditorFileActions::ResolveClose(bool save, bool discard, double& now) {
    if (pendingClose_.empty()) return false;
    const auto file = pendingClose_;
    if (!save && !discard) {
        pendingClose_.clear();
        return true;
    }
    std::filesystem::path savedPath = file;
    if (save && !SaveParked(file, now, &savedPath))
        return false; // Leave dialog open if writing to disk failed.
    const auto removeReference = [&](const std::filesystem::path& candidate) {
        auto& references = context_.ui.comparisonFiles;
        references.erase(std::remove(references.begin(), references.end(),
                                     candidate), references.end());
        parked_.erase(candidate);
    };
    removeReference(file);
    removeReference(savedPath);
    pendingClose_.clear();
    context_.ui.comparisonPickerOpen = false;
    context_.status = save ? "CHART SAVED AND CLOSED"
                           : "CHART CLOSED WITHOUT SAVING";
    return true;
}

bool EditorFileActions::SaveAll(double& now) {
    // Ordinary Ctrl+S writes ONLY the focused document. Batch save is used
    // solely when the user explicitly chooses Save in a leave/quit dialog.
    if (context_.editor.Dirty() && !Save(false, now)) return false;
    std::vector<std::filesystem::path> dirty;
    for (const auto& entry : parked_)
        if (entry.second.editor.Dirty()) dirty.push_back(entry.first);
    for (const auto& file : dirty)
        if (parked_.count(file) && !SaveParked(file, now)) return false;
    return !AnyDirty();
}

} // namespace locus::app
