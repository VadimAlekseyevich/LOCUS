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
    if (changed)
        action.historyOperation = undo ? EditorHistoryOperation::Undo
                                       : EditorHistoryOperation::Redo;
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
    if (ui.editingSetPreview || ui.tempoEditingRow >= 0) return action;

    if (ui.EditingText())
        return editor_text::HandleTextInput(draft, ui, input);

    // Comparison picker owns its mouse and keyboard input until dismissed.
    // A click outside it may close the popup, but must NEVER trigger a
    // direction change, transport action or destructive pattern command.
    // EditorSetController handles the picker after this pure editing pass.
    if (ui.tab == EditorTab::Route &&
        (ui.comparisonPickerOpen ||
         (input.mouseClick && Hit(kCompareToggle, input.mouse))))
        return action;

    // The fixed, whole-song strip owns pointer dragging on every editor tab.
    // A seek only moves the editor cursor; it never authors or selects notes.
    if (ui.draggingSongOverview) {
        if (input.mouseHeld || input.mouseReleased)
            editor_timeline::SeekSongOverview(draft, ui, input.mouse.x);
        if (input.mouseReleased || !input.mouseHeld)
            ui.draggingSongOverview = false;
        return action;
    }
    if (input.mouseClick && Hit(kSongOverview, input.mouse)) {
        ui.draggingSongOverview = input.mouseHeld;
        editor_timeline::SeekSongOverview(draft, ui, input.mouse.x);
        return action;
    }

    // The rate selector and transport are shared by every editor tab.
    if (ui.rateDropdownOpen && !input.nativeChrome) {
        if (input.escape) { ui.rateDropdownOpen = false; return action; }
        if (input.mouseClick) {
            for (int i = 0; i < 5; ++i)
                if (Hit(EditorRateOption(i), input.mouse)) {
                    action.speedIndex = i;
                    break;
                }
            ui.rateDropdownOpen = false;
            return action;
        }
    }
    if (!input.nativeChrome && input.mouseClick &&
        Hit(kEditorRateDropdown, input.mouse)) {
        ui.rateDropdownOpen = true;
        return action;
    }
    if (!input.nativeChrome && input.mouseClick &&
        Hit(kEditorPreviewPlay, input.mouse)) {
        action.previewToggle = true;
        return action;
    }
    if (!input.nativeChrome && input.mouseClick &&
        Hit(kEditorPreviewRestart, input.mouse)) {
        action.previewRestart = true;
        return action;
    }
    // The current Route time readout, not individual timeline beats, copies
    // a single exact audio timestamp as whole milliseconds.
    if (!input.nativeChrome && input.mouseClick &&
        Hit(kRouteTimeValue, input.mouse)) {
        const long long ms = RouteTimeMs(draft, input.timelineViewBeat);
        action.clipboardWrite = std::to_string(ms);
        ui.feedback = std::to_string(ms) + " MS COPIED";
        ui.feedbackError = false;
        return action;
    }

    // The native AUDIO marker table handles its own clicks after this pass.
    // Dropdown popup options overlap that table visually and take priority.
    if (ui.tab == EditorTab::Audio && input.mouseClick &&
        (Hit(kTempoTable, input.mouse) ||
         Hit(kTempoAdd, input.mouse) ||
         Hit(kTempoPrev, input.mouse) ||
         Hit(kTempoNext, input.mouse))) return action;

    // RmlUi time button and legacy time hitbox share one exact clock action.
    if (input.copySongTime) {
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
        if (input.mouseClick && !input.nativeChrome) {
            const EditorTab next = ClickedTab(input.mouse, ui.tab);
            if (next != ui.tab) { ui.tab = next; ui.rateDropdownOpen = false; }
        }
        action.previewToggle |= input.previewToggle;
        action.previewRestart |= input.previewRestart;
        if (input.speedIndex >= 0) action.speedIndex = input.speedIndex;
        return action;
    }

    // Undo/redo also cover field edits, history of notes and grid changes.
    if (input.undo || input.redo) {
        ApplyHistoryAction(draft, ui, input.undo, action);
        return action;
    }

    if ((ui.tab == EditorTab::Route || ui.tab == EditorTab::Field) &&
        (input.snapMore || input.snapLess)) {
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
        const EditorTab nextTab = input.nativeChrome
            ? ui.tab : ClickedTab(mouse, ui.tab);
        if (nextTab != ui.tab) {
            ui.comparisonPickerOpen = false;
            ui.rateDropdownOpen = false;
            ui.tab = nextTab;
            return action;
        }
        const EditorField selected = editor_text::ClickedField(mouse, ui.tab);
        if (selected != EditorField::None) {
            editor_text::BeginField(ui, draft, selected);
            return action;
        }

        if (ui.tab == EditorTab::Route && Hit(kSnapLess, mouse)) { editor_timeline::ChangeSnap(draft, ui, -1, action); return action; }
        if (ui.tab == EditorTab::Route && Hit(kSnapMore, mouse)) { editor_timeline::ChangeSnap(draft, ui, 1, action); return action; }
        if (ui.tab == EditorTab::Audio && Hit(kSong, mouse))
            action.chooseAudio = true;
        else if (ui.tab == EditorTab::Route && Hit(kRouteTest, mouse))
            action.testPlay = true;
        else if (ui.tab == EditorTab::Route && Hit(kZoomOut, mouse))
            ui.timelineZoomIndex = std::max(0, ui.timelineZoomIndex - 1);
        else if (ui.tab == EditorTab::Route && Hit(kZoomIn, mouse))
            ui.timelineZoomIndex = std::min(kZoomMax, ui.timelineZoomIndex + 1);
        else if (ui.tab == EditorTab::Route && Hit(kMirrorHorizontal, mouse))
            commands.mirrorH = true;
        else if (ui.tab == EditorTab::Route && Hit(kMirrorVertical, mouse))
            commands.mirrorV = true;
        else if (ui.tab == EditorTab::Route &&
                 Hit(kRotateCounterclockwise, mouse))
            commands.rotateCCW = true;
        else if (ui.tab == EditorTab::Route && Hit(kRotateClockwise, mouse))
            commands.rotateCW = true;
        else if (ui.tab == EditorTab::Route && Hit(kStartHeadingPrev, mouse)) {
            draft.RotateHeading45(-1);
            action.changedChart = true;
        } else if (ui.tab == EditorTab::Route &&
                   (Hit(kStartHeadingValue, mouse) ||
                    Hit(kStartHeadingNext, mouse))) {
            draft.RotateHeading45(1);
            action.changedChart = true;
        } else if (ui.tab == EditorTab::Route &&
                   mouse.x >= 748.0f && mouse.x <= 932.0f &&
                   mouse.y >= 181.0f && mouse.y <= 356.0f) {
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
        } else if (mouse.y >= kTimelineY && mouse.y <= kTimelineY + kTimelineHeight &&
                   mouse.x >= kTimelineX &&
                   mouse.x <= kTimelineX + kTimelineWidth) {
            editor_timeline::BeginSelection(draft, ui, input);
        }
    }

    editor_timeline::UpdateNavigation(draft, ui, input);

    if (ui.tab == EditorTab::Route)
        editor_pattern::Apply(draft, ui, input, commands, action);
    editor_timeline::ApplyWheel(draft, input.mouseWheel);
    action.previewToggle |= input.previewToggle;
    action.previewRestart |= input.previewRestart;
    if (input.speedIndex >= 0) action.speedIndex = input.speedIndex;
    return action;
}

} // namespace locus::app
