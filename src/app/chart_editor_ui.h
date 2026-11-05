#pragma once

#include "core/chart_editor.h"
#include "raylib.h"

#include <limits>
#include <string>

namespace locus::app {

struct EditorControls {
    bool previousBeat = false, nextBeat = false;
    bool previousBar = false, nextBar = false; // Four beats = one 4/4 bar.
    // Held Ctrl+arrow direction for deterministic continuous navigation.
    // UI input polls the key state; tests can supply elapsedSeconds directly.
    int heldBarDirection = 0; // -1 = left, +1 = right, 0 = released.
    float elapsedSeconds = 0.0f;
    bool movePrevious = false, moveNext = false;
    bool addLeft = false, addRight = false, removeTurn = false;
    bool addLeft45 = false, addRight45 = false;
    bool addLeft135 = false, addRight135 = false;
    bool headingLeft = false, headingRight = false;
    bool mouseClick = false, mouseHeld = false, mouseReleased = false;
    bool shiftHeld = false;
    Vector2 mouse{};
    float mouseWheel = 0.0f;
    // NaN means use the selected beat. -1 is a legitimate pre-offset beat.
    double timelineViewBeat = std::numeric_limits<double>::quiet_NaN();
    bool zoomIn = false, zoomOut = false;
    bool copyRange = false, cutRange = false, pasteRange = false;
    bool invertRange = false, undo = false, redo = false;
    bool shiftRangePrevious = false, shiftRangeNext = false;

    bool enter = false, escape = false, backspace = false, tab = false;
    bool previewToggle = false, previewRestart = false;
    int speedIndex = -1; // 0..4: 100, 75, 50, 25, 10 percent.
    std::string typedUtf8;
};

enum class EditorTab { Route, Audio, Field };
enum class EditorField { None, Title, Bpm, OffsetMs, GridSize, StartCell };

struct EditorUiState {
    EditorField active = EditorField::None;
    EditorTab tab = EditorTab::Route;
    std::string buffer;
    bool replaceOnType = false;
    std::string feedback;
    bool feedbackError = false;
    int rangeFirst = -1, rangeLast = -1; // Inclusive selected beats.
    bool draggingTimeline = false;
    int dragAnchorBeat = 0;
    double dragViewBeat = 0.0;
    int heldBarDirection = 0;
    float heldBarSeconds = 0.0f;
    float barRepeatSeconds = 0.0f;
    core::ChartEditor::Pattern clipboard;
    bool HasRange() const noexcept {
        return rangeFirst >= 0 && rangeLast >= rangeFirst;
    }
    int timelineZoomIndex = 3; // Default 36 px/beat; zoom changes view only.
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
