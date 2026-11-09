#include "app/chart_editor_layout.h"
#include "app/viewport.h"

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

const char* FieldName(EditorField field) {
    switch (field) {
        case EditorField::Title: return "TITLE";
        case EditorField::Bpm: return "BPM";
        case EditorField::OffsetMs: return "OFFSET (MS)";
        case EditorField::GridSize: return "SQUARE SIZE";
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
        if (Hit(kGridSize, mouse)) return EditorField::GridSize;
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
            std::snprintf(text, sizeof(text), "%.0f", draft.Data().offsetSeconds * 1000.0);
            return text;
        case EditorField::GridSize:
            return std::to_string(draft.Data().route.gridWidth);
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

// One integer controls the square's side: never accept a width/height pair.
bool ParseSquareSize(const std::string& value, int& size) {
    if (value.empty() || value.size() > 2 ||
        !std::all_of(value.begin(), value.end(),
                     [](unsigned char c) { return c >= '0' && c <= '9'; }))
        return false;
    size = std::atoi(value.c_str());
    return size >= core::kMinGridSize && size <= core::kMaxGridSize;
}

bool CommitField(EditorUiState& ui, core::ChartEditor& draft) {
    if (!ui.EditingText()) return true;
    const auto field = ui.active;
    bool valid = false;
    if (field == EditorField::Title) {
        valid = draft.SetTitle(ui.buffer);
    } else if (field == EditorField::GridSize) {
        int size = 0;
        if (ParseSquareSize(ui.buffer, size))
            valid = draft.SetGridSize(size, size);
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

// Key masks correspond to the visual 3x3 direction pad, not relative
// left/right turn types. The chart editor resolves these into a turn using
// its incoming route heading; no chart or timing semantics change.
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

Vector2 EditorVirtualMouse(Vector2 physical, int screenWidth, int screenHeight) {
    const auto transform = ViewportTransform::Menu(screenWidth, screenHeight);
    return transform.scale > 0.0f ? transform.ToVirtual(physical)
                                  : Vector2{-1.0f, -1.0f};
}

EditorControls PollEditorControls(const GameSettings& settings,
                                  const std::array<bool, 4>& gameplayHeld) {
    EditorControls input;
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    input.shiftHeld = shift;
    input.copyRange = control && IsKeyPressed(KEY_C);
    input.cutRange = control && IsKeyPressed(KEY_X);
    input.pasteRange = control && IsKeyPressed(KEY_V);
    input.undo = control && !shift && IsKeyPressed(KEY_Z);
    input.redo = (control && IsKeyPressed(KEY_Y)) ||
                 (control && shift && IsKeyPressed(KEY_Z));

    // A configured gameplay direction always wins over a bare editor shortcut.
    // Modifier shortcuts can still use Left/Right or PgUp/PgDn deliberately,
    // because modifiers suppress direction placement.
    const auto available = [&](int key) {
        return !IsGameDirectionKey(settings, key);
    };
    const auto plain = [&](int key) {
        return !control && !shift && available(key) && IsKeyPressed(key);
    };
    const auto navigation = [&](int key) {
        return (control || shift || available(key)) && IsKeyPressed(key);
    };
    const auto navigationHeld = [&](int key) {
        return (control || shift || available(key)) && IsKeyDown(key);
    };
    const bool previous = navigation(KEY_LEFT) || navigation(KEY_PAGE_UP);
    const bool next = navigation(KEY_RIGHT) || navigation(KEY_PAGE_DOWN);
    input.previousBeat = !control && !shift && previous;
    input.nextBeat = !control && !shift && next;
    input.previousBar = control && !shift && previous;
    input.nextBar = control && !shift && next;
    input.shiftRangePrevious = control && shift && previous;
    input.shiftRangeNext = control && shift && next;
    input.movePrevious = shift && !control && previous;
    input.moveNext = shift && !control && next;
    input.heldBarDirection = control && !shift
        ? (static_cast<int>(navigationHeld(KEY_RIGHT) || navigationHeld(KEY_PAGE_DOWN)) -
           static_cast<int>(navigationHeld(KEY_LEFT) || navigationHeld(KEY_PAGE_UP)))
        : 0;

    // The exact four game-direction key states are reused, not polled from a
    // hard-coded DFJK set. Modifier-based chart actions never place notes.
    input.directionKeysSuppressed = control || shift;
    input.directionKeysHeld = input.directionKeysSuppressed
        ? 0u : EditorDirectionMask(gameplayHeld);
    input.directionChordWindowSeconds = settings.chordWindowMs / 1000.0f;
    input.elapsedSeconds = GetFrameTime();

    input.invertRange = plain(KEY_I);
    input.zoomIn = (available(KEY_EQUAL) && IsKeyPressed(KEY_EQUAL)) ||
                   (available(KEY_KP_ADD) && IsKeyPressed(KEY_KP_ADD)) ||
                   (available(KEY_RIGHT_BRACKET) && IsKeyPressed(KEY_RIGHT_BRACKET));
    input.zoomOut = (available(KEY_MINUS) && IsKeyPressed(KEY_MINUS)) ||
                    (available(KEY_KP_SUBTRACT) && IsKeyPressed(KEY_KP_SUBTRACT)) ||
                    (available(KEY_LEFT_BRACKET) && IsKeyPressed(KEY_LEFT_BRACKET));
    // With the new DFJK default, vertical arrows adjust placement
    // density while horizontal arrows move between timeline grid lines.
    // Rebinding an arrow to gameplay suppresses this editor shortcut.
    input.snapMore = plain(KEY_PERIOD) || plain(KEY_UP);
    input.snapLess = plain(KEY_COMMA) || plain(KEY_DOWN);
    input.removeTurn = IsKeyPressed(KEY_BACKSPACE) ||
                       (available(KEY_DELETE) && IsKeyPressed(KEY_DELETE));
    input.headingLeft = plain(KEY_Q);
    input.headingRight = plain(KEY_E);
    input.previewToggle = plain(KEY_SPACE);
    input.previewRestart = plain(KEY_R);
    input.enter = IsKeyPressed(KEY_ENTER);
    input.escape = IsKeyPressed(KEY_ESCAPE);
    input.backspace = IsKeyPressed(KEY_BACKSPACE);
    input.tab = IsKeyPressed(KEY_TAB);
    input.speedIndex = plain(KEY_ONE) ? 0
                     : plain(KEY_TWO) ? 1
                     : plain(KEY_THREE) ? 2
                     : plain(KEY_FOUR) ? 3
                     : plain(KEY_FIVE) ? 4 : -1;
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
        if (clipboard) input.typedUtf8 = clipboard;
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

    // Song Set preview time belongs to the Set controller, not the chart.
    // Suppress route/transport shortcuts; the Set controller consumes text.
    if (ui.editingSetPreview) return action;

    // Keep all editor shortcuts away from active text fields.
    if (ui.EditingText()) {
        ui.pendingDirection = 0;
        ui.pendingDirectionSeconds = 0.0f;
        ui.directionKeysHandled = false;
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
                const bool wholeGrid = ui.active == EditorField::GridSize;
                if (ui.active == EditorField::Title ||
                    (!wholeGrid && (c == '.' || c == ',') &&
                     ui.active != EditorField::OffsetMs) ||
                    (!wholeGrid && c == '-') ||
                    (c >= '0' && c <= '9')) {
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
            ui.pendingDirection = 0;
            ui.directionKeysHandled = false;
            return action;
        }
        const EditorField selected = ClickedField(mouse, ui.tab);
        if (selected != EditorField::None) {
            BeginField(ui, draft, selected);
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
                            "NO CHANGE - ALREADY HEADING THAT WAY", true);
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

    if (ui.tab == EditorTab::Route) {
        ApplyDirectionChord(draft, ui, input, action);
        if (input.movePrevious && !ui.HasRange()) action.changedChart |= draft.MoveTurn(-draft.Data().route.ticksPerBeat /
                                                           draft.Data().route.snapDivision);
        if (input.moveNext && !ui.HasRange()) action.changedChart |= draft.MoveTurn(draft.Data().route.ticksPerBeat /
                                                          draft.Data().route.snapDivision);
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
            if (cut && any) action.changedChart |= draft.DeleteTurns(first, last);
            notice(any ? (cut ? "TURNS CUT - CTRL+V TO PASTE"
                              : "TURNS COPIED - NO LEADING/TRAILING GAPS")
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
                ui.rangeLast = destination +
                    (ui.clipboard.span - 1) *
                    (draft.Data().route.ticksPerBeat / ui.clipboard.ticksPerBeat);
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
