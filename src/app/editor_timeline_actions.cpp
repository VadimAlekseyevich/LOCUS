#include "app/editor_timeline_actions.h"
#include "app/chart_editor_layout.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace locus::app::editor_timeline {
using namespace detail;

void ChangeSnap(core::ChartEditor& draft, EditorUiState& ui,
                int direction, EditorUiAction& action) {
    const int oldTicks = draft.Data().route.ticksPerBeat;
    const int prior = draft.Data().route.snapDivision;
    int index = 0;
    while (index < 9 && kSnapDivisions[index] != prior) ++index;
    if (index == 9) return;
    const int next = std::clamp(index + direction, 0, 8);
    if (next == index) return;
    if (!draft.SetSnapDivision(kSnapDivisions[next])) {
        ui.feedback = "GRID TOO DENSE FOR SONG LENGTH (MAX 65536 TICKS)";
        ui.feedbackError = true;
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
}

void SeekSongOverview(core::ChartEditor& draft, EditorUiState& ui,
                      float mouseX) {
    if (!draft.HasSongDuration()) return;
    const double songSeconds = SongOverviewSeconds(
        mouseX, draft.SongDurationSeconds());
    const int tick = SongOverviewTick(draft, songSeconds);
    ui.rangeFirst = ui.rangeLast = -1;
    ui.draggingTimeline = false;
    draft.SelectBeat(tick);
}

void BeginSelection(core::ChartEditor& draft, EditorUiState& ui,
                    const EditorControls& input) {
    const Vector2 mouse = input.mouse;
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

void UpdateNavigation(core::ChartEditor& draft, EditorUiState& ui,
                      const EditorControls& input) {
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
}

void ApplyWheel(core::ChartEditor& draft, float wheel) {
    if (wheel > 0) draft.StepSnapSelection(-1);
    if (wheel < 0) draft.StepSnapSelection(1);
}
} // namespace locus::app::editor_timeline
