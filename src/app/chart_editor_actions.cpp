#include "app/chart_editor_layout.h"
#include "app/editor_text_fields.h"
#include "app/editor_pattern_actions.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>

namespace locus::app {
using namespace detail;
namespace {


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
        ui.pendingDirection = 0;
        ui.directionKeysHandled = false;
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

    // Undo/redo also cover non-pattern edits (BPM, grid size, individual turns).
    if (input.undo || input.redo) {
        const int originalTicks = draft.Data().route.ticksPerBeat;
        const bool ok = input.undo ? draft.Undo() : draft.Redo();
        action.changedChart = ok;
        notice(ok ? (input.undo ? "UNDO APPLIED" : "REDO APPLIED")
                  : (input.undo ? "NOTHING TO UNDO" : "NOTHING TO REDO"), !ok);
        if (ok && originalTicks != draft.Data().route.ticksPerBeat) {
            // A range from the old tick domain would select wrong notes.
            ui.rangeFirst = ui.rangeLast = -1;
            ui.draggingTimeline = false;
        } else if (ok && ui.HasRange() &&
                   ui.rangeLast >= draft.Data().route.lengthBeats) {
            ui.rangeFirst = ui.rangeLast = -1;
        }
        return action;
    }

    // Timeline placement grid, independent of the tempo and view zoom.
    const auto changeSnap = [&](int direction) {
        const int oldTicks = draft.Data().route.ticksPerBeat;
        const int prior = draft.Data().route.snapDivision;
        int index = 0;
        while (index < 9 && kSnapDivisions[index] != prior) ++index;
        if (index == 9) return;
        const int next = std::clamp(index + direction, 0, 8);
        if (next == index) return;
        if (!draft.SetSnapDivision(kSnapDivisions[next])) {
            notice("GRID TOO DENSE FOR SONG LENGTH (MAX 65536 TICKS)", true);
            return;
        }
        const int factor = draft.Data().route.ticksPerBeat / oldTicks;
        if (ui.HasRange()) {
            ui.rangeFirst *= factor;
            ui.rangeLast *= factor;
        }
        ui.dragAnchorBeat *= factor;
        ui.dragViewBeat *= factor;
        ui.draggingTimeline = false;
        action.changedChart = true;
        ui.feedback = "GRID 1/" + std::to_string(kSnapDivisions[next]) +
                      " - ORIGINAL NOTES / BPM UNCHANGED";
        ui.feedbackError = false;
    };
    if (input.snapMore || input.snapLess) {
        changeSnap(input.snapMore ? 1 : -1);
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
            ui.tab = nextTab;
            ui.pendingDirection = 0;
            ui.directionKeysHandled = false;
            return action;
        }
        const EditorField selected = editor_text::ClickedField(mouse, ui.tab);
        if (selected != EditorField::None) {
            editor_text::BeginField(ui, draft, selected);
            return action;
        }

