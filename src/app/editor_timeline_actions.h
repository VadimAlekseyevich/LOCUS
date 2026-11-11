#pragma once
#include "app/chart_editor_ui.h"

namespace locus::app::editor_timeline {
// Timeline/selection operates on integer ticks; preview/zoom is view-only.
// No OS input polling and no judgement/session ownership.
void ChangeSnap(core::ChartEditor& draft, EditorUiState& ui,
                int direction, EditorUiAction& action);
void BeginSelection(core::ChartEditor& draft, EditorUiState& ui,
                    const EditorControls& input);
// Fixed full-song timeline: navigate by audio duration, then focus the closest
// snap tick. It does not write notes, BPM, or Song Set metadata.
void SeekSongOverview(core::ChartEditor& draft, EditorUiState& ui,
                      float mouseX);
void UpdateNavigation(core::ChartEditor& draft, EditorUiState& ui,
                      const EditorControls& input);
void ApplyWheel(core::ChartEditor& draft, float wheel);
} // namespace locus::app::editor_timeline
