#include "app/editor_pattern_actions.h"

#include <algorithm>
#include <optional>

namespace locus::app::editor_pattern {
void Apply(core::ChartEditor& draft, EditorUiState& ui,
           const EditorControls& input, const Commands& commands,
           EditorUiAction& action) {
    const auto notice = [&](const char* message, bool error = false) {
        ui.feedback = message;
        ui.feedbackError = error;
    };
    if (input.movePrevious && !ui.HasRange()) action.changedChart |= draft.MoveTurn(-draft.Data().route.ticksPerBeat /
                                                       draft.Data().route.snapDivision);
    if (input.moveNext && !ui.HasRange()) action.changedChart |= draft.MoveTurn(draft.Data().route.ticksPerBeat /
                                                      draft.Data().route.snapDivision);
    const int first = ui.HasRange() ? ui.rangeFirst : draft.SelectedBeat();
    const int last = ui.HasRange() ? ui.rangeLast : draft.SelectedBeat();
    const bool valid = first >= draft.Data().route.firstBeat &&
                       last < draft.Data().route.lengthBeats;

    if (commands.copy || commands.cut) {
        ui.clipboard = valid ? draft.CopyTurns(first, last)
                             : core::ChartEditor::Pattern{};
        const bool any = !ui.clipboard.notes.empty();
        if (any) {
            // Reflect actual copied note bounds instead of empty ticks
            // that the mouse happened to include before/after the notes.
            const auto& turns = draft.Data().route.turns;
            const auto lower = std::lower_bound(turns.begin(), turns.end(), first,
                [](const core::TurnEvent& event, int beat) {
                    return event.beat < beat;
                });
            const auto upper = std::upper_bound(lower, turns.end(), last,
                [](int beat, const core::TurnEvent& event) {
                    return beat < event.beat;
                });
            ui.rangeFirst = lower->beat;
            ui.rangeLast = (upper - 1)->beat;
        }
        if (commands.cut && any) action.changedChart |= draft.DeleteTurns(first, last);
        notice(any ? (commands.cut ? "TURNS CUT - CTRL+V TO PASTE"
                          : "TURNS COPIED - NO LEADING/TRAILING GAPS")
                   : "NO TURNS IN SELECTION", !any);
    }
    if (commands.paste) {
        const int destination = draft.SelectedBeat();
        const bool ok = draft.PasteTurns(destination, ui.clipboard);
        action.changedChart |= ok;
        if (ok) {
            // Keep the entire pasted pattern selected, including rests
            // between notes, for immediate shift / flip / copy / delete.
            // PasteTurns already checks that the whole span fits.
            ui.rangeFirst = destination;
            ui.rangeLast = destination +
                (ui.clipboard.span - 1) *
                (draft.Data().route.ticksPerBeat / ui.clipboard.ticksPerBeat);
            ui.draggingTimeline = false;
        }
        notice(ok ? "PATTERN PASTED - RANGE STILL SELECTED"
                  : "CANNOT PASTE - EMPTY CLIPBOARD, SAME NOTES OR OUT OF BOUNDS", !ok);
    }
    if (commands.mirrorH || commands.mirrorV) {
        const bool ok = valid && (commands.mirrorH
            ? draft.MirrorTurnsHorizontal(first, last)
            : draft.MirrorTurnsVertical(first, last));
        action.changedChart |= ok;
        notice(ok ? (commands.mirrorH ? "HORIZONTAL MIRROR APPLIED"
                               : "VERTICAL MIRROR APPLIED")
                  : "NO DIRECTIONS CHANGED", !ok);
    }
    if (commands.rotateCW || commands.rotateCCW) {
        const bool ok = valid &&
            draft.RotateTurns(first, last, commands.rotateCW ? 1 : -1);
        action.changedChart |= ok;
        notice(ok ? (commands.rotateCW ? "PATTERN ROTATED 90 CLOCKWISE"
                               : "PATTERN ROTATED 90 COUNTERCLOCKWISE")
                  : "NO TURNS TO ROTATE", !ok);
    }
    if (commands.deleteTurns) {
        const bool ok = valid && draft.DeleteTurns(first, last);
        action.changedChart |= ok;
        notice(ok ? "SELECTED TURNS DELETED" : "NO TURNS TO DELETE", !ok);
    }
    if (input.shiftRangePrevious || input.shiftRangeNext) {
        const int delta = (input.shiftRangePrevious ? -1 : 1) *
                          (draft.Data().route.ticksPerBeat /
                           draft.Data().route.snapDivision);
        const bool ok = ui.HasRange() && valid &&
                        draft.ShiftTurns(first, last, delta);
        action.changedChart |= ok;
        if (ok) {
            ui.rangeFirst += delta;
            ui.rangeLast += delta;
            draft.StepSelection(delta);
        }
        notice(ok ? "PATTERN SHIFTED ONE BEAT"
                  : "SHIFT BLOCKED - SELECT TURNS OR AVOID COLLISIONS", !ok);
    }

}
} // namespace locus::app::editor_pattern
