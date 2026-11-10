#include "app/editor_pattern_actions.h"

#include <algorithm>
#include <optional>

namespace locus::app::editor_pattern {
namespace {
// Keys and the 3x3 pad directly author absolute outgoing compass headings.
std::optional<core::Heading> DirectionForKeys(unsigned mask) {
    switch (mask) {
        case 1u: return core::Heading::Left;       // D
        case 2u: return core::Heading::Down;       // F
        case 4u: return core::Heading::Up;         // J
        case 8u: return core::Heading::Right;      // K
        case 3u: return core::Heading::DownLeft;   // D+F
        case 5u: return core::Heading::UpLeft;     // D+J
        case 10u: return core::Heading::DownRight; // F+K
        case 12u: return core::Heading::UpRight;   // J+K
        default: return std::nullopt;              // opposite or 3+ keys
    }
}

void ApplyDirectionChord(core::ChartEditor& draft, EditorUiState& ui,
                         const EditorControls& input, EditorUiAction& action) {
    const unsigned held = input.directionKeysHeld & 15u;
    if (input.directionKeysSuppressed) {
        ui.pendingDirection = 0;
        ui.pendingDirectionSeconds = 0.0f;
        ui.directionKeysHandled = true; // re-arm only after release
        return;
    }
    // Exactly one insertion for a held key / chord, even if a key is released
    // earlier than the other. Re-arm only after *all* keys are released.
    if (ui.directionKeysHandled) {
        if (held == 0) {
            ui.directionKeysHandled = false;
            ui.pendingDirection = 0;
            ui.pendingDirectionSeconds = 0.0f;
        }
        return;
    }
    const auto commit = [&](unsigned keys) {
        const auto heading = DirectionForKeys(keys);
        if (!heading) {
            ui.feedback = "USE ONE GAME DIRECTION OR TWO ADJACENT KEYS";
            ui.feedbackError = true;
            return;
        }
        const bool changed = draft.SetOutgoingHeading(*heading);
        action.changedChart |= changed;
        if (!changed) {
            ui.feedback = "NO CHANGE - ALREADY HEADING THAT WAY";
            ui.feedbackError = true;
        } else {
            ui.feedback.clear();
            ui.feedbackError = false;
        }
    };

    if (ui.pendingDirection == 0) {
        if (held == 0) return;
        if ((held & (held - 1u)) != 0u) {
            commit(held); // Multiple keys pressed in the same frame.
            ui.directionKeysHandled = true;
            return;
        }
        ui.pendingDirection = held;
        ui.pendingDirectionSeconds = 0.0f;
        return;
    }

    if (held == 0) {
        commit(ui.pendingDirection); // Quick tap: resolve on release.
        ui.pendingDirection = 0;
        ui.pendingDirectionSeconds = 0.0f;
        return;
    }
    if (held != ui.pendingDirection) {
        if ((held & (held - 1u)) != 0u) {
            commit(held); // Chord resolves before its first single note.
            ui.pendingDirection = 0;
            ui.pendingDirectionSeconds = 0.0f;
            ui.directionKeysHandled = true;
            return;
        }
        // Non-overlapping keys changed in one frame. Do not lose the tap.
        commit(ui.pendingDirection);
        ui.pendingDirection = held;
        ui.pendingDirectionSeconds = 0.0f;
        return;
    }
    ui.pendingDirectionSeconds += std::clamp(input.elapsedSeconds, 0.0f, 0.1f);
    if (ui.pendingDirectionSeconds >=
        std::clamp(input.directionChordWindowSeconds, 0.04f, 0.16f)) {
        commit(held);
        ui.pendingDirection = 0;
        ui.pendingDirectionSeconds = 0.0f;
        ui.directionKeysHandled = true;
    }
}

} // namespace

void Apply(core::ChartEditor& draft, EditorUiState& ui,
           const EditorControls& input, const Commands& commands,
           EditorUiAction& action) {
    const auto notice = [&](const char* message, bool error = false) {
        ui.feedback = message;
        ui.feedbackError = error;
    };
    ApplyDirectionChord(draft, ui, input, action);
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
