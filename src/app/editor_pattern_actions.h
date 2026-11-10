#pragma once
#include "app/chart_editor_ui.h"

namespace locus::app::editor_pattern {
// A single routed operation may come from either the keyboard or its button.
// Keep the core pattern semantics separate from hitbox dispatch.
struct Commands {
    bool copy = false, cut = false, paste = false;
    bool mirrorH = false, mirrorV = false;
    bool rotateCW = false, rotateCCW = false;
    bool deleteTurns = false;
};
void Apply(core::ChartEditor& draft, EditorUiState& ui,
           const EditorControls& input, const Commands& commands,
           EditorUiAction& action);
} // namespace locus::app::editor_pattern
