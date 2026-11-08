#include "app/editor_set_controller.h"

#include "app/chart_editor_layout.h"
#include "app/editor_file_actions.h"
#include "app/menu_ui.h"
#include "app/song_library.h"
#include "core/chart_editor.h"

#include <algorithm>
#include <filesystem>
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
    return action == EditorSetAction::Open ||
           action == EditorSetAction::NewDifficulty ||
           action == EditorSetAction::Duplicate ||
           action == EditorSetAction::DeleteSongSet;
}

} // namespace

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
    view.audioName = set->audioFile.filename().u8string();
    view.id = set->id;
    view.newDifficultyDraft = ctx_.chartPath.empty();
    for (std::size_t i = 0; i < set->charts.size(); ++i) {
        view.difficulties.push_back(set->charts[i].data.title);
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

void EditorSetController::Notice(const std::string& message, bool failed) {
    ctx_.editorStatus = (failed ? "SET FAILED: " : "SET: ") + message;
    ctx_.ui.feedback = ctx_.editorStatus;
    ctx_.ui.feedbackError = failed;
}

void EditorSetController::HandleClick(Vector2 mouse, bool clicked, float wheel) {
    if (ctx_.ui.tab != EditorTab::Set || pending_) return;
    EditorSetView view = View();
    const int count = static_cast<int>(view.difficulties.size());
    if (wheel != 0.0f && count > 0) {
        ctx_.ui.setSelected = view.selected; // Browsing need not follow active.
        const int delta = wheel > 0.0f ? -1 : 1;
        ctx_.ui.setFirstVisible = std::clamp(
            view.firstVisible + delta, 0, std::max(0, count - 5));
    }
    if (!clicked) return;
    if (CheckCollisionPointRec(mouse, detail::kSetPrevious)) {
        ctx_.ui.setSelected = view.selected;
        ctx_.ui.setFirstVisible = std::max(0, view.firstVisible - 5);
        return;
    }
    if (CheckCollisionPointRec(mouse, detail::kSetNext)) {
        ctx_.ui.setSelected = view.selected;
        ctx_.ui.setFirstVisible = std::min(
            std::max(0, count - 5), view.firstVisible + 5);
        return;
    }
    for (int row = 0; row < 5; ++row) {
        const int index = view.firstVisible + row;
        if (index >= count) break;
        if (CheckCollisionPointRec(mouse, detail::SetDifficultyRow(row))) {
            ctx_.ui.setSelected = index;
            return;
        }
    }
    const EditorSetAction action = detail::ClickedSetAction(mouse);
    if (action != EditorSetAction::None) Request(action);
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
    request.discardsDraft = ctx_.editor.Dirty() &&
        (ReplacesDraft(action) || affectsCurrent);
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
        case EditorSetAction::Open: {
            if (!ctx_.files.OpenFromLibrary(request.chart.wstring(), error)) break;
            ctx_.ui.tab = EditorTab::Set;
            ctx_.ui.setSelected = -1;
            Notice("DIFFICULTY OPENED");
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
            if (active) {
                const auto* updated = FindSet(ctx_.library, folder);
                if (updated && !updated->charts.empty()) {
                    const auto candidate = updated->charts.front().file;
                    if (!ctx_.files.OpenFromLibrary(candidate.wstring(), error)) {
                        Notice("DELETED; OPEN ANOTHER DIFFICULTY: " + error, true);
                        return false;
                    }
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
