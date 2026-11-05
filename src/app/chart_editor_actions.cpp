#include "app/chart_editor_layout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

namespace locus::app {
using namespace detail;
namespace {

const char* FieldName(EditorField field) {
    switch (field) {
        case EditorField::Title: return "TITLE";
        case EditorField::Bpm: return "BPM";
        case EditorField::OffsetMs: return "OFFSET (MS)";
        case EditorField::GridSize: return "GRID SIZE (W/H)";
        case EditorField::StartCell: return "START (X/Y)";
        case EditorField::None: break;
    }
    return "";
}

Rectangle FieldBounds(EditorField field) {
    switch (field) {
        case EditorField::Title: return kTitle;
        case EditorField::Bpm: return kBpm;
        case EditorField::OffsetMs: return kOffset;
         case EditorField::GridSize: return kGridSize;
        case EditorField::StartCell: return kStartCell;
        case EditorField::None: break;
    }
    return {0, 0, 0, 0};
}

EditorField ClickedField(Vector2 mouse, EditorTab tab) {
    if (tab == EditorTab::Audio) {
        for (const auto field : {EditorField::Title, EditorField::Bpm,
                                  EditorField::OffsetMs}) {
            if (Hit(FieldBounds(field), mouse)) return field;
        }
    } else if (tab == EditorTab::Field) {
        for (const auto field : {EditorField::GridSize, EditorField::StartCell,
                                  }) {
            if (Hit(FieldBounds(field), mouse)) return field;
        }
    }
    return EditorField::None;
}

std::string InitialValue(const core::ChartEditor& draft, EditorField field) {
    char text[100]{};
    switch (field) {
        case EditorField::Title: return draft.Data().title;
        case EditorField::Bpm:
            std::snprintf(text, sizeof(text), "%.3f", draft.Data().route.bpm);
            return text;
        case EditorField::OffsetMs:
            std::snprintf(text, sizeof(text), "%.3f", draft.Data().offsetSeconds * 1000.0);
            return text;
         case EditorField::GridSize:
            return std::to_string(draft.Data().route.gridWidth) + "/" +
                   std::to_string(draft.Data().route.gridHeight);
        case EditorField::StartCell:
            return std::to_string(draft.Data().route.start.x) + "/" +
                   std::to_string(draft.Data().route.start.y);
        default: return "";
    }
}

void BeginField(EditorUiState& ui, const core::ChartEditor& draft, EditorField field) {
    ui.active = field;
    ui.buffer = InitialValue(draft, field);
    ui.replaceOnType = true; // The first character replaces the shown number.
    ui.feedback = "";
}

bool ParseNumeric(const std::string& string, double& result) {
    if (string.empty()) return false;
    char* end = nullptr;
    result = std::strtod(string.c_str(), &end);
    return end && end != string.c_str() && *end == '\0' && std::isfinite(result);
}

// A field size like 8/8 (or 8x8) is two game-field dimensions,
// not a musical time signature. Coordinates are zero-based.
bool ParsePair(const std::string& value, int& first, int& second) {
    if (value.empty()) return false;
    char* middle = nullptr;
    const long a = std::strtol(value.c_str(), &middle, 10);
    if (middle == value.c_str() ||
        (*middle != '/' && *middle != 'x' && *middle != 'X')) return false;
    char* end = nullptr;
    const long b = std::strtol(middle + 1, &end, 10);
    if (end == middle + 1 || *end != '\0' ||
        a < 0 || a > std::numeric_limits<int>::max() ||
        b < 0 || b > std::numeric_limits<int>::max()) return false;
    first = static_cast<int>(a);
    second = static_cast<int>(b);
    return true;
}

bool CommitField(EditorUiState& ui, core::ChartEditor& draft) {
    if (!ui.EditingText()) return true;
    const auto field = ui.active;
    bool valid = false;
    if (field == EditorField::Title) {
        valid = draft.SetTitle(ui.buffer);
    } else if (field == EditorField::GridSize || field == EditorField::StartCell) {
        int x = 0, y = 0;
        if (ParsePair(ui.buffer, x, y)) {
            if (field == EditorField::GridSize) valid = draft.SetGridSize(x, y);
            else valid = draft.SetStart({x, y}) ||
                         (draft.Data().route.start.x == x &&
                          draft.Data().route.start.y == y);
        }
    } else {
        double value = 0.0;
        if (ParseNumeric(ui.buffer, value)) {
            if (field == EditorField::Bpm) valid = draft.SetBpm(value);
            if (field == EditorField::OffsetMs) valid = draft.SetOffsetMs(value);
         }
    }
    if (!valid) {
        ui.feedback = std::string("INVALID ") + FieldName(field) +
                      " - CHECK VALUE AND TRY ENTER";
        return false;
    }
    ui.active = EditorField::None;
    ui.buffer.clear();
    ui.replaceOnType = false;
    ui.feedback.clear();
    return true;
}

} // namespace

Vector2 EditorVirtualMouse(Vector2 physical, int screenWidth, int screenHeight) {
    const float scale = std::min(static_cast<float>(screenWidth) / kVirtualWidth,
                                 static_cast<float>(screenHeight) / kVirtualHeight);
    if (scale <= 0.0f) return {-1, -1};
    const float x = (screenWidth - kVirtualWidth * scale) / 2.0f;
    const float y = (screenHeight - kVirtualHeight * scale) / 2.0f;
    return {(physical.x - x) / scale, (physical.y - y) / scale};
}

EditorControls PollEditorControls() {
    EditorControls input;
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    input.shiftHeld = shift;
    input.copyRange = control && IsKeyPressed(KEY_C);
    input.cutRange = control && IsKeyPressed(KEY_X);
    input.pasteRange = control && IsKeyPressed(KEY_V);
    input.invertRange = !control && IsKeyPressed(KEY_I);
    input.undo = control && !shift && IsKeyPressed(KEY_Z);
    input.redo = (control && IsKeyPressed(KEY_Y)) ||
                 (control && shift && IsKeyPressed(KEY_Z));
    input.shiftRangePrevious = control && shift && IsKeyPressed(KEY_LEFT);
    input.shiftRangeNext = control && shift && IsKeyPressed(KEY_RIGHT);
    input.zoomIn = IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD) ||
                   IsKeyPressed(KEY_RIGHT_BRACKET);
    input.zoomOut = IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT) ||
                    IsKeyPressed(KEY_LEFT_BRACKET);
    input.previousBeat = IsKeyPressed(KEY_LEFT) && !shift && !control;
    input.nextBeat = IsKeyPressed(KEY_RIGHT) && !shift && !control;
    input.previousBar = IsKeyPressed(KEY_LEFT) && control && !shift;
    input.nextBar = IsKeyPressed(KEY_RIGHT) && control && !shift;
    input.heldBarDirection = control && !shift
        ? (static_cast<int>(IsKeyDown(KEY_RIGHT)) -
           static_cast<int>(IsKeyDown(KEY_LEFT))) : 0;
    input.elapsedSeconds = GetFrameTime();
    input.movePrevious = IsKeyPressed(KEY_LEFT) && shift && !control;
    input.moveNext = IsKeyPressed(KEY_RIGHT) && shift && !control;
    input.addLeft = IsKeyPressed(KEY_F);
    input.addRight = IsKeyPressed(KEY_J);
    input.addLeft45 = IsKeyPressed(KEY_G) && !control;
    input.addRight45 = IsKeyPressed(KEY_H) && !control;
    input.addLeft135 = IsKeyPressed(KEY_V) && !control;
    input.addRight135 = IsKeyPressed(KEY_N) && !control;
    input.removeTurn = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE);
    input.headingLeft = IsKeyPressed(KEY_Q);
    input.headingRight = IsKeyPressed(KEY_E);
    input.previewToggle = IsKeyPressed(KEY_SPACE);
    input.previewRestart = IsKeyPressed(KEY_R);
    input.enter = IsKeyPressed(KEY_ENTER);
    input.escape = IsKeyPressed(KEY_ESCAPE);
    input.backspace = IsKeyPressed(KEY_BACKSPACE);
    input.tab = IsKeyPressed(KEY_TAB);
    input.speedIndex = IsKeyPressed(KEY_ONE) ? 0
                     : IsKeyPressed(KEY_TWO) ? 1
                     : IsKeyPressed(KEY_THREE) ? 2
                     : IsKeyPressed(KEY_FOUR) ? 3
                     : IsKeyPressed(KEY_FIVE) ? 4 : -1;
    input.mouseClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.mouseHeld = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    input.mouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    input.mouse = EditorVirtualMouse(GetMousePosition(), GetScreenWidth(),
                                     GetScreenHeight());
    input.mouseWheel = GetMouseWheelMove();
    if (control && input.mouseWheel != 0.0f) {
        input.zoomIn |= input.mouseWheel > 0;
        input.zoomOut |= input.mouseWheel < 0;
        input.mouseWheel = 0.0f;
    }
    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        int bytes = 0;
        const char* utf8 = CodepointToUTF8(codepoint, &bytes);
        if (utf8 && bytes > 0) input.typedUtf8.append(utf8, bytes);
        codepoint = GetCharPressed();
    }
    if (control && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (clipboard) input.typedUtf8 = clipboard; // Ctrl+V pastes the full value.
    }
    return input;
}

