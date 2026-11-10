#include "app/chart_editor_layout.h"
#include "app/editor_text_fields.h"
#include "app/editor_pattern_actions.h"
#include "app/editor_timeline_actions.h"

#include <algorithm>
#include <string>

namespace locus::app {
using namespace detail;
namespace {

// Keyboard and button Undo/Redo must apply the same tick-domain selection
// cleanup. Otherwise a mouse Undo after a grid change leaves a stale range.
void ApplyHistoryAction(core::ChartEditor& draft, EditorUiState& ui,
                        bool undo, EditorUiAction& action) {
    const int beforeTicks = draft.Data().route.ticksPerBeat;
    const bool changed = undo ? draft.Undo() : draft.Redo();
    action.changedChart = changed;
    ui.feedback = changed
        ? (undo ? "UNDO APPLIED" : "REDO APPLIED")
        : (undo ? "NOTHING TO UNDO" : "NOTHING TO REDO");
    ui.feedbackError = !changed;
    if (changed && beforeTicks != draft.Data().route.ticksPerBeat) {
        ui.rangeFirst = ui.rangeLast = -1;
        ui.draggingTimeline = false;
    } else if (changed && ui.HasRange() &&
               ui.rangeLast >= draft.Data().route.lengthBeats) {
        ui.rangeFirst = ui.rangeLast = -1;
    }
}
} // namespace

EditorUiAction ApplyEditorControls(core::ChartEditor& draft, EditorUiState& ui,
                                  const EditorControls& input) {
    EditorUiAction action;
    const auto notice = [&](const char* message, bool error = false) {
        ui.feedback = message;
        ui.feedbackError = error;
    };

    // Song Set preview time belongs to the Set controller, not the chart.
    // Suppress route/transport shortcuts; the Set controller consumes text.
    if (ui.editingSetPreview) return action;

    if (ui.EditingText())
        return editor_text::HandleTextInput(draft, ui, input);

    // The comparison selector is handled by EditorSetController after this
    // pure edit pass. Do not let popup clicks trigger note or field actions.
    if (ui.tab == EditorTab::Route &&
        ((input.mouseClick && (Hit(kCompareToggle, input.mouse) ||
           (ui.comparisonPickerOpen && Hit(kComparePopup, input.mouse)))) ||
         (ui.comparisonPickerOpen && input.escape)))
        return action;

    // The current Route time readout, not individual timeline beats, copies
    // a single exact audio timestamp as whole milliseconds.
    if (ui.tab == EditorTab::Route && input.mouseClick &&
        Hit(kRouteTimeValue, input.mouse)) {
        const long long ms = RouteTimeMs(draft, input.timelineViewBeat);
        action.clipboardWrite = std::to_string(ms);
        ui.feedback = std::to_string(ms) + " MS COPIED";
        ui.feedbackError = false;
        return action;
    }

    // SET is a library-management tab: note keys, undo and the timeline
    // must not modify the current chart while managing its difficulties.
    if (ui.tab == EditorTab::Set) {
        ui.draggingTimeline = false;
        if (input.mouseClick) {
            const EditorTab next = ClickedTab(input.mouse, ui.tab);
            if (next != ui.tab) ui.tab = next;
            else if (Hit(kSong, input.mouse)) action.chooseAudio = true;
            else if (Hit(kPlay, input.mouse)) action.previewToggle = true;
            else if (Hit(kStop, input.mouse)) action.previewRestart = true;
            else if (Hit(kTest, input.mouse)) action.testPlay = true;
        }
        action.previewToggle |= input.previewToggle;
        action.previewRestart |= input.previewRestart;
        return action;
    }

    // Undo/redo also cover field edits, history of notes and grid changes.
    if (input.undo || input.redo) {
        ApplyHistoryAction(draft, ui, input.undo, action);
        return action;
    }

    if (input.snapMore || input.snapLess) {
        editor_timeline::ChangeSnap(draft, ui, input.snapMore ? 1 : -1, action);
        return action;
    }

    editor_pattern::Commands commands;
    commands.copy = input.copyRange && ui.tab == EditorTab::Route;
    commands.cut = input.cutRange && ui.tab == EditorTab::Route;
    commands.paste = input.pasteRange && ui.tab == EditorTab::Route;
    commands.mirrorH = input.mirrorHorizontal && ui.tab == EditorTab::Route;
    commands.mirrorV = input.mirrorVertical && ui.tab == EditorTab::Route;
    commands.rotateCW = input.rotateClockwise && ui.tab == EditorTab::Route;
    commands.rotateCCW = input.rotateCounterclockwise && ui.tab == EditorTab::Route;
    commands.deleteTurns = input.removeTurn && ui.tab == EditorTab::Route;

    if (input.mouseClick) {
        ui.draggingTimeline = false;
        const Vector2 mouse = input.mouse;
        const EditorTab nextTab = ClickedTab(mouse, ui.tab);
        if (nextTab != ui.tab) {
            ui.comparisonPickerOpen = false;
            ui.tab = nextTab;
            return action;
        }
        const EditorField selected = editor_text::ClickedField(mouse, ui.tab);
        if (selected != EditorField::None) {
            editor_text::BeginField(ui, draft, selected);
            return action;
        }

        if (Hit(kSnapLess, mouse)) { editor_timeline::ChangeSnap(draft, ui, -1, action); return action; }
        if (Hit(kSnapMore, mouse)) { editor_timeline::ChangeSnap(draft, ui, 1, action); return action; }
        const Rectangle song = ui.tab == EditorTab::Route ? kRouteSong : kSong;
        const Rectangle play = ui.tab == EditorTab::Route ? kRoutePlay : kPlay;
        const Rectangle stop = ui.tab == EditorTab::Route ? kRouteStop : kStop;
        const Rectangle test = ui.tab == EditorTab::Route ? kRouteTest : kTest;
        if (Hit(song, mouse)) action.chooseAudio = true;
        else if (Hit(play, mouse)) action.previewToggle = true;
        else if (Hit(stop, mouse)) action.previewRestart = true;
        else if (Hit(test, mouse)) action.testPlay = true;
        else if (Hit(kZoomOut, mouse))
            ui.timelineZoomIndex = std::max(0, ui.timelineZoomIndex - 1);
        else if (Hit(kZoomIn, mouse))
            ui.timelineZoomIndex = std::min(kZoomMax, ui.timelineZoomIndex + 1);
        else if (ui.tab == EditorTab::Route && Hit(kCopyTurns, mouse)) commands.copy = true;
        else if (ui.tab == EditorTab::Route && Hit(kPasteTurns, mouse)) commands.paste = true;
        else if (ui.tab == EditorTab::Route && Hit(kMirrorHorizontal, mouse)) commands.mirrorH = true;
        else if (ui.tab == EditorTab::Route && Hit(kMirrorVertical, mouse)) commands.mirrorV = true;
        else if (ui.tab == EditorTab::Route && Hit(kRotateCounterclockwise, mouse)) commands.rotateCCW = true;
        else if (ui.tab == EditorTab::Route && Hit(kRotateClockwise, mouse)) commands.rotateCW = true;
        else if (ui.tab == EditorTab::Route && Hit(kCutTurns, mouse)) commands.cut = true;
        else if (ui.tab == EditorTab::Route && Hit(kUndoEdit, mouse)) {
            ApplyHistoryAction(draft, ui, true, action);
            return action;
        } else if (ui.tab == EditorTab::Route && Hit(kRedoEdit, mouse)) {
            ApplyHistoryAction(draft, ui, false, action);
            return action;
        } else if (ui.tab == EditorTab::Route && Hit(kHeading, mouse)) {
            draft.RotateHeading45(1);
            action.changedChart = true;
        } else if (ui.tab == EditorTab::Field && Hit(kFieldHeading, mouse)) {
            draft.RotateHeading45(1);
            action.changedChart = true;
        } else if (ui.tab == EditorTab::Route &&
                   mouse.x >= 744.0f && mouse.x <= 908.0f &&
                   mouse.y >= 275.0f && mouse.y <= 388.0f) {
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col) {
                    if (!Hit(DirectionPadBounds(row, col), mouse)) continue;
                    const auto heading = DirectionPadHeading(row, col);
                    if (heading) {
                        const bool changed = draft.SetOutgoingHeading(*heading);
                        action.changedChart |= changed;
                        if (!changed) notice(
                            "NO CHANGE - ALREADY HEADING THAT WAY", true);
                    } else {
                        commands.deleteTurns = true;
                    }
                }
        } else if (ui.tab == EditorTab::Route &&
                   mouse.y >= 116.0f && mouse.y < 143.0f &&
                   mouse.x >= 415.0f && mouse.x < 606.0f) {
            for (int i = 0; i < 5; ++i) {
                if (Hit(RouteSpeedBounds(i), mouse)) {
                    action.speedIndex = i;
                    break;
                }
            }
        } else if (mouse.y >= kTimelineY && mouse.y <= kTimelineY + kTimelineHeight &&
                   mouse.x >= kTimelineX &&
                   mouse.x <= kTimelineX + kTimelineWidth) {
            editor_timeline::BeginSelection(draft, ui, input);
        }
    }

    editor_timeline::UpdateNavigation(draft, ui, input);

    if (ui.tab == EditorTab::Route)
        editor_pattern::Apply(draft, ui, input, commands, action);
    if (ui.tab != EditorTab::Audio) {
        if (input.headingLeft) { draft.RotateHeading45(-1); action.changedChart = true; }
        if (input.headingRight) { draft.RotateHeading45(1); action.changedChart = true; }
    }
    editor_timeline::ApplyWheel(draft, input.mouseWheel);
    action.previewToggle |= input.previewToggle;
    action.previewRestart |= input.previewRestart;
    if (input.speedIndex >= 0) action.speedIndex = input.speedIndex;
    return action;
}

} // namespace locus::app
