#pragma once
#include "app/confirmation_view.h"
#include "app/editor_discard_guard.h"
#include "app/editor_set_controller.h"

namespace locus::app {

// Presentation only: all writes, deletion and draft lifetime remain in
// EditorFileActions, EditorSetController and EditorDiscardGuard.
inline ConfirmationView EditorConfirmationView(const EditorSetRequest* set,
    EditorPending leave, bool closePane, const std::string& status) {
    ConfirmationView view;
    if (!set && !closePane && leave == EditorPending::None) return view;
    view.visible = true;
    view.canSave = !set || set->discardsDraft;
    view.approveLabel = view.canSave ? "Y / DISCARD" : "Y / CONFIRM";
    if (set && set->action == EditorSetAction::DeleteSongSet) {
        view.heading = "DELETE ENTIRE SONG SET?";
        view.message = "Song, audio and all charts go to recoverable trash.";
        if (view.canSave) view.message += " Unsaved edits will be lost unless saved first.";
    } else if (set && set->action == EditorSetAction::DeleteDifficulty) {
        view.heading = "DELETE THIS DIFFICULTY?";
        view.message = "The selected chart goes to recoverable trash.";
        if (view.canSave) view.message += " Unsaved edits will be lost unless saved first.";
    } else {
        view.heading = "UNSAVED CHART CHANGES";
        view.message = closePane ? "Save before closing this chart?"
            : set ? "Save changed charts before switching documents?"
            : "Save changed charts before leaving?";
    }
    if (view.canSave) {
        view.notice = "Failed saves keep unsaved drafts open.";
        if (status.find("SAVE FAILED") != std::string::npos ||
            status.find("FOCUS RESTORE FAILED") != std::string::npos)
            view.notice = status;
    }
    return view;
}
} // namespace locus::app
