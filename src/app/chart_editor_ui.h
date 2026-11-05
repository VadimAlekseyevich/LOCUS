#pragma once

#include "core/chart_editor.h"
#include "raylib.h"

namespace locus::app {

struct EditorControls {
    bool previousBeat = false;
    bool nextBeat = false;
    bool movePrevious = false;
    bool moveNext = false;
    bool addLeft = false;
    bool addRight = false;
    bool removeTurn = false;
    bool bpmDown = false;
    bool bpmUp = false;
    bool offsetDown = false;
    bool offsetUp = false;
    bool headingLeft = false;
    bool headingRight = false;
    bool lengthDown = false;
    bool lengthUp = false;
    bool mouseClick = false;
    Vector2 mouse{};
    float mouseWheel = 0.0f;
};

// Convert from a stretched/fullscreen raylib window into the original
// 960x540 UI coordinates (the same transform as RenderFrame's Camera2D).
Vector2 EditorVirtualMouse(Vector2 physicalMouse, int screenWidth, int screenHeight);
EditorControls PollEditorControls();
void ApplyEditorControls(core::ChartEditor& draft, const EditorControls& controls);
void RenderChartEditor(Font font, const core::ChartEditor& draft);

} // namespace locus::app
