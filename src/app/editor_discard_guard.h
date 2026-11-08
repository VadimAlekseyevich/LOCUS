#pragma once

#include "app/app_input.h"

namespace locus::app {

// Confirmation for abandoning an unsaved chart when leaving Editor, opening
// another file, or starting a blank draft. The global quit confirmation is
// separate and takes priority over this editor-specific modal.
enum class EditorPending { None, Home, NewDraft, OpenChart };
enum class EditorDiscardOutcome { None, PromptOpened, ChangesKept, Approved };

class EditorDiscardGuard {
public:
    EditorPending Pending() const noexcept { return pending_; }

    // Called only on the Editor screen, before UpdateScreenFlow. UI supplies
    // Y/N/Esc events; this class filters all frame actions so only an
    // explicitly approved action can run while the modal is active.
    EditorDiscardOutcome Handle(FrameInput& input, bool dirty,
                                bool approve, bool cancel) noexcept;

private:
    EditorPending pending_ = EditorPending::None;
};

} // namespace locus::app
