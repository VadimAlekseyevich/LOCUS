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
        mouseX, draft.SongDurationSeconds(), OverviewBounds(ui));
    const int tick = SongOverviewTick(draft, songSeconds);
    ui.rangeFirst = ui.rangeLast = -1;
    ui.draggingTimeline = false;
    draft.SelectBeat(tick);
    ui.pendingExactPlacementTimeMs.reset();
}

void BeginSelection(core::ChartEditor& draft, EditorUiState& ui,
                    const EditorControls& input) {
    const Vector2 mouse = input.mouse;
    const double anchor = ViewBeat(draft, input.timelineViewBeat);
    const int beat = TimelineTickAtX(draft, ui, anchor, mouse.x);
    if (ui.tab == EditorTab::Route && input.ctrlHeld && !input.shiftHeld) {
        const auto& data = draft.Data();
        const auto milliseconds = static_cast<std::int64_t>(std::llround(
            TimelineSecondsAtX(draft, ui, anchor, mouse.x) * 1000.0));
        const int tick = core::TempoTickAtMs(
            data.route, data.offsetSeconds, milliseconds);
        if (tick >= data.route.firstBeat && tick < data.route.lengthBeats) {
            draft.SelectBeat(tick);
            ui.pendingExactPlacementTimeMs = milliseconds;
            ui.rangeFirst = ui.rangeLast = -1;
            ui.draggingTimeline = false;
            ui.feedback = "EXACT " + std::to_string(milliseconds) +
                          " MS - CHOOSE DIRECTION";
            ui.feedbackError = false;
        }
        return;
    }
    ui.pendingExactPlacementTimeMs.reset();
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
    const auto priorExact = draft.SelectedTurnTimeMs();
    draft.SelectBeat(beat);
    // The hit uses source milliseconds, not grid ticks. Repeated clicking
    // near a visually overlapping note cycles only the notes in that same
    // tick and hit radius. That keeps subpixel note clusters selectable.
    if (ui.tab == EditorTab::Route && !input.shiftHeld) {
        if (const auto noteTime = TimelineNoteTimeAtX(
                draft, ui, anchor, mouse.x)) {
            std::int64_t chosen = *noteTime;
            if (priorExact && *priorExact == *noteTime) {
                const auto& chart = draft.Data().route;
                const auto& events = chart.turns;
                const auto current = std::lower_bound(
                    events.begin(), events.end(), *noteTime,
                    [&](const core::TurnEvent& e, std::int64_t ms) {
                        return core::EventTimeMs(
                            chart, e, draft.Data().offsetSeconds) < ms;
                    });
                if (current != events.end() && current->beat == beat) {
                    auto next = current + 1;
                    if (next == events.end() || next->beat != beat)
                        next = std::lower_bound(
                            events.begin(), events.end(), beat,
                            [](const core::TurnEvent& e, int tick) {
                                return e.beat < tick;
                            });
                    if (next != current && next != events.end() &&
                        next->beat == beat) {
                        const auto ms = core::EventTimeMs(
                            chart, *next, draft.Data().offsetSeconds);
                        if (std::abs(TimelineXAtSeconds(
                                draft, ui, anchor, ms / 1000.0) -
                                mouse.x) <= 7.0f)
                            chosen = ms;
                    }
                }
            }
            draft.SelectTurnAtTimeMs(chosen);
        }
    }
}

void UpdateNavigation(core::ChartEditor& draft, EditorUiState& ui,
                      const EditorControls& input) {
    // Dragging past the visible ruler now autoscrolls the *view* as well as
    // extending the selected interval. Keep the beat-to-pixel transform
    // stable while dragging, except for deliberate edge scrolling.
    if (ui.draggingTimeline && (input.mouseHeld || input.mouseReleased)) {
        if (input.mouseHeld && ui.tab == EditorTab::Route) {
            const auto bounds = TimelineBounds(ui);
            const float left = bounds.x;
            const float right = bounds.x + bounds.width;
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
                // Pan in song seconds, not tick indices. Changing BPM
                // must not make the ruler drag/autoscroll accelerate.
                const auto& loaded = draft.Data();
                const auto& chart = loaded.route;
                const double secondsPerSecond =
                    (8.0 + intensity * 16.0) * 60.0 / chart.bpm;
                const double current = TimelineAnchorSeconds(
                    draft, ui.dragViewBeat);
                const double first = core::TempoSecondsAtTick(
                    chart, loaded.offsetSeconds, chart.firstBeat);
                const double last = core::TempoSecondsAtTick(
                    chart, loaded.offsetSeconds, chart.lengthBeats - 1);
                const double moved = std::clamp(
                    current + (edge > 0.0 ? 1.0 : -1.0) *
                                  secondsPerSecond * dt, first, last);
                ui.dragViewBeat = std::clamp(
                    core::TempoBeatAtSeconds(
                        chart, loaded.offsetSeconds, moved) *
                        chart.ticksPerBeat,
                    static_cast<double>(chart.firstBeat),
                    static_cast<double>(chart.lengthBeats - 1));
            }
        }
        const auto bounds = TimelineBounds(ui);
        const float x = std::clamp(input.mouse.x, bounds.x, bounds.x+bounds.width);
        const int beat = TimelineTickAtX(draft, ui, ui.dragViewBeat, x);
        if (beat != ui.dragAnchorBeat) {
            ui.rangeFirst = std::min(ui.dragAnchorBeat, beat);
            ui.rangeLast = std::max(ui.dragAnchorBeat, beat);
            // Moving to another tick makes this a genuine range drag,
            // which intentionally clears any individually selected note.
            draft.SelectBeat(beat);
        } else {
            const bool hadRange = ui.HasRange();
            ui.rangeFirst = ui.rangeLast = -1;
            // A real click frame has mouseHeld=true, so BeginSelection and
            // UpdateNavigation run in the SAME frame. Re-selecting the same
            // tick here used to erase the exact time we just hit-tested!
            // Only reset the cursor after dragging back from another tick.
            if (hadRange) draft.SelectBeat(beat);
        }
    }
    if (input.mouseReleased || (!input.mouseHeld && !input.mouseClick))
        ui.draggingTimeline = false;

    if (input.zoomOut) ui.timelineZoomIndex = std::max(0, ui.timelineZoomIndex - 1);
    if (input.zoomIn) ui.timelineZoomIndex = std::min(kZoomMax, ui.timelineZoomIndex + 1);
    if (input.previousBeat || input.nextBeat ||
        input.previousBar || input.nextBar ||
        input.heldBarDirection != 0 || input.mouseWheel != 0.0f)
        ui.pendingExactPlacementTimeMs.reset();
    if (input.previousBeat) { draft.StepSnapSelection(-1); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.nextBeat) { draft.StepSnapSelection(1); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.previousBar || input.nextBar) {
        const auto& loaded = draft.Data();
        const int destination = core::TempoAdjacentBarTick(
            loaded.route, loaded.offsetSeconds, draft.SelectedBeat(),
            input.previousBar ? -1 : 1);
        draft.SelectBeat(destination);
        ui.rangeFirst = ui.rangeLast = -1;
    }
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
                const auto& loaded = draft.Data();
                const int target = core::TempoAdjacentBarTick(
                    loaded.route, loaded.offsetSeconds,
                    draft.SelectedBeat(), held);
                if (!draft.SelectBeat(target)) {
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
