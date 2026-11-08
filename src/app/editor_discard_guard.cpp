#include "app/editor_discard_guard.h"

namespace locus::app {
namespace {

bool WantsToLeave(const FrameInput& input) noexcept {
    return input.homePressed || input.backPressed ||
           input.editorNew || input.editorOpen ||
           (input.requestedScreen && *input.requestedScreen != Screen::Editor);
}

void SuppressModalInput(FrameInput& input) noexcept {
    // Discard-confirmation has the same exclusive-input rule as OS exit:
    // never let a coincident test-play, music picker or editor note through.
    // The approved command is reconstructed after suppression.
    input = FrameInput{};
}

} // namespace

EditorDiscardOutcome EditorDiscardGuard::Handle(
    FrameInput& input, bool dirty, bool approve, bool cancel) noexcept {
    // Native X/Alt+F4 has its own quit warning (including unsaved changes).
    // Never stack an editor-discard prompt beneath it.
    if (input.closeRequested) {
        pending_ = EditorPending::None;
        return EditorDiscardOutcome::None;
    }

    if (pending_ != EditorPending::None) {
        const EditorPending requested = pending_;
        // While the modal is open, no unrelated navigation can escape.
        SuppressModalInput(input);
        if (approve) {
            pending_ = EditorPending::None;
            switch (requested) {
                case EditorPending::Home: input.homePressed = true; break;
                case EditorPending::NewDraft: input.editorNew = true; break;
                case EditorPending::OpenChart: input.editorOpen = true; break;
                case EditorPending::None: break;
            }
            return EditorDiscardOutcome::Approved;
        }
        if (cancel) {
            pending_ = EditorPending::None;
            return EditorDiscardOutcome::ChangesKept;
        }
        return EditorDiscardOutcome::None;
    }

    if (!dirty || !WantsToLeave(input))
        return EditorDiscardOutcome::None;

    // The original priority is New Draft > Open Chart > leaving Editor.
    pending_ = input.editorNew ? EditorPending::NewDraft
             : input.editorOpen ? EditorPending::OpenChart
                                : EditorPending::Home;
    SuppressModalInput(input);
    return EditorDiscardOutcome::PromptOpened;
}

} // namespace locus::app
