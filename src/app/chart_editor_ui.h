#pragma once

#include "core/chart_editor.h"
#include "raylib.h"

#include <string>

namespace locus::app {

struct EditorControls {
    bool previousBeat = false, nextBeat = false;
    bool previousBar = false, nextBar = false; // Four beats = one 4/4 bar.
    bool movePrevious = false, moveNext = false;
    bool addLeft = false, addRight = false, removeTurn = false;
    bool headingLeft = false, headingRight = false;
    bool mouseClick = false;
    Vector2 mouse{};
    float mouseWheel = 0.0f;

    bool enter = false, escape = false, backspace = false, tab = false;
    bool previewToggle = false, previewRestart = false;
    int speedIndex = -1; // 0..4: 100, 75, 50, 25, 10 percent.
    std::string typedUtf8;
};

enum class EditorField { None, Title, Bpm, OffsetMs, Length };

struct EditorUiState {
    EditorField active = EditorField::None;
    std::string buffer;
    bool replaceOnType = false;
    std::string feedback;
    bool EditingText() const noexcept { return active != EditorField::None; }
};

struct EditorUiAction {
    bool chooseAudio = false;
    bool previewToggle = false;
    bool previewRestart = false;
    bool testPlay = false;
    bool changedChart = false;
    int speedIndex = -1;
};

Vector2 EditorVirtualMouse(Vector2 physicalMouse, int screenWidth, int screenHeight);
EditorControls PollEditorControls();
EditorUiAction ApplyEditorControls(core::ChartEditor& draft, EditorUiState& ui,
                                  const EditorControls& controls);
void RenderChartEditor(Font font, const core::ChartEditor& draft,
                       const EditorUiState& ui, const std::string& songName,
                       bool hasAudio, bool previewPlaying, bool previewPaused,
                       double previewBeat, double playbackSpeed);

} // namespace locus::app
