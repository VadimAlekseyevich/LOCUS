#include "app/editor_screen_actions.h"

#include "app/editor_file_actions.h"
#include "app/editor_set_controller.h"
#include "app/chart_editor_ui.h"
#include "app/chart_editor_layout.h"
#include "raylib.h"

namespace locus::app {

bool EditorScreenActions::Apply(const FrameInput& input, double& now) {
    // Process the picker BEFORE the editor: the former implementation
    // passed its click through generic editor actions first, and a popup
    // often never opened. A picker interaction consumes the whole frame.
    const bool compareInteraction =
        context_.ui.tab == EditorTab::Route &&
        (context_.ui.comparisonPickerOpen ||
         (input.editor.mouseClick &&
          detail::Hit(detail::kCompareToggle, input.editor.mouse)));
    if (compareInteraction) {
        context_.sets.HandleControls(input.editor);
        return false;
    }
    const EditorUiAction uiAction = context_.session.ApplyControls(
        input.editor, now);
    if (!uiAction.clipboardWrite.empty())
        SetClipboardText(uiAction.clipboardWrite.c_str());
    // BPM/offset are Song Set properties. Confirm a field edit by updating
    // its one canonical manifest; never change only this difficulty's timing.
    // Field confirmation may change tabs in the same frame; Undo of a BPM
    // change can also occur on ROUTE. Compare canonical timing on any edit.
    if (uiAction.changedChart)
        context_.sets.SyncSongTiming();
    // ROUTE owns comparison selection; SET owns library tools; AUDIO has
    // the editable Song Set preview start. Selection never edits chart data.
    if (context_.ui.tab == EditorTab::Route ||
        context_.ui.tab == EditorTab::Set ||
        context_.ui.tab == EditorTab::Audio) {
        context_.sets.HandleControls(input.editor);
        if (context_.clock) now = context_.clock();
    }

    switch (ChooseEditorCommand(input, uiAction)) {
        case EditorFileCommand::NewDraft:
            context_.files.NewDraft();
            break;
        case EditorFileCommand::OpenChart:
            context_.files.Open(now, context_.error);
            break;
        case EditorFileCommand::AttachAudio:
            context_.files.AttachAudio(now);
            break;
        case EditorFileCommand::Save:
            context_.files.Save(false, now);
            break;
        case EditorFileCommand::SaveAs:
            context_.files.Save(true, now);
            break;
        case EditorFileCommand::TestPlay:
            return context_.session.BeginTest(
                context_.testTarget, now, context_.error);
        case EditorFileCommand::None:
            break;
    }
    return false;
}

} // namespace locus::app
