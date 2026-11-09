#pragma once
#include "app/screen_flow.h"
namespace locus::app {
// A test is an in-memory view: Esc/Home should reopen its unsaved draft,
// not the last difficulty selected in the Song Library.
inline bool EditorTestExitToEditor(Screen current, bool cameFromEditor,
                                   bool back, bool home) noexcept {
    return cameFromEditor && (back || home) &&
           (current == Screen::Play || current == Screen::Results);
}

// Home creates a fresh editor; explicit library EDIT has already loaded its
// chosen chart. Editor TEST preserves the unsaved draft on return.
enum class EditorEntryAction { KeepLoaded, NewDraft, OpenPlayedDifficulty };
inline EditorEntryAction ChooseEditorEntry(Screen previous, Screen next,
                                           bool playCameFromEditor,
                                           bool customChart,
                                           bool selectedAvailable) noexcept {
    if (next != Screen::Editor) return EditorEntryAction::KeepLoaded;
    if (previous == Screen::Home) return EditorEntryAction::NewDraft;
    if (previous == Screen::Play || previous == Screen::Results) {
        if (playCameFromEditor) return EditorEntryAction::KeepLoaded;
        return customChart && selectedAvailable
            ? EditorEntryAction::OpenPlayedDifficulty
            : EditorEntryAction::NewDraft;
    }
    return EditorEntryAction::KeepLoaded;
}
} // namespace locus::app
