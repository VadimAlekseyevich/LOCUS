#include "app/chart_editor_ui.h"

#include <cmath>
#include <iostream>

using namespace locus::app;
using namespace locus::core;

namespace {
bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "EDITOR_UI_FAIL " << label << '\n';
    return condition;
}
EditorControls Click(float x, float y) {
    EditorControls result;
    result.mouseClick = true;
    result.mouse = {x, y};
    return result;
}
EditorControls Type(const char* text) {
    EditorControls result;
    result.typedUtf8 = text;
    return result;
}
}

int main() {
    bool ok = true;
    ChartEditor chart;
    EditorUiState ui;

    ApplyEditorControls(chart, ui, Click(455, 190));
    ok &= Check(ui.EditingText() && ui.active == EditorField::Bpm,
                "BPM field accepts mouse focus");
    ApplyEditorControls(chart, ui, Type("137,5"));
    EditorControls enter;
    enter.enter = true;
    const auto committed = ApplyEditorControls(chart, ui, enter);
    ok &= Check(committed.changedChart && !ui.EditingText() &&
                    std::abs(chart.Data().route.bpm - 137.5) < 1e-9,
                "typing replaces old BPM, accepts decimal comma");

    ApplyEditorControls(chart, ui, Click(677, 194));
    ApplyEditorControls(chart, ui, Type("-175.25"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(std::abs(chart.Data().offsetSeconds + 0.17525) < 1e-10,
                "negative millisecond offset entered directly");

    ApplyEditorControls(chart, ui, Click(691, 260));
    ApplyEditorControls(chart, ui, Type("48"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().route.lengthBeats == 48,
                "whole timeline length is directly editable");

    ApplyEditorControls(chart, ui, Click(150, 130));
    ApplyEditorControls(chart, ui, Type("My rhythm chart"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().title == "My rhythm chart",
                "chart title can be typed");

    const auto song = ApplyEditorControls(chart, ui, Click(477, 99));
    const auto play = ApplyEditorControls(chart, ui, Click(618, 99));
    const auto restart = ApplyEditorControls(chart, ui, Click(746, 99));
    const auto test = ApplyEditorControls(chart, ui, Click(850, 99));
    ok &= Check(song.chooseAudio && play.previewToggle &&
                    restart.previewRestart && test.testPlay,
                "large visible song, preview, restart and test buttons");

    const auto slow = ApplyEditorControls(chart, ui, Click(840, 329));
    ok &= Check(slow.speedIndex == 4, "10 percent music speed is clickable");
    const auto normal = ApplyEditorControls(chart, ui, Click(435, 329));
    ok &= Check(normal.speedIndex == 0, "100 percent speed is clickable");

    ApplyEditorControls(chart, ui, Click(50 + 53.75f * 9 + 8, 469));
    ok &= Check(chart.SelectedBeat() == 9, "timeline selects a beat for manual preview");
    ApplyEditorControls(chart, ui, Click(96 + 2*59, 197 + 3*59));
    ok &= Check(chart.Data().route.start.x == 2 && chart.Data().route.start.y == 3,
                "clicking the grid moves the starting point");

    const Vector2 mapped = EditorVirtualMouse({1920, 1080}, 3840, 2160);
    ok &= Check(std::abs(mapped.x - 480) < 1e-4 &&
                    std::abs(mapped.y - 270) < 1e-4,
                "editor mouse coordinates survive fullscreen scaling");

    if (!ok) return 1;
    std::cout << "EDITOR_UI_PASS text=1 speed=5 preview=1 grid=1\n";
    return 0;
}