        if (Hit(kSnapLess, mouse)) { changeSnap(-1); return action; }
        if (Hit(kSnapMore, mouse)) { changeSnap(1); return action; }
        if (Hit(kSong, mouse)) action.chooseAudio = true;
        else if (Hit(kPlay, mouse)) action.previewToggle = true;
        else if (Hit(kStop, mouse)) action.previewRestart = true;
        else if (Hit(kTest, mouse)) action.testPlay = true;
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
            action.changedChart = draft.Undo();
            notice(action.changedChart ? "UNDO APPLIED" : "NOTHING TO UNDO",
                   !action.changedChart);
            return action;
        } else if (ui.tab == EditorTab::Route && Hit(kRedoEdit, mouse)) {
            action.changedChart = draft.Redo();
            notice(action.changedChart ? "REDO APPLIED" : "NOTHING TO REDO",
                   !action.changedChart);
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
                   mouse.x >= kDefaultEditorRouteLayout.practiceSpeedX &&
                   mouse.x <= kDefaultEditorRouteLayout.practiceSpeedX +
                              kDefaultEditorRouteLayout.practiceSpeedWidth &&
                   mouse.y >= kDefaultEditorRouteLayout.practiceSpeedStartY &&
                   mouse.y < kDefaultEditorRouteLayout.practiceSpeedStartY +
                       5 * (kDefaultEditorRouteLayout.practiceSpeedHeight +
                            kDefaultEditorRouteLayout.practiceSpeedGap)) {
            for (int i = 0; i < 5; ++i) {
                if (Hit(RouteSpeedBounds(i), mouse)) {
                    action.speedIndex = i;
                    break;
                }
            }
        } else if (mouse.y >= kTimelineY && mouse.y <= kTimelineY + kTimelineHeight &&
                   mouse.x >= kTimelineX &&
                   mouse.x <= kTimelineX + kTimelineWidth) {
            const double anchor = ViewBeat(draft, input.timelineViewBeat);
            const double clicked = anchor + (mouse.x - kPlayheadX) /
                                                 PixelsPerTick(ui, draft.Data().route);
            const int beat = SnapTick(clicked, draft.Data().route);
            if (ui.tab == EditorTab::Route && input.shiftHeld) {
                const int origin = draft.SelectedBeat();
                ui.rangeFirst = std::min(origin, beat);
                ui.rangeLast = std::max(origin, beat);
                ui.draggingTimeline = false;
            } else {
                ui.rangeFirst = ui.rangeLast = -1;
                ui.draggingTimeline = ui.tab == EditorTab::Route && input.mouseHeld;
                ui.dragAnchorBeat = beat;
                ui.dragViewBeat = anchor;
            }
            draft.SelectBeat(beat);
        }
    }

    // Dragging past the visible ruler now autoscrolls the *view* as well as
    // extending the selected interval. Keep the beat-to-pixel transform
    // stable while dragging, except for deliberate edge scrolling.
    if (ui.draggingTimeline && (input.mouseHeld || input.mouseReleased)) {
        if (input.mouseHeld && ui.tab == EditorTab::Route) {
            const float left = kTimelineX;
            const float right = kTimelineX + kTimelineWidth;
            const float x = input.mouse.x;
            float edge = 0.0f;
            if (x > right - kDragEdgeZone) {
                edge = (x - (right - kDragEdgeZone)) / kDragEdgeZone;
            } else if (x < left + kDragEdgeZone) {
                edge = (x - (left + kDragEdgeZone)) / kDragEdgeZone;
            }
            if (edge != 0.0f) {
                const float intensity = std::min(3.0f, std::abs(edge));
                const float dt = std::clamp(input.elapsedSeconds, 0.0f, 0.10f);
                const double beatsPerSecond = 8.0 + intensity * 16.0;
                ui.dragViewBeat = std::clamp(
                    ui.dragViewBeat +
                        (edge > 0.0f ? 1.0 : -1.0) * beatsPerSecond * dt *
                        draft.Data().route.ticksPerBeat,
                    static_cast<double>(draft.Data().route.firstBeat),
                    static_cast<double>(draft.Data().route.lengthBeats - 1));
            }
        }
        const float x = std::clamp(input.mouse.x, kTimelineX,
                                    kTimelineX + kTimelineWidth);
        const double point = ui.dragViewBeat +
                             (x - kPlayheadX) / PixelsPerTick(ui, draft.Data().route);
        const int beat = SnapTick(point, draft.Data().route);
        if (beat != ui.dragAnchorBeat) {
            ui.rangeFirst = std::min(ui.dragAnchorBeat, beat);
            ui.rangeLast = std::max(ui.dragAnchorBeat, beat);
        } else {
            ui.rangeFirst = ui.rangeLast = -1;
        }
        draft.SelectBeat(beat);
    }
    if (input.mouseReleased || (!input.mouseHeld && !input.mouseClick))
        ui.draggingTimeline = false;

    if (input.zoomOut) ui.timelineZoomIndex = std::max(0, ui.timelineZoomIndex - 1);
    if (input.zoomIn) ui.timelineZoomIndex = std::min(kZoomMax, ui.timelineZoomIndex + 1);
    if (input.previousBeat) { draft.StepSnapSelection(-1); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.nextBeat) { draft.StepSnapSelection(1); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.previousBar) { draft.StepSelection(-4 * draft.Data().route.ticksPerBeat); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.nextBar) { draft.StepSelection(4 * draft.Data().route.ticksPerBeat); ui.rangeFirst = ui.rangeLast = -1; }
    // Use held key state instead of OS key-repeat: consistent on all PCs.
    // First key-down gets a single step above; continuous steps begin after
    // 230ms, then speed up after 1.2 seconds of holding.
    const int held = std::clamp(input.heldBarDirection, -1, 1);
    if (held != ui.heldBarDirection) {
        ui.heldBarDirection = held;
        ui.heldBarSeconds = 0.0f;
        ui.barRepeatSeconds = 0.0f;
    }
    if (held != 0) {
        const float dt = std::clamp(input.elapsedSeconds, 0.0f, 0.10f);
        const float before = ui.heldBarSeconds;
        ui.heldBarSeconds += dt;
        if (ui.heldBarSeconds >= kBarRepeatDelay) {
            ui.barRepeatSeconds += before < kBarRepeatDelay
                ? ui.heldBarSeconds - kBarRepeatDelay : dt;
            const float interval = ui.heldBarSeconds >= 1.20f
                ? kBarRepeatFastInterval : kBarRepeatInterval;
            int repeats = 0;
            while (ui.barRepeatSeconds >= interval && repeats++ < 8) {
                ui.barRepeatSeconds -= interval;
                if (!draft.StepSelection(held * 4 * draft.Data().route.ticksPerBeat)) {
                    ui.barRepeatSeconds = 0.0f; // Stay at the chart boundary.
                    break;
                }
                ui.rangeFirst = ui.rangeLast = -1;
            }
        }
    }

    if (ui.tab == EditorTab::Route)
        editor_pattern::Apply(draft, ui, input, commands, action);
    if (ui.tab != EditorTab::Audio) {
        if (input.headingLeft) { draft.RotateHeading45(-1); action.changedChart = true; }
        if (input.headingRight) { draft.RotateHeading45(1); action.changedChart = true; }
    }
    if (input.mouseWheel > 0) draft.StepSnapSelection(-1);
    if (input.mouseWheel < 0) draft.StepSnapSelection(1);
    action.previewToggle |= input.previewToggle;
    action.previewRestart |= input.previewRestart;
    if (input.speedIndex >= 0) action.speedIndex = input.speedIndex;
    return action;
}

} // namespace locus::app
