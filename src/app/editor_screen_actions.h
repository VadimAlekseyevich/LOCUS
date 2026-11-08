#pragma once

#include "app/app_input.h"
#include "app/editor_session.h"

#include <string>

namespace locus::app {
class EditorFileActions;
class EditorSetController;

// Pure precedence rule: no UI, decoder or file picker required.
enum class EditorFileCommand {
    None, NewDraft, OpenChart, AttachAudio, Save, SaveAs, TestPlay
};
inline EditorFileCommand ChooseEditorCommand(
    const FrameInput& input, const EditorUiAction& uiAction) noexcept {
    if (input.editorNew) return EditorFileCommand::NewDraft;
    if (input.editorOpen) return EditorFileCommand::OpenChart;
    if (input.chooseMusicPressed || uiAction.chooseAudio)
        return EditorFileCommand::AttachAudio;
    if (input.editorSave || input.editorSaveAs)
        return input.editorSaveAs
            ? EditorFileCommand::SaveAs : EditorFileCommand::Save;
    if (input.editorTest || uiAction.testPlay)
        return EditorFileCommand::TestPlay;
    return EditorFileCommand::None;
}

// Only the Editor screen's file/UI actions. No AppState, renderer, transport
// ownership or game navigation is held; true asks caller to enter Play.
struct EditorScreenContext {
    EditorSession& session;
    EditorFileActions& files;
    EditorSetController& sets;
    EditorUiState& ui;
    EditorTestTarget testTarget;
    std::string& error;
    double (*clock)();
};

class EditorScreenActions {
public:
    explicit EditorScreenActions(EditorScreenContext context) noexcept
        : context_(context) {}
    bool Apply(const FrameInput& input, double& now);

private:
    EditorScreenContext context_;
};
} // namespace locus::app
