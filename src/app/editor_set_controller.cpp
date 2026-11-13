#include "app/editor_set_controller.h"

#include "app/chart_editor_layout.h"
#include "app/editor_comparison.h"
#include "app/editor_tempo_controls.h"
#include "app/editor_file_actions.h"
#include "app/image_asset_decode.h"
#include "app/menu_ui.h"
#include "app/open_folder.h"
#include "app/song_library.h"
#include "app/song_set_storage.h"
#include "core/chart_editor.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <string>

namespace locus::app {
namespace {
namespace fs = std::filesystem;

const LibrarySong* FindSet(const SongLibrary& library, const fs::path& folder) {
    if (folder.empty()) return nullptr;
    for (const auto& song : library.Songs())
        if (song.managed && song.folder == folder) return &song;
    return nullptr;
}

bool ReplacesDraft(EditorSetAction action) {
    // Open only switches focus and parks the current draft.
    return action == EditorSetAction::NewDifficulty ||
           action == EditorSetAction::Duplicate ||
           action == EditorSetAction::DeleteSongSet;
}

} // namespace

bool ParseSongPreviewMs(std::string_view input, int& value) noexcept {
    if (input.empty() || input.size() > 7) return false;
    int parsed = 0;
    for (unsigned char ch : input) {
        if (ch < '0' || ch > '9') return false;
        parsed = parsed * 10 + (ch - '0');
    }
    if (parsed > kMaxSongPreviewStartMs) return false;
    value = parsed;
    return true;
}

bool EditorSetNeedsConfirmation(const EditorSetRequest& request) noexcept {
    return request.discardsDraft ||
           request.action == EditorSetAction::DeleteDifficulty ||
           request.action == EditorSetAction::DeleteSongSet;
}

EditorSetView EditorSetController::View() const {
    EditorSetView view;
    const fs::path folder = ctx_.files.ActiveSetFolder();
    const LibrarySong* set = FindSet(ctx_.library, folder);
    if (!set) return view;

    view.managed = true;
    view.title = set->title;
    view.artist = set->artist;
    view.mapper = set->mapper;
    view.tags = set->tags;
    view.audioName = set->audioFile.filename().u8string();
    view.id = set->id;
    view.newDifficultyDraft = ctx_.chartPath.empty();
    view.previewStartMs = set->previewStartMs;
    view.bpm = set->bpm;
    view.offsetMs = set->offsetMs;
    view.tempoMap = set->tempoMap;
    view.hasBackground = !set->backgroundImage.empty();
    view.backgroundPath = set->backgroundImage;
    for (std::size_t i = 0; i < set->charts.size(); ++i) {
        view.difficulties.push_back(set->charts[i].data.title);
        view.comparisonOptions.push_back({
            set->charts[i].file, set->charts[i].data.title,
            &set->charts[i].data, set->charts[i].indexedFileSize,
            set->charts[i].indexedWriteTime});
        if (!ctx_.chartPath.empty() &&
            set->charts[i].file == fs::path(ctx_.chartPath))
            view.current = static_cast<int>(i);
    }
    const int count = static_cast<int>(view.difficulties.size());
    if (count == 0) return view;
    view.selected = ctx_.ui.setSelected < 0
        ? std::max(0, view.current)
        : std::clamp(ctx_.ui.setSelected, 0, count - 1);
    view.firstVisible = std::clamp(ctx_.ui.setFirstVisible, 0,
                                   std::max(0, count - 5));
    if (ctx_.ui.setSelected < 0) {
        if (view.selected < view.firstVisible)
            view.firstVisible = view.selected;
        if (view.selected >= view.firstVisible + 5)
            view.firstVisible = view.selected - 4;
    }
    return view;
}

bool EditorSetController::ApplyTempoMap(
    const std::vector<core::TempoMarker>& markers) {
    if (ctx_.files.HasDirtyParked()) {
        Notice("SAVE OR CLOSE UNSAVED COMPARISON CHARTS BEFORE CHANGING BPM",
               true);
        return false;
    }
    std::string error;
    if (!core::ValidTempoMap(markers, error)) {
        Notice(error, true);
        return false;
    }
    // Validate an isolated draft BEFORE touching the shared manifest:
    // no note timestamps or current editor history may be destroyed.
    core::ChartEditor validation;
    validation.Load(ctx_.editor.Data());
    if (ctx_.editor.HasSongDuration() &&
        !validation.SetSongDuration(ctx_.editor.SongDurationSeconds())) {
        Notice("INVALID AUDIO DURATION", true);
        return false;
    }
    if (!validation.SetTempoMap(markers)) {
        Notice("BPM CHANGE WOULD MERGE OR STRAND NOTES", true);
        return false;
    }
    const auto& songs = ctx_.library.Songs();
    const auto folder = ctx_.files.ActiveSetFolder();
    const auto it = std::find_if(songs.begin(), songs.end(),
        [&](const LibrarySong& song) {
            return song.managed && song.folder == folder;
        });
    const bool managed = it != songs.end();
    if (managed) {
        const int index = static_cast<int>(it - songs.begin());
        if (!ctx_.library.SetSongTempoMap(index, markers,
                                          &ctx_.editor.Data(), error)) {
            Notice(error, true);
            return false;
        }
    }
    const int oldTicks = ctx_.editor.Data().route.ticksPerBeat;
    if (!ctx_.editor.SetTempoMap(markers)) {
        Notice("UNEXPECTED EDITOR TEMPO PROJECTION FAILURE", true);
        return false;
    }
    if (ctx_.editor.Data().route.ticksPerBeat != oldTicks)
        ctx_.ui.rangeFirst = ctx_.ui.rangeLast = -1;
    ctx_.ui.draggingTimeline = false;
    ctx_.files.DropCleanParked();
    Notice(managed ? "TEMPO MAP UPDATED FOR SONG SET"
                   : "DRAFT TEMPO MAP UPDATED");
    return true;
}

bool EditorSetController::SyncSongTiming(EditorHistoryOperation operation) {
    const auto folder = ctx_.files.ActiveSetFolder();
    const auto& songs = ctx_.library.Songs();
    const auto it = std::find_if(songs.begin(), songs.end(),
        [&](const LibrarySong& song) {
            return song.managed && song.folder == folder;
        });
    if (it == songs.end()) return true;
    const auto tempo = core::EffectiveTempoMap(
        ctx_.editor.Data().route, ctx_.editor.Data().offsetSeconds);
    if (tempo == it->tempoMap) return true;
    if (ctx_.files.HasDirtyParked()) {
        const bool restored = operation == EditorHistoryOperation::Undo
            ? ctx_.editor.Redo() : ctx_.editor.Undo();
        Notice(restored
            ? "SAVE OR CLOSE UNSAVED COMPARISON CHARTS BEFORE CHANGING BPM"
            : "UNSAVED COMPARISON CHART BLOCKED BPM (UNDO FAILED)", true);
        return false;
    }
    std::string error;
    if (!ctx_.library.SetSongTempoMap(
            static_cast<int>(it - songs.begin()), tempo,
            &ctx_.editor.Data(), error)) {
        // A failed publish must leave the editor exactly as it was before
        // this action. Calling Undo() after the user's Undo would instead
        // roll back an unrelated older edit and corrupt the working state.
        const bool restored = operation == EditorHistoryOperation::Undo
            ? ctx_.editor.Redo() : ctx_.editor.Undo();
        Notice(restored ? error : error + " (EDITOR HISTORY RESTORE FAILED)",
               true);
        return false;
    }
    ctx_.ui.setSelected = -1;
    ctx_.files.DropCleanParked();
    Notice("SONG SET TEMPO MAP UPDATED");
    return true;
}

void EditorSetController::Notice(const std::string& message, bool failed) {
    ctx_.editorStatus = (failed ? "SET FAILED: " : "SET: ") + message;
    ctx_.ui.feedback = ctx_.editorStatus;
    ctx_.ui.feedbackError = failed;
}

void EditorSetController::HandleControls(const EditorControls& input) {
    if (pending_) return;
    auto& ui = ctx_.ui;
    const Vector2 mouse = input.mouse;
    const bool clicked = input.mouseClick;
    if (ui.tab == EditorTab::Route) {
        if (ui.active != EditorField::None) return;
        if (input.mouseClick) {
            const EditorSetView view = View();
            // A preview click changes focus; this frame must not also edit
            // the old chart. The top ruler and controls subsequently use
            // the newly focused ChartEditor and its independent history.
            if (!ui.comparisonPickerOpen) {
                const int count = 1 + static_cast<int>(ui.comparisonFiles.size());
                const int focusedSlot = std::clamp(ui.comparisonFocusedSlot, 0,
                                                    count - 1);
                for (int i = 0; i < count; ++i) {
                    if (i == focusedSlot ||
                        !detail::Hit(detail::ComparisonPreviewBounds(count, i),
                                     input.mouse)) continue;
                    const int refIndex = ComparisonReferenceIndexForSlot(
                        i, focusedSlot);
                    const fs::path chosen = ui.comparisonFiles[
                        static_cast<std::size_t>(refIndex)];
                    std::string problem;
                    if (!ctx_.files.FocusChart(chosen, problem))
                        Notice(problem, true);
                    return;
                }
            } else {
                for (int row = 0; row < 5; ++row) {
                    if (!detail::Hit(detail::ComparePickerRow(row),
                                     input.mouse)) continue;
                    const int index = ui.comparisonPickerFirst + row;
                    if (index < 0 ||
                        index >= static_cast<int>(view.comparisonOptions.size()))
                        break;
                    const fs::path file = view.comparisonOptions[
                        static_cast<std::size_t>(index)].file;
                    if (HasEditorComparison(ui, file)) {
                        ctx_.files.RequestCloseComparison(file);
                        ui.comparisonPickerOpen = false;
                        return;
                    }
                    break;
                }
            }
        }
        HandleEditorComparisonControls(ui, View(), input);
        return;
    }
    if (ui.tab != EditorTab::Set && ui.tab != EditorTab::Audio) return;
    if (ui.tab == EditorTab::Audio && !ui.editingSetPreview) {
        const EditorSetView view = View();
        const auto tempo = view.managed && !view.tempoMap.empty()
            ? view.tempoMap
            : core::EffectiveTempoMap(ctx_.editor.Data().route,
                                       ctx_.editor.Data().offsetSeconds);
        if (HandleTempoTable(ui, input, tempo, ctx_.editor,
                             [&](const std::vector<core::TempoMarker>& markers) {
                                 return ApplyTempoMap(markers);
                             })) return;
    }
    if (ui.editingSetPreview) {
        if (input.escape) {
            ui.editingSetPreview = false;
            ui.setPreviewBuffer.clear();
            ui.feedback.clear();
            return;
        }
        if (input.backspace) {
            if (ui.setPreviewReplaceOnType) ui.setPreviewBuffer.clear();
            else if (!ui.setPreviewBuffer.empty()) ui.setPreviewBuffer.pop_back();
            ui.setPreviewReplaceOnType = false;
        }
        for (unsigned char ch : input.typedUtf8) {
            if (ui.setPreviewReplaceOnType) {
                ui.setPreviewBuffer.clear();
                ui.setPreviewReplaceOnType = false;
            }
            // Invalid pasted punctuation stays visible and fails validation.
            if (ui.setPreviewBuffer.size() < 24)
                ui.setPreviewBuffer.push_back(static_cast<char>(ch));
        }
        if (input.enter ||
            (clicked && !CheckCollisionPointRec(mouse, detail::kAudioPreviewMs))) {
            int parsed = 0;
            if (!ParseSongPreviewMs(ui.setPreviewBuffer, parsed)) {
                Notice("PREVIEW MUST BE WHOLE MS (0..3600000)", true);
                return;
            }
            EditorSetRequest request;
            request.action = EditorSetAction::SetPreviewExact;
            request.folder = ctx_.files.ActiveSetFolder();
            request.previewMs = parsed;
            if (!Execute(request)) return; // Keep the text for correction.
            ui.editingSetPreview = false;
            ui.setPreviewBuffer.clear();
            ui.setPreviewReplaceOnType = false;
            if (clicked) ui.tab = detail::ClickedTab(mouse, ui.tab);
        }
        return;
    }
    // Only a manually typed preview position is editable in AUDIO.
    if (ui.tab == EditorTab::Audio) {
        if (clicked && ui.active == EditorField::None &&
            CheckCollisionPointRec(mouse, detail::kAudioPreviewMs)) {
            const EditorSetView view = View();
            if (!view.managed) {
                Notice("SAVE SONG FIRST (CTRL+S)", true);
                return;
            }
            ui.editingSetPreview = true;
            ui.setPreviewReplaceOnType = true;
            ui.setPreviewBuffer = std::to_string(view.previewStartMs);
            ui.feedback = "PREVIEW MS / CTRL+V PASTE / ENTER SAVE / ESC CANCEL";
            ui.feedbackError = false;
        }
        return;
    }
    EditorSetView view = View();
    const int count = static_cast<int>(view.difficulties.size());
    const float wheel = input.mouseWheel;
    if (wheel != 0.0f && count > 0) {
        ctx_.ui.setSelected = view.selected; // Browsing need not follow active.
        const int delta = wheel > 0.0f ? -1 : 1;
        ctx_.ui.setFirstVisible = std::clamp(
            view.firstVisible + delta, 0, std::max(0, count - 5));
    }
    if (!clicked) return;
    for (int row = 0; row < 5; ++row) {
        const int index = view.firstVisible + row;
        if (index >= count) break;
        if (CheckCollisionPointRec(mouse, detail::SetDifficultyRow(row))) {
            ctx_.ui.setSelected = index;
            // A row click changes focus within this shared Song Set.
            // Dirty drafts remain parked and are not discarded.
            if (index != view.current) {
                std::string problem;
                const fs::path file = view.comparisonOptions[
                    static_cast<std::size_t>(index)].file;
                if (!ctx_.files.FocusChart(file, problem))
                    Notice(problem, true);
            }
            return;
        }
    }
    const EditorSetAction action = detail::ClickedSetAction(mouse);
    if (action != EditorSetAction::None)
        Request(action);
}

void EditorSetController::Request(EditorSetAction action) {
    if (pending_ || action == EditorSetAction::None) return;
    const fs::path folder = ctx_.files.ActiveSetFolder();
    const LibrarySong* song = FindSet(ctx_.library, folder);
    EditorSetRequest request;
    request.action = action;
    request.folder = folder;
    if (action != EditorSetAction::Import &&
        action != EditorSetAction::Restore && !song) {
        Notice("SAVE YOUR SONG FIRST (CTRL+S) OR IMPORT A SONG SET", true);
        return;
    }
    if (song && (action == EditorSetAction::Open ||
                 action == EditorSetAction::Duplicate ||
                 action == EditorSetAction::DeleteDifficulty)) {
        const EditorSetView view = View();
        if (view.selected < 0 ||
            view.selected >= static_cast<int>(song->charts.size())) {
            Notice("SELECT A DIFFICULTY FIRST", true);
            return;
        }
        request.chart = song->charts[static_cast<std::size_t>(
            view.selected)].file;
        if (action == EditorSetAction::Open &&
            request.chart == fs::path(ctx_.chartPath)) {
            Notice("THIS DIFFICULTY IS ALREADY OPEN");
            return;
        }
    }
    const bool affectsCurrent = request.action == EditorSetAction::DeleteDifficulty &&
                                request.chart == fs::path(ctx_.chartPath);
    const auto* parked = ctx_.files.ParkedEditor(request.chart);
    const bool deletesUnsaved = action == EditorSetAction::DeleteDifficulty &&
        ((affectsCurrent && ctx_.editor.Dirty()) ||
         (parked && parked->Dirty()));
    request.discardsDraft = deletesUnsaved ||
        (ctx_.files.AnyDirty() &&
         (ReplacesDraft(action) || affectsCurrent));
    if (EditorSetNeedsConfirmation(request)) {
        pending_ = std::move(request);
        return;
    }
    Execute(request);
}

void EditorSetController::Confirm(bool approve, bool cancel) {
    if (!pending_) return;
    if (cancel) {
        pending_.reset();
        Notice("ACTION CANCELED; YOUR CHART IS UNCHANGED");
        return;
    }
    if (!approve) return;
    const EditorSetRequest saved = *pending_;
    pending_.reset();
    Execute(saved);
}

bool EditorSetController::Execute(const EditorSetRequest& request) {
    const fs::path folder = request.folder;
    const LibrarySong* song = FindSet(ctx_.library, folder);
    std::string error;
    switch (request.action) {
        case EditorSetAction::OpenFolder: {
            if (!song) { error = "Song Set no longer exists."; break; }
            if (!OpenFolderInExplorer(folder, error)) break;
            Notice("SONG SET FOLDER OPENED");
            return true;
        }
        case EditorSetAction::SetPreviewExact:
        case EditorSetAction::AttachBackground:
        case EditorSetAction::RemoveBackground: {
            if (!song) { error = "Song Set no longer exists."; break; }
            const auto& all = ctx_.library.Songs();
            const auto it = std::find_if(all.begin(), all.end(),
                [&](const LibrarySong& candidate) {
                    return candidate.managed && candidate.folder == folder;
                });
            if (it == all.end()) {
                error = "Song Set no longer exists.";
                break;
            }
            const int index = static_cast<int>(std::distance(all.begin(), it));
            if (request.action == EditorSetAction::SetPreviewExact) {
                const int next = request.previewMs;
                if (!ctx_.library.SetPreviewStartMs(index, next, error)) break;
                Notice("PREVIEW FROM " + std::to_string(next) + " MS");
                return true;
            }
            if (request.action == EditorSetAction::RemoveBackground) {
                if (!ctx_.library.SetBackgroundImage(index, {}, error)) break;
                Notice("SONG BACKGROUND REMOVED");
                return true;
            }
            if (!pickers_.chooseBackground) {
                error = "Background file picker unavailable.";
                break;
            }
            const auto picked = pickers_.chooseBackground();
            if (picked.canceled) return true;
            if (picked.path.empty()) {
                error = picked.error.empty() ? "No image selected." : picked.error;
                break;
            }
            // Validate real decoded pixels BEFORE copying and publishing
            // the Song Set manifest. A valid header alone is not sufficient.
            Image candidate{};
            if (!DecodeImageAssetFile(fs::path(picked.path), candidate, error))
                break;
            UnloadImage(candidate);
            if (!ctx_.library.SetBackgroundImage(index, fs::path(picked.path), error))
                break;
            Notice("SONG BACKGROUND SAVED (ALL DIFFICULTIES)");
            return true;
        }
        case EditorSetAction::Open: {
            if (!ctx_.files.FocusChart(request.chart, error)) break;
            ctx_.ui.setSelected = -1;
            Notice("DIFFICULTY FOCUSED - CTRL+S SAVES ONLY THIS CHART");
            return true;
        }
        case EditorSetAction::NewDifficulty: {
            if (!song) { error = "Song Set no longer exists."; break; }
            const fs::path audio = song->audioFile;
            if (!ctx_.files.NewDifficultyFromLibrary(
                    audio.wstring(), folder.wstring(), error)) break;
            ctx_.ui.tab = EditorTab::Set;
            ctx_.ui.setSelected = -1;
            Notice("NEW DIFFICULTY - CTRL+S TO SAVE");
            return true;
        }
        case EditorSetAction::Duplicate: {
            fs::path copied;
            if (!ctx_.library.DuplicateDifficulty(request.chart, copied, error)) break;
            if (!ctx_.files.OpenFromLibrary(copied.wstring(), error)) {
                Notice("COPY CREATED; OPEN FAILED: " + error, true);
                return false;
            }
            ctx_.ui.tab = EditorTab::Set;
            ctx_.ui.setSelected = -1;
            Notice("DIFFICULTY COPIED AND OPENED");
            return true;
        }
        case EditorSetAction::DeleteDifficulty: {
            const bool active = request.chart == fs::path(ctx_.chartPath);
            if (!ctx_.library.TrashDifficulty(request.chart, error)) break;
            ctx_.files.ForgetChart(request.chart);
            if (active) {
                const auto* updated = FindSet(ctx_.library, folder);
                if (updated && !updated->charts.empty()) {
                    const auto candidate = updated->charts.front().file;
                    if (!ctx_.files.OpenFromLibrary(candidate.wstring(), error)) {
                        // The deleted file cannot remain the active draft.
                        ctx_.files.NewDraft();
                        ctx_.ui.tab = EditorTab::Set;
                        Notice("DELETED; REOPEN NEXT DIFFICULTY: " + error, true);
                        return false;
                    }
                    ctx_.ui.tab = EditorTab::Set;
                } else {
                    ctx_.files.NewDraft();
                    ctx_.ui.tab = EditorTab::Set;
                }
            }
            ctx_.ui.setSelected = -1;
            Notice("DIFFICULTY MOVED TO TRASH - RESTORE LAST TO UNDO");
            return true;
        }
        case EditorSetAction::DeleteSongSet: {
            if (!ctx_.library.TrashSongSet(folder, error)) break;
            ctx_.files.NewDraft();
            ctx_.ui.tab = EditorTab::Set;
            Notice("SONG SET MOVED TO TRASH - RESTORE LAST TO UNDO");
            return true;
        }
        case EditorSetAction::Restore: {
            if (!ctx_.library.RestoreLastDeleted(error)) break;
            Notice("LAST DELETED ITEM RESTORED");
            return true;
        }
        case EditorSetAction::Export: {
            if (!song) { error = "Song Set no longer exists."; break; }
            if (!pickers_.chooseExport) { error = "Export file dialog unavailable."; break; }
            const auto selected = pickers_.chooseExport(
                (song->folder.filename().wstring() + L".locuspack"));
            if (selected.canceled) return true;
            if (selected.path.empty()) {
                error = selected.error.empty() ? "No destination selected." : selected.error;
                break;
            }
            if (!ctx_.library.ExportPackage(folder, fs::path(selected.path), error))
                break;
            Notice(ctx_.editor.Dirty()
                ? "SAVED VERSION EXPORTED (UNSAVED EDITS EXCLUDED)"
                : "SONG SET EXPORTED");
            return true;
        }
        case EditorSetAction::Import: {
            if (!pickers_.chooseImport) { error = "Import file dialog unavailable."; break; }
            const auto picked = pickers_.chooseImport();
            if (picked.canceled) return true;
            if (picked.path.empty()) {
                error = picked.error.empty() ? "No file selected." : picked.error;
                break;
            }
            const fs::path source(picked.path);
            if (source.extension() == L".locuspack") {
                fs::path importedSet;
                if (!ctx_.library.ImportPackage(source, importedSet, error)) break;
                Notice("SONG SET IMPORTED; OPEN IT IN PLAY LIBRARY");
                return true;
            }
            if (!song) {
                fs::path installed;
                if (!ctx_.library.Import(source, installed, error)) break;
                Notice("CHART IMPORTED; OPEN ITS SET IN PLAY LIBRARY");
                return true;
            }
            fs::path installed;
            if (!ctx_.library.ImportIntoSet(source, folder, installed, error))
                break;
            Notice("DIFFICULTY IMPORTED; SELECT IT IN THE LIST");
            return true;
        }
        case EditorSetAction::None:
            return false;
    }
    Notice(error.empty() ? "OPERATION FAILED" : error, true);
    return false;
}

} // namespace locus::app
