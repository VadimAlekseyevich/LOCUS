#pragma once
#include "app/screen_flow.h"
namespace locus::app {
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
