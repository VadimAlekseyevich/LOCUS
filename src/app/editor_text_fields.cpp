#include "app/editor_text_fields.h"
#include "app/chart_editor_layout.h"
#include "core/utf8.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace locus::app::editor_text {
using namespace detail;

static const char* FieldName(EditorField field) {
    switch (field) {
        case EditorField::Title: return "TITLE";
        case EditorField::Bpm: return "BPM";
        case EditorField::OffsetMs: return "OFFSET (MS)";
        case EditorField::GridSize: return "SQUARE SIZE";
        case EditorField::None: break;
    }
    return "";
}

static Rectangle FieldBounds(EditorField field) {
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

static std::string InitialValue(const core::ChartEditor& draft, EditorField field) {
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

static bool ParseNumeric(const std::string& string, double& result) {
    if (string.empty()) return false;
    char* end = nullptr;
    result = std::strtod(string.c_str(), &end);
    return end && end != string.c_str() && *end == '\0' && std::isfinite(result);
}

// One integer controls the square's side: never accept a width/height pair.
static bool ParseSquareSize(const std::string& value, int& size) {
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

// Field editing consumes all shortcuts, including pending chords.
EditorUiAction HandleTextInput(core::ChartEditor& draft, EditorUiState& ui,
                               const EditorControls& input) {
    EditorUiAction action;
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
                core::utf8::PopBack(ui.buffer);
            }
        }
        if (!input.typedUtf8.empty()) {
            if (ui.active == EditorField::Title) {
                std::string next = ui.replaceOnType ? "" : ui.buffer;
                if (core::utf8::AppendLimited(next, input.typedUtf8, 100, 1024)) {
                    ui.buffer = std::move(next);
                    ui.replaceOnType = false;
                }
            } else {
                if (ui.replaceOnType) {
                    ui.buffer.clear();
                    ui.replaceOnType = false;
                }
                for (unsigned char c : input.typedUtf8) {
                    const bool wholeGrid = ui.active == EditorField::GridSize;
                    if ((!wholeGrid && (c == '.' || c == ',') &&
                         ui.active != EditorField::OffsetMs) ||
                        (!wholeGrid && c == '-') ||
                        (c >= '0' && c <= '9')) {
                        if (ui.buffer.size() < 100)
                            ui.buffer.push_back(static_cast<char>(c == ',' ? '.' : c));
                    }
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
    return action;
}

} // namespace locus::app::editor_text
