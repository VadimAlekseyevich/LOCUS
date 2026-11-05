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
    ok &= Check(ui.tab == EditorTab::Route &&
                chart.Data().route.gridWidth == 4 &&
                chart.Data().route.gridHeight == 4,
                "editor opens with a 4x4 main Route tab");

    // Audio settings are no longer stacked on top of the route grid.
    ApplyEditorControls(chart, ui, Click(612, 89));
    ok &= Check(ui.tab == EditorTab::Audio, "Audio tab opens");
    ApplyEditorControls(chart, ui, Click(145, 312));
    ok &= Check(ui.EditingText() && ui.active == EditorField::Bpm,
                "Audio BPM field accepts mouse focus");
    ApplyEditorControls(chart, ui, Type("400,5"));
    EditorControls enter;
    enter.enter = true;
    const auto committed = ApplyEditorControls(chart, ui, enter);
    ok &= Check(committed.changedChart && !ui.EditingText() &&
                    std::abs(chart.Data().route.bpm - 400.5) < 1e-9,
                "Audio BPM accepts fast decimal values");

    ApplyEditorControls(chart, ui, Click(600, 310));
    ApplyEditorControls(chart, ui, Type("-175.25"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(std::abs(chart.Data().offsetSeconds + 0.17525) < 1e-10,
                "negative millisecond offset entered directly");

    ApplyEditorControls(chart, ui, Click(145, 226));
    ApplyEditorControls(chart, ui, Type("My rhythm chart"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().title == "My rhythm chart",
                "chart title can be typed in Audio tab");

    const auto audioEmpty = ApplyEditorControls(chart, ui, Click(116, 390));
    ok &= Check(audioEmpty.speedIndex == -1,
                "practice speed buttons are no longer in the Audio tab");

    // Geometry is a separate authoring tab, accepting size WxH as 8/8.
    ApplyEditorControls(chart, ui, Click(783, 89));
    ok &= Check(ui.tab == EditorTab::Field, "Field tab opens");
    ApplyEditorControls(chart, ui, Click(157, 247));
    ApplyEditorControls(chart, ui, Type("8/8"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().route.gridWidth == 8 &&
                chart.Data().route.gridHeight == 8,
                "8/8 sets eight rows and eight columns");

    ApplyEditorControls(chart, ui, Click(598, 247));
    ApplyEditorControls(chart, ui, Type("7/7"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().route.start.x == 7 &&
                chart.Data().route.start.y == 7,
                "Field tab supports start cells beyond 4x4");

    ApplyEditorControls(chart, ui, Click(157, 247));
    ApplyEditorControls(chart, ui, Type("4/4"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(ui.EditingText() &&
                chart.Data().route.gridWidth == 8 &&
                !ui.feedback.empty(),
                "cannot shrink a grid over an authored start cell");
    EditorControls escape;
    escape.escape = true;
    ApplyEditorControls(chart, ui, escape);

    ApplyEditorControls(chart, ui, Click(155, 343));
    ApplyEditorControls(chart, ui, Type("48"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().route.lengthBeats == 48,
                "total timeline length is edited under Field");

    // Transport is shared by all tabs.
    const auto song = ApplyEditorControls(chart, ui, Click(113, 134));
    const auto play = ApplyEditorControls(chart, ui, Click(285, 134));
    const auto restart = ApplyEditorControls(chart, ui, Click(445, 134));
    const auto test = ApplyEditorControls(chart, ui, Click(556, 134));
    ok &= Check(song.chooseAudio && play.previewToggle &&
                    restart.previewRestart && test.testPlay,
                "song, play, restart and test remain on every tab");

    ApplyEditorControls(chart, ui, Click(450, 89));
    ok &= Check(ui.tab == EditorTab::Route, "Route tab restores the large grid");
    const auto normal = ApplyEditorControls(chart, ui, Click(668, 221));
    const auto slow = ApplyEditorControls(chart, ui, Click(668, 370));
    ok &= Check(normal.speedIndex == 0 && slow.speedIndex == 4,
                "practice speed 100 and 10 percent are clickable on Route");

    // At x = 50 + 860/3, the selected beat stays under the fixed playhead.
    constexpr float cursor = 50.0f + 860.0f / 3.0f;
    chart.SelectBeat(15);
    ApplyEditorControls(chart, ui, Click(cursor + 36, 477));
    ok &= Check(chart.SelectedBeat() == 16,
                "timeline crosses 15/16 without any page jump");
    ApplyEditorControls(chart, ui, Click(cursor - 36 * 3, 477));
    ok &= Check(chart.SelectedBeat() == 13,
                "clicking behind the one-third playhead selects a past beat");

    chart.SelectBeat(15);
    EditorControls moving = Click(cursor + 18, 477);
    moving.timelineViewBeat = 15.5;
    ApplyEditorControls(chart, ui, moving);
    ok &= Check(chart.SelectedBeat() == 16,
                "fractional playback beat drives timeline hit-testing");

    EditorControls zoomIn;
    zoomIn.zoomIn = true;
    ApplyEditorControls(chart, ui, zoomIn);
    ok &= Check(ui.timelineZoomIndex == 4,
                "zoom hotkey changes pixels per beat");
    chart.SelectBeat(16);
    ApplyEditorControls(chart, ui, Click(cursor + 52, 477));
    ok &= Check(chart.SelectedBeat() == 17,
                "zoomed ruler maps clicks to nearby beat ticks");
    ApplyEditorControls(chart, ui, Click(820, 430));
    ok &= Check(ui.timelineZoomIndex == 3,
                "zoom-out button restores the original scale");
    EditorControls zoomOut;
    zoomOut.zoomOut = true;
    for (int n = 0; n < 10; ++n) ApplyEditorControls(chart, ui, zoomOut);
    ok &= Check(ui.timelineZoomIndex == 0, "zoom-out clamps");
    for (int n = 0; n < 10; ++n) ApplyEditorControls(chart, ui, zoomIn);
    ok &= Check(ui.timelineZoomIndex == 6, "zoom-in clamps");
    chart.SelectBeat(0);
    ApplyEditorControls(chart, ui, Click(60, 477));
    ok &= Check(chart.SelectedBeat() == 0, "no negative timeline selection");

    // The main grid fits the requested 8x8 geometry. Center at 448,282.
    // 240-pixel span across 7 steps => cell (2,3) at about 397,265.
    chart.SelectBeat(20);
    ApplyEditorControls(chart, ui, Click(397, 265));
    ok &= Check(chart.Data().route.start.x == 2 &&
                chart.Data().route.start.y == 3 &&
                chart.SelectedBeat() == 0,
                "clicking an 8x8 dot changes start and resets selected beat");

    const Vector2 mapped = EditorVirtualMouse({1920, 1080}, 3840, 2160);
    ok &= Check(std::abs(mapped.x - 480) < 1e-4 &&
                std::abs(mapped.y - 270) < 1e-4,
                "editor mouse coordinates survive fullscreen scaling");

    // ROUTE only: select a continuous beat interval with a real mouse drag,
    // copy it, and paste it at an unrelated timeline position.
    chart.NewDraft();
    ui = EditorUiState{};
    chart.SelectBeat(6);
    chart.PutTurn(Turn::Left);
    chart.SelectBeat(8);
    chart.PutTurn(Turn::Right);
    chart.SelectBeat(6);

    EditorControls press = Click(cursor, 477);
    press.mouseHeld = true;
    press.timelineViewBeat = 6.0;
    ApplyEditorControls(chart, ui, press);
    EditorControls drag;
    drag.mouseHeld = true;
    drag.mouse = {cursor + 72.0f, 477};
    drag.timelineViewBeat = 8.0;
    ApplyEditorControls(chart, ui, drag);
    EditorControls release;
    release.mouseReleased = true;
    release.mouse = {cursor + 72.0f, 477};
    ApplyEditorControls(chart, ui, release);
    ok &= Check(ui.HasRange() && ui.rangeFirst == 6 &&
                ui.rangeLast == 8 && chart.SelectedBeat() == 8,
                "mouse drag highlights inclusive beat range 6..8");

    EditorControls copy;
    copy.copyRange = true;
    const auto copied = ApplyEditorControls(chart, ui, copy);
    ok &= Check(!copied.changedChart && ui.clipboard.span == 3 &&
                ui.clipboard.notes.size() == 2 &&
                ui.clipboard.notes[1].beat == 2,
                "Ctrl+C copies group using relative beat positions");

    ApplyEditorControls(chart, ui, Click(cursor + 36.0f * 4.0f, 477));
    ok &= Check(!ui.HasRange() && chart.SelectedBeat() == 12,
                "clicking destination clears previous range selection");
    EditorControls paste;
    paste.pasteRange = true;
    const auto pasted = ApplyEditorControls(chart, ui, paste);
    ok &= Check(pasted.changedChart &&
                chart.Data().route.turns.size() == 4 &&
                chart.Data().route.turns[2].beat == 12 &&
                chart.Data().route.turns[3].beat == 14,
                "Ctrl+V pastes selected turn pattern starting at beat 12");

    // Shift-click builds a range from the currently selected beat.
    EditorControls extend = Click(cursor + 72, 477);
    extend.shiftHeld = true;
    ApplyEditorControls(chart, ui, extend);
    ok &= Check(ui.HasRange() && ui.rangeFirst == 12 && ui.rangeLast == 14,
                "Shift+click extends selection without opening another tab");
    EditorControls invert;
    invert.invertRange = true;
    const auto flipped = ApplyEditorControls(chart, ui, invert);
    ok &= Check(flipped.changedChart &&
                chart.Data().route.turns[2].direction == Turn::Right &&
                chart.Data().route.turns[3].direction == Turn::Left,
                "I inverts Left and Right for selected interval");
    EditorControls undo;
    undo.undo = true;
    ApplyEditorControls(chart, ui, undo);
    ok &= Check(chart.Data().route.turns[2].direction == Turn::Left,
                "Ctrl+Z undoes the entire inversion");
    EditorControls redo;
    redo.redo = true;
    ApplyEditorControls(chart, ui, redo);
    ok &= Check(chart.Data().route.turns[2].direction == Turn::Right,
                "Ctrl+Y redoes inverted group");
    EditorControls shift;
    shift.shiftRangePrevious = true;
    const auto shifted = ApplyEditorControls(chart, ui, shift);
    ok &= Check(shifted.changedChart && ui.rangeFirst == 11 &&
                ui.rangeLast == 13 && chart.Data().route.turns[2].beat == 11 &&
                chart.Data().route.turns[3].beat == 13,
                "Ctrl+Shift+Left moves whole highlighted group one beat");

    if (!ok) return 1;
    std::cout << "EDITOR_UI_PASS tabs=3 grid=8x8 preview=1 zoom=1\n";
    return 0;
}