EditorUiAction ApplyEditorControls(core::ChartEditor& draft, EditorUiState& ui,
                                  const EditorControls& input) {
    EditorUiAction action;
    const auto notice = [&](const char* message, bool error = false) {
        ui.feedback = message;
        ui.feedbackError = error;
    };

    // Keep all editor shortcuts away from active text fields.
    if (ui.EditingText()) {
        ui.heldBarDirection = 0;
        ui.heldBarSeconds = 0.0f;
        ui.barRepeatSeconds = 0.0f;
        if (input.escape) {
            ui.active = EditorField::None;
            ui.buffer.clear();
            ui.replaceOnType = false;
            ui.feedback.clear();
            ui.feedbackError = false;
        } else if (input.backspace) {
            if (ui.replaceOnType) {
                ui.buffer.clear();
                ui.replaceOnType = false;
            } else if (!ui.buffer.empty()) {
                std::size_t pos = ui.buffer.size() - 1;
                while (pos > 0 &&
                       (static_cast<unsigned char>(ui.buffer[pos]) & 0xc0) == 0x80)
                    --pos;
                ui.buffer.erase(pos);
            }
        }
        if (!input.typedUtf8.empty()) {
            if (ui.replaceOnType) {
                ui.buffer.clear();
                ui.replaceOnType = false;
            }
            for (unsigned char c : input.typedUtf8) {
                const bool pairField = ui.active == EditorField::GridSize ||
                                       ui.active == EditorField::StartCell;
                if (ui.active == EditorField::Title ||
                    (pairField && (c == '/' || c == 'x' || c == 'X')) ||
                    c == '.' || c == ',' || c == '-' || (c >= '0' && c <= '9')) {
                    if (ui.buffer.size() < 100)
                        ui.buffer.push_back(static_cast<char>(
                            c == ',' && ui.active != EditorField::Title ? '.' : c));
                }
            }
        }
        if (input.enter || input.tab) {
            const bool committed = CommitField(ui, draft);
            action.changedChart = committed;
            if (committed && input.tab) {
                const EditorField next = ui.tab == EditorTab::Audio
                    ? EditorField::Bpm : EditorField::GridSize;
                BeginField(ui, draft, next);
            }
        } else if (input.mouseClick) {
            const EditorField target = ClickedField(input.mouse, ui.tab);
            const EditorTab nextTab = ClickedTab(input.mouse, ui.tab);
            if (target != ui.active || nextTab != ui.tab) {
                const bool committed = CommitField(ui, draft);
                action.changedChart = committed;
                if (committed) {
                    ui.tab = nextTab;
                    if (target != EditorField::None && nextTab == ui.tab)
                        BeginField(ui, draft, target);
                }
            }
        }
        return action;
    }

    // Undo/redo also cover non-pattern edits (BPM, grid size, individual turns).
    if (input.undo || input.redo) {
        const bool ok = input.undo ? draft.Undo() : draft.Redo();
        action.changedChart = ok;
        notice(ok ? (input.undo ? "UNDO APPLIED" : "REDO APPLIED")
                  : (input.undo ? "NOTHING TO UNDO" : "NOTHING TO REDO"), !ok);
        if (ok && ui.HasRange()) {
            if (ui.rangeLast >= draft.Data().route.lengthBeats)
                ui.rangeFirst = ui.rangeLast = -1;
        }
        return action;
    }

    bool copy = input.copyRange && ui.tab == EditorTab::Route;
    bool cut = input.cutRange && ui.tab == EditorTab::Route;
    bool paste = input.pasteRange && ui.tab == EditorTab::Route;
    bool invert = input.invertRange && ui.tab == EditorTab::Route;
    bool deleteTurns = input.removeTurn && ui.tab == EditorTab::Route;

    if (input.mouseClick) {
        ui.draggingTimeline = false;
        const Vector2 mouse = input.mouse;
        const EditorTab nextTab = ClickedTab(mouse, ui.tab);
        if (nextTab != ui.tab) {
            ui.tab = nextTab;
            return action;
        }
        const EditorField selected = ClickedField(mouse, ui.tab);
        if (selected != EditorField::None) {
            BeginField(ui, draft, selected);
            return action;
        }

        if (Hit(kSong, mouse)) action.chooseAudio = true;
        else if (Hit(kPlay, mouse)) action.previewToggle = true;
        else if (Hit(kStop, mouse)) action.previewRestart = true;
        else if (Hit(kTest, mouse)) action.testPlay = true;
        else if (Hit(kZoomOut, mouse))
            ui.timelineZoomIndex = std::max(0, ui.timelineZoomIndex - 1);
        else if (Hit(kZoomIn, mouse))
            ui.timelineZoomIndex = std::min(kZoomMax, ui.timelineZoomIndex + 1);
        else if (ui.tab == EditorTab::Route && Hit(kCopyTurns, mouse)) copy = true;
        else if (ui.tab == EditorTab::Route && Hit(kPasteTurns, mouse)) paste = true;
        else if (ui.tab == EditorTab::Route && Hit(kInvertTurns, mouse)) invert = true;
        else if (ui.tab == EditorTab::Route && Hit(kCutTurns, mouse)) cut = true;
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
                            "NO CHANGE - STRAIGHT OR OPPOSITE DIRECTION", true);
                    } else {
                        deleteTurns = true;
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
            const double clicked = anchor + (mouse.x - kPlayheadX) / PixelsPerBeat(ui);
            const int beat = std::clamp(static_cast<int>(std::lround(clicked)),
                                        draft.Data().route.firstBeat,
                                        draft.Data().route.lengthBeats - 1);
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
        } else if (ui.tab == EditorTab::Route) {
            const auto& chart = draft.Data().route;
            const EditorGrid grid(chart);
            const float tolerance = std::clamp(grid.step * 0.38f, 7.0f, 19.0f);
            for (int y = 0; y < chart.gridHeight; ++y)
                for (int x = 0; x < chart.gridWidth; ++x) {
                    const auto center = grid.Point(core::Cell{x, y});
                    if (std::abs(mouse.x - center.x) <= tolerance &&
                        std::abs(mouse.y - center.y) <= tolerance) {
                        if (draft.SetStart({x, y})) {
                            draft.SelectBeat(draft.Data().route.firstBeat);
                            action.changedChart = true;
                        }
                        return action;
                    }
                }
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
                        (edge > 0.0f ? 1.0 : -1.0) * beatsPerSecond * dt,
                    static_cast<double>(draft.Data().route.firstBeat),
                    static_cast<double>(draft.Data().route.lengthBeats - 1));
            }
        }
        const float x = std::clamp(input.mouse.x, kTimelineX,
                                    kTimelineX + kTimelineWidth);
        const double point = ui.dragViewBeat +
                             (x - kPlayheadX) / PixelsPerBeat(ui);
        const int beat = std::clamp(static_cast<int>(std::lround(point)),
                                    draft.Data().route.firstBeat,
                                    draft.Data().route.lengthBeats - 1);
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
    if (input.previousBeat) { draft.StepSelection(-1); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.nextBeat) { draft.StepSelection(1); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.previousBar) { draft.StepSelection(-4); ui.rangeFirst = ui.rangeLast = -1; }
    if (input.nextBar) { draft.StepSelection(4); ui.rangeFirst = ui.rangeLast = -1; }
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
                if (!draft.StepSelection(held * 4)) {
                    ui.barRepeatSeconds = 0.0f; // Stay at the chart boundary.
                    break;
                }
                ui.rangeFirst = ui.rangeLast = -1;
            }
        }
    }

    if (ui.tab == EditorTab::Route) {
        if (input.movePrevious && !ui.HasRange()) action.changedChart |= draft.MoveTurn(-1);
        if (input.moveNext && !ui.HasRange()) action.changedChart |= draft.MoveTurn(1);
        if (input.addLeft) { draft.PutTurn(core::Turn::Left); action.changedChart = true; }
        if (input.addRight) { draft.PutTurn(core::Turn::Right); action.changedChart = true; }
        if (input.addLeft45) { draft.PutTurn(core::Turn::Left45); action.changedChart = true; }
        if (input.addRight45) { draft.PutTurn(core::Turn::Right45); action.changedChart = true; }
        if (input.addLeft135) { draft.PutTurn(core::Turn::Left135); action.changedChart = true; }
        if (input.addRight135) { draft.PutTurn(core::Turn::Right135); action.changedChart = true; }
        const int first = ui.HasRange() ? ui.rangeFirst : draft.SelectedBeat();
        const int last = ui.HasRange() ? ui.rangeLast : draft.SelectedBeat();
        const bool valid = first >= draft.Data().route.firstBeat &&
                           last < draft.Data().route.lengthBeats;

        if (copy || cut) {
            ui.clipboard = valid ? draft.CopyTurns(first, last)
                                 : core::ChartEditor::Pattern{};
            const bool any = !ui.clipboard.notes.empty();
            if (cut && any) action.changedChart |= draft.DeleteTurns(first, last);
            notice(any ? (cut ? "TURNS CUT - CTRL+V TO PASTE"
                              : "TURNS COPIED - SELECT DESTINATION BEAT")
                       : "NO TURNS IN SELECTION", !any);
        }
        if (paste) {
            const int destination = draft.SelectedBeat();
            const bool ok = draft.PasteTurns(destination, ui.clipboard);
            action.changedChart |= ok;
            if (ok) {
                // Keep the entire pasted pattern selected, including rests
                // between notes, for immediate shift / flip / copy / delete.
                // PasteTurns already checks that the whole span fits.
                ui.rangeFirst = destination;
                ui.rangeLast = destination + ui.clipboard.span - 1;
                ui.draggingTimeline = false;
            }
            notice(ok ? "PATTERN PASTED - RANGE STILL SELECTED"
                      : "CANNOT PASTE - EMPTY CLIPBOARD, SAME NOTES OR OUT OF BOUNDS", !ok);
        }
        if (invert) {
            const bool ok = valid && draft.InvertTurns(first, last);
            action.changedChart |= ok;
            notice(ok ? "LEFT / RIGHT REVERSED" : "NO TURNS TO INVERT", !ok);
        }
        if (deleteTurns) {
            const bool ok = valid && draft.DeleteTurns(first, last);
            action.changedChart |= ok;
            notice(ok ? "SELECTED TURNS DELETED" : "NO TURNS TO DELETE", !ok);
        }
        if (input.shiftRangePrevious || input.shiftRangeNext) {
            const int delta = input.shiftRangePrevious ? -1 : 1;
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
    if (ui.tab != EditorTab::Audio) {
        if (input.headingLeft) { draft.RotateHeading45(-1); action.changedChart = true; }
        if (input.headingRight) { draft.RotateHeading45(1); action.changedChart = true; }
    }
    if (input.mouseWheel > 0) draft.StepSelection(-1);
    if (input.mouseWheel < 0) draft.StepSelection(1);
    action.previewToggle |= input.previewToggle;
    action.previewRestart |= input.previewRestart;
    if (input.speedIndex >= 0) action.speedIndex = input.speedIndex;
    return action;
}

} // namespace locus::app
