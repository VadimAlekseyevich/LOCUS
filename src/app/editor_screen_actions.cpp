#include "app/editor_screen_actions.h"

#include "app/editor_file_actions.h"
#include "app/editor_set_controller.h"
#include "app/chart_editor_ui.h"

namespace locus::app {

bool EditorScreenActions::Apply(const FrameInput& input, double& now) {
    const EditorUiAction uiAction = context_.session.ApplyControls(
        input.editor, now);
    // SET has independent hitboxes and imported native dialogs; route input
    // cannot place notes while this tab is active.
    if (context_.ui.tab == EditorTab::Set) {
        context_.sets.HandleClick(input.editor.mouse,
                                  input.editor.mouseClick,
                                  input.editor.mouseWheel);
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
