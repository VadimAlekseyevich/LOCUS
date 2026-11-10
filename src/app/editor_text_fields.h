#pragma once

#include "app/chart_editor_ui.h"

namespace locus::app::editor_text {
EditorField ClickedField(Vector2 mouse, EditorTab tab);
void BeginField(EditorUiState& ui, const core::ChartEditor& draft, EditorField field);
// Process a focused authoring field without touching the device input APIs.
EditorUiAction HandleTextInput(core::ChartEditor& draft, EditorUiState& ui,
                               const EditorControls& input);
} // namespace locus::app::editor_text
