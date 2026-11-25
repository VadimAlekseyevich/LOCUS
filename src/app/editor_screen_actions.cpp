#include "app/editor_screen_actions.h"

#include "app/editor_file_actions.h"
#include "app/editor_set_controller.h"
#include "app/chart_editor_ui.h"
#include "app/chart_editor_layout.h"
#include "core/tempo_map.h"
#include "raylib.h"

#include <algorithm>

namespace locus::app {

void EditorScreenActions::BlurTextOnPointer(Vector2 mouse, double& now) {
    auto& ui = context_.ui;
    if (!ui.EditingText()) return;
    const bool inside = ui.active == EditorField::Title
        ? detail::Hit(detail::kTitle, mouse)
        : ui.editingSetField != EditorSetField::None
        ? detail::Hit(detail::SetMetadataBounds(ui.editingSetField), mouse)
        : ui.editingSetPreview ? detail::Hit(detail::kAudioPreviewMs, mouse)
        : ui.tempoEditingRow >= 0 &&
          detail::Hit(ui.tempoEditingBpm
              ? detail::TempoBpmCell(ui.tempoEditingRow - ui.tempoFirstVisible)
              : detail::TempoOffsetCell(ui.tempoEditingRow - ui.tempoFirstVisible), mouse);
    if (inside) return;
    EditorControls commit;
    commit.enter = true;
    if (ui.active == EditorField::Title) {
        const auto value = ui.buffer;
        ui.active = EditorField::None;
        ui.buffer.clear();
        ui.replaceOnType = false;
        context_.sets.CommitDifficultyTitle(value, now);
    } else if (ui.active != EditorField::None) {
        context_.session.ApplyControls(commit, now);
    } else {
        const auto before = context_.session.TempoMap();
        context_.sets.HandleControls(commit);
        if (before != context_.session.TempoMap())
            context_.session.StopPreviewForTempoChange();
    }
    // Invalid input does not trap pointer focus. Keep saved data and feedback.
    ui.active = EditorField::None;
    ui.editingSetField = EditorSetField::None;
    ui.editingSetId.clear();
    ui.setFieldBuffer.clear();
    ui.editingSetPreview = false;
    ui.setPreviewBuffer.clear();
    ui.tempoEditingRow = -1;
    ui.tempoBuffer.clear();
}

bool EditorScreenActions::Apply(const FrameInput& input, double& now) {
    if (input.editor.difficultyTitle)
        context_.sets.CommitDifficultyTitle(*input.editor.difficultyTitle, now);
    if (context_.ui.active == EditorField::Title &&
        (input.editor.enter || input.editor.tab)) {
        const auto value = context_.ui.buffer;
        if (context_.sets.CommitDifficultyTitle(value, now)) {
            context_.ui.active = EditorField::None;
            context_.ui.buffer.clear();
        }
        return false;
    }
    if (input.editor.renameDifficulty >= 0) {
        context_.sets.RenameDifficulty(input.editor.renameDifficulty);
        return false;
    }
    if (input.editor.enterChart ||
        (context_.ui.tab == EditorTab::Set && !input.editor.nativeChrome &&
         input.editor.mouseClick && detail::Hit(detail::kSetupEnter, input.editor.mouse))) {
        context_.sets.EnterChart(now);
        return false;
    }
    // RmlUi SET commands are typed events and take precedence over the
    // legacy pointer path. They are validated by the same domain controller.
    if (input.editor.metadataField) {
        context_.sets.CommitMetadata(*input.editor.metadataField,
            input.editor.metadataValue, input.editor.metadataSetId);
    }
    if (input.editor.selectedDifficulty >= 0) {
        context_.sets.FocusDifficulty(input.editor.selectedDifficulty);
        return false;
    }
    if (input.editor.setRequest) {
        context_.sets.Request(*input.editor.setRequest);
        return false;
    }
    // Process the picker BEFORE the editor: the former implementation
    // passed its click through generic editor actions first, and a popup
    // often never opened. A picker interaction consumes the whole frame.
    bool previewFocusClick = false;
    if (context_.ui.tab == EditorTab::Route &&
        input.editor.mouseClick && !context_.ui.comparisonPickerOpen) {
        const int count = 1 +
            static_cast<int>(context_.ui.comparisonFiles.size());
        const int focusedSlot = std::clamp(
            context_.ui.comparisonFocusedSlot, 0, count - 1);
        for (int i = 0; i < count; ++i)
            if (i != focusedSlot)
                previewFocusClick |= detail::Hit(
                    detail::ComparisonPreviewBounds(count, i),
                    input.editor.mouse);
    }
    const bool compareInteraction =
        context_.ui.tab == EditorTab::Route &&
        (context_.ui.comparisonPickerOpen || previewFocusClick ||
         (input.editor.mouseClick &&
          detail::Hit(detail::kCompareToggle, input.editor.mouse)));
    if (compareInteraction) {
        context_.sets.HandleControls(input.editor);
        return false;
    }
    // Metadata text owns all keystrokes and mouse focus changes until
    // committed/canceled. Never route these keys into chart note editing.
    if (context_.ui.tab == EditorTab::Set &&
        (context_.ui.editingSetField != EditorSetField::None ||
         (input.editor.mouseClick &&
          !input.editor.nativeChrome && detail::ClickedSetMetadataField(input.editor.mouse) !=
              EditorSetField::None))) {
        context_.sets.HandleControls(input.editor);
        return false;
    }
    // SET row focus clicks are exclusive too. A simultaneous Ctrl+S must
    // never accidentally write whichever chart was clicked in that frame.
    if (context_.ui.tab == EditorTab::Set && input.editor.mouseClick && !input.editor.nativeChrome) {
        const EditorSetView set = context_.sets.View();
        for (int row = 0; row < 5; ++row) {
            const int index = set.firstVisible + row;
            if (index >= static_cast<int>(set.difficulties.size())) break;
            if (detail::Hit(detail::SetDifficultyRow(row),input.editor.mouse) ||
                detail::Hit(detail::SetDifficultyRenameRow(row),input.editor.mouse)) {
                context_.sets.HandleControls(input.editor);
                return false;
            }
        }
    }
    // Footer controls and the expanded playback-rate popup own their click.
    // Its options overlap AUDIO's tempo cells, but must never edit BPM.
    const bool footerClick = input.editor.mouseClick &&
        !input.editor.nativeChrome &&
        (context_.ui.rateDropdownOpen ||
         detail::Hit(detail::kEditorRateDropdown,input.editor.mouse) ||
         detail::Hit(detail::kEditorPreviewPlay,input.editor.mouse) ||
         detail::Hit(detail::kEditorPreviewRestart,input.editor.mouse) ||
         detail::Hit(detail::kRouteTimeValue,input.editor.mouse));
    const auto beforeTempo = context_.session.TempoMap();
    const EditorUiAction uiAction = context_.session.ApplyControls(
        input.editor, now);
    if (!uiAction.clipboardWrite.empty())
        SetClipboardText(uiAction.clipboardWrite.c_str());
    // BPM/offset are Song Set properties. Confirm a field edit by updating
    // its one canonical manifest; never change only this difficulty's timing.
    // Field confirmation may change tabs in the same frame; Undo of a BPM
    // change can also occur on ROUTE. Compare canonical timing on any edit.
    if (uiAction.changedChart &&
        context_.session.TempoMap() != beforeTempo)
        context_.sets.SyncSongTiming(uiAction.historyOperation);
    // ROUTE owns comparison selection; SET owns library tools; AUDIO has
    // the editable Song Set preview start. Selection never edits chart data.
    if (!footerClick &&
        (context_.ui.tab == EditorTab::Route ||
         context_.ui.tab == EditorTab::Set ||
         context_.ui.tab == EditorTab::Audio)) {
        const auto beforeTempo = context_.session.TempoMap();
        context_.sets.HandleControls(input.editor);
        if (context_.session.TempoMap() != beforeTempo)
            context_.session.StopPreviewForTempoChange();
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
            if (context_.ui.tab != EditorTab::Route) return false;
            return context_.session.BeginTest(
                context_.testTarget, now, context_.error);
        case EditorFileCommand::None:
            break;
    }
    return false;
}

} // namespace locus::app
