#include "app/chart_editor_ui.h"
#include "app/chart_editor_layout.h"
#include "app/visual_style.h"
#include "app/viewport.h"

#include <cmath>
#include <iostream>
#include <utility>

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
    // A physical window maps both drawing and hitboxes to the same virtual
    // positions, with the gameplay square touching the limiting window edge.
    const auto wide = ViewportTransform::Gameplay(1280, 800);
    const auto portrait = ViewportTransform::Gameplay(600, 800);
    const auto menu = ViewportTransform::Menu(1280, 800);
    const auto wideClip = wide.Scissor({210, 0, 540, 540});
    const auto portraitClip = portrait.Scissor({210, 0, 540, 540});
    const auto menuPoint = menu.ToVirtual({menu.offsetX + 450 * menu.scale,
                                            menu.offsetY + 270 * menu.scale});
    ok &= Check(std::abs(wideClip.x - 240) <= 1 &&
                wideClip.y == 0 && std::abs(wideClip.width - 800) <= 2 &&
                wideClip.height == 800 &&
                std::abs(portraitClip.x) <= 1 &&
                std::abs(portraitClip.y - 100) <= 1 &&
                std::abs(portraitClip.width - 600) <= 2 &&
                std::abs(portraitClip.height - 600) <= 2 &&
                std::abs(menuPoint.x - 450.0f) < 0.001f &&
                std::abs(menuPoint.y - 270.0f) < 0.001f,
                "shared viewport keeps square, clipping and input aligned");
    ok &= Check(RouteVisualStyle::lookaheadBeats == 4 &&
                std::abs(TrajectoryOpacity(0.0) - 1.0f) < 1e-6f &&
                std::abs(TrajectoryOpacity(2.0) - 0.5f) < 1e-6f &&
                std::abs(TrajectoryOpacity(4.0)) < 1e-6f,
                "one-bar red path uses four beats and a linear fade");
    ChartEditor chart;
    EditorUiState ui;
    ok &= Check(ui.tab == EditorTab::Route &&
                chart.Data().route.gridWidth == 8 &&
                chart.Data().route.gridHeight == 8 &&
                chart.Data().route.start.x == 3 &&
                chart.Data().route.start.y == 3,
                "editor opens with 8x8 square view and automatic route origin");
    const detail::EditorView defaultGeometry(chart.Data().route, 0.0);
    ok &= Check(std::abs(defaultGeometry.step - 30.0f) < 0.001f,
                "8-unit preview scale is unchanged after removal of the drawn grid");

    Chart normalized = chart.Data().route;
    normalized.initialHeading = Heading::UpRight;
    normalized.lengthBeats = 8;
    normalized.turns = {{2, Turn::Right45}};
    const detail::EditorView preview(normalized, 0.0);
    const GridPoint firstPoint = RoutePositionAt(normalized, 0.0);
    const GridPoint nextPoint = RoutePositionAt(normalized, 1.0);
    const GridPoint junction = RoutePositionAt(normalized, 2.0);
    const Vector2 screen0 = preview.Point(firstPoint);
    const Vector2 screen1 = preview.Point(nextPoint);
    const Vector2 screenJunction = preview.Point(junction);
    const auto visibleTurns = PreviewTurnPoints(normalized, 0.0, 4);
    ok &= Check(std::abs(std::hypot(screen1.x - screen0.x,
                                     screen1.y - screen0.y) - preview.step) < 0.001f &&
                visibleTurns.size() == 1 &&
                std::abs(screenJunction.x - preview.Point(visibleTurns[0].position).x)
                    < 0.001f &&
                std::abs(screenJunction.y - preview.Point(visibleTurns[0].position).y)
                    < 0.001f &&
                std::abs((screenJunction.x - screen0.x) / preview.step -
                         std::round((screenJunction.x - screen0.x) / preview.step)) >
                    0.1f,
                "grid-free preview maintains equal-length diagonal steps and exact junctions");

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
    ApplyEditorControls(chart, ui, Type("-175"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(std::abs(chart.Data().offsetSeconds + 0.175) < 1e-10,
                "negative whole-millisecond offset entered directly");

    ApplyEditorControls(chart, ui, Click(145, 226));
    ApplyEditorControls(chart, ui, Type("My rhythm chart"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().title == "My rhythm chart",
                "chart title can be typed in Audio tab");

    const auto audioEmpty = ApplyEditorControls(chart, ui, Click(116, 390));
    ok &= Check(audioEmpty.speedIndex == -1,
                "practice speed buttons are no longer in the Audio tab");

    ApplyEditorControls(chart, ui, Click(300, 89));
    ok &= Check(ui.tab == EditorTab::Set, "Song Set management tab opens");
    const auto titleBeforeSet = chart.Data().title;
    EditorControls noModification;
    noModification.removeTurn = true;
    noModification.undo = true;
    noModification.snapMore = true;
    ApplyEditorControls(chart, ui, noModification);
    ok &= Check(chart.Data().title == titleBeforeSet && ui.tab == EditorTab::Set,
                "SET tab does not mutate chart from editing shortcuts");
    ok &= Check(detail::ClickedSetAction({240, 353}) ==
                    EditorSetAction::NewDifficulty &&
                detail::ClickedSetAction({370, 383}) ==
                    EditorSetAction::DeleteDifficulty &&
                detail::ClickedSetAction({370, 416}) ==
                    EditorSetAction::Restore,
                "Song Set action buttons have distinct hitboxes");
    ok &= Check(detail::kAudioPreviewMs.x <= 168 &&
                detail::kAudioPreviewMs.y <= 390 &&
                detail::kAudioPreviewMs.x + detail::kAudioPreviewMs.width > 168 &&
                detail::kAudioPreviewMs.y + detail::kAudioPreviewMs.height > 390 &&
                detail::ClickedSetAction({320, 280}) == EditorSetAction::None,
                "Song preview lives in AUDIO and has no increment buttons");
    ok &= Check(detail::ClickedSetAction({85, 317}) ==
                    EditorSetAction::AttachBackground &&
                detail::ClickedSetAction({285, 317}) ==
                    EditorSetAction::RemoveBackground,
                "background actions remain in SET");
    // Ruler labels are real music time; renderer/click detection agree
    // even at fractional selected positions and different zoom values.
    ChartEditor ruler;
    int barFound = -1;
    detail::ForEachTimelineTimeLabel(ruler, EditorUiState{}, 0.0,
        [&](int tick, Rectangle bounds) {
            if (tick == 4 && bounds.width > 60 && bounds.y < 470)
                barFound = static_cast<int>(std::llround(
                    ruler.AudioSecondsAtBeat(tick) * 1000));
        });
    ok &= Check(barFound == 2000,
                "timeline time labels map to whole audio milliseconds");
    // FIELD has one integer square size, no editable start coordinates.
    ApplyEditorControls(chart, ui, Click(783, 89));
    ok &= Check(ui.tab == EditorTab::Field, "Field tab opens");
    ApplyEditorControls(chart, ui, Click(157, 247));
    ApplyEditorControls(chart, ui, Type("12"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().route.gridWidth == 12 &&
                chart.Data().route.gridHeight == 12 &&
                chart.Data().route.start.x == 5 &&
                chart.Data().route.start.y == 5,
                "typing a single size 12 sets 12x12 with automatic start");
    ok &= Check(std::abs(
                    detail::EditorView(chart.Data().route, 0.0).step - 20.0f) <
                    0.001f,
                "view size still determines square preview magnification");
    ApplyEditorControls(chart, ui, Click(598, 247));
    ok &= Check(!ui.EditingText(),
                "removed start coordinate field cannot receive focus");
    ApplyEditorControls(chart, ui, Click(157, 247));
    ApplyEditorControls(chart, ui, Type("8/8"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(ui.EditingText() &&
                chart.Data().route.gridWidth == 12 &&
                !ui.feedback.empty(),
                "width/height matrix input is no longer accepted");
    EditorControls escape;
    escape.escape = true;
    ApplyEditorControls(chart, ui, escape);
    ApplyEditorControls(chart, ui, Click(157, 247));
    ApplyEditorControls(chart, ui, Type("4"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(!ui.EditingText() &&
                chart.Data().route.gridWidth == 4 &&
                chart.Data().route.gridHeight == 4 &&
                chart.Data().route.start.x == 1 &&
                chart.Data().route.start.y == 1,
                "shrinking square automatically re-centers the hidden start");
    ApplyEditorControls(chart, ui, Click(155, 343));
    ok &= Check(!ui.EditingText(),
                "song duration stays read-only on Field");

    ok &= Check(chart.SetSongDuration(10.0) &&
                chart.Data().route.lengthBeats > 0,
                "loading audio derives timeline length");

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
    chart.SelectBeat(chart.Data().route.firstBeat);
    ApplyEditorControls(chart, ui, Click(60, 477));
    ok &= Check(chart.SelectedBeat() == chart.Data().route.firstBeat,
                "timeline navigation clamps to derived earliest song beat");

    // ROUTE grid is for preview only; clicking nodes cannot reposition
    // the hidden automatic chart origin.
    chart.SelectBeat(chart.Data().route.firstBeat);
    const auto untouchedStart = chart.Data().route.start;
    const auto clickedGrid = ApplyEditorControls(
        chart, ui, Click(420.0f, 253.0f));
    ok &= Check(!clickedGrid.changedChart &&
                chart.Data().route.start.x == untouchedStart.x &&
                chart.Data().route.start.y == untouchedStart.y,
                "clicking editor lattice never edits starting coordinates");

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
    ok &= Check(ui.HasRange() && ui.rangeFirst == 12 &&
                ui.rangeLast == 14 && chart.SelectedBeat() == 12,
                "Ctrl+V keeps the full pasted span selected, including rests");

    chart.SelectBeat(chart.Data().route.lengthBeats - 1);
    const auto failedPaste = ApplyEditorControls(chart, ui, paste);
    ok &= Check(!failedPaste.changedChart && ui.HasRange() &&
                ui.rangeFirst == 12 && ui.rangeLast == 14 &&
                chart.Data().route.turns.size() == 4,
                "out-of-bounds paste leaves previous selection and notes intact");
    chart.SelectBeat(12);

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

    // The on-screen PASTE button must preserve selection just like Ctrl+V.
    chart.SelectBeat(20);
    ui.rangeFirst = ui.rangeLast = -1;
    const auto clickedPaste = ApplyEditorControls(chart, ui, Click(120, 262));
    ok &= Check(clickedPaste.changedChart && ui.HasRange() &&
                ui.rangeFirst == 20 && ui.rangeLast == 22 &&
                chart.Data().route.turns.back().beat == 22,
                "ROUTE paste button selects newly pasted notes and gaps");

    // Regression: dragging off the right edge must keep scrolling the ruler
    // instead of capping selection at the last visible tick.
    ChartEditor longChart;
    longChart.NewDraft();
    ok &= Check(longChart.SetSongDuration(180.0) &&
                longChart.Data().route.lengthBeats >= 300,
                "long song provides room for drag autoscroll");
    EditorUiState scrollUi;
    longChart.SelectBeat(20);
    EditorControls beginDrag = Click(cursor, 477);
    beginDrag.mouseHeld = true;
    beginDrag.elapsedSeconds = 1.0f / 60.0f;
    ApplyEditorControls(longChart, scrollUi, beginDrag);
    const int firstVisibleEdge = longChart.SelectedBeat() + 16;
    EditorControls atRightEdge;
    atRightEdge.mouse = {909, 477};
    atRightEdge.mouseHeld = true;
    atRightEdge.elapsedSeconds = 0.05f;
    for (int frame = 0; frame < 80; ++frame)
        ApplyEditorControls(longChart, scrollUi, atRightEdge);
    ok &= Check(scrollUi.HasRange() && scrollUi.rangeFirst == 20 &&
                scrollUi.rangeLast > firstVisibleEdge &&
                longChart.SelectedBeat() == scrollUi.rangeLast,
                "holding a range at the right edge extends beyond the visible screen");
    EditorControls stopDrag;
    stopDrag.mouseReleased = true;
    stopDrag.mouse = {909, 477};
    ApplyEditorControls(longChart, scrollUi, stopDrag);
    const int finishedSelection = longChart.SelectedBeat();
    for (int i = 0; i < 20; ++i) {
        EditorControls idle;
        idle.mouse = {909, 477};
        idle.elapsedSeconds = 0.05f;
        ApplyEditorControls(longChart, scrollUi, idle);
    }
    ok &= Check(!scrollUi.draggingTimeline &&
                longChart.SelectedBeat() == finishedSelection,
                "autoscrolling stops immediately on mouse release");
    // Left edge also supports continuous backwards range selection.
    longChart.SelectBeat(170);
    scrollUi = EditorUiState{};
    ApplyEditorControls(longChart, scrollUi, beginDrag);
    EditorControls atLeftEdge;
    atLeftEdge.mouse = {51, 477};
    atLeftEdge.mouseHeld = true;
    atLeftEdge.elapsedSeconds = 0.05f;
    for (int i = 0; i < 50; ++i)
        ApplyEditorControls(longChart, scrollUi, atLeftEdge);
    ok &= Check(scrollUi.HasRange() && scrollUi.rangeLast == 170 &&
                scrollUi.rangeFirst < 140,
                "left-edge drag scrolls backwards continuously");

    // Holding Ctrl+arrows repeats four-beat navigation at increasing speed,
    // without modifying authored chart events.
    scrollUi = EditorUiState{};
    longChart.SelectBeat(4);
    const auto unchangedTurns = longChart.Data().route.turns.size();
    EditorControls initialBar;
    initialBar.nextBar = true;
    initialBar.heldBarDirection = 1;
    initialBar.elapsedSeconds = 0.016f;
    ApplyEditorControls(longChart, scrollUi, initialBar);
    ok &= Check(longChart.SelectedBeat() == 8,
                "first Ctrl+Right keydown jumps exactly one bar");
    EditorControls heldBar;
    heldBar.heldBarDirection = 1;
    heldBar.elapsedSeconds = 0.05f;
    for (int i = 0; i < 30; ++i)
        ApplyEditorControls(longChart, scrollUi, heldBar);
    ok &= Check(longChart.SelectedBeat() > 40 &&
                unchangedTurns == longChart.Data().route.turns.size(),
                "holding Ctrl+Right rapidly traverses many bars without editing");
    EditorControls releasedBar;
    releasedBar.elapsedSeconds = 0.05f;
    ApplyEditorControls(longChart, scrollUi, releasedBar);
    const int releasedBeat = longChart.SelectedBeat();
    for (int i = 0; i < 25; ++i)
        ApplyEditorControls(longChart, scrollUi, releasedBar);
    ok &= Check(longChart.SelectedBeat() == releasedBeat,
                "bar navigation repeat stops on key release");
    EditorControls reverseBar;
    reverseBar.previousBar = true;
    reverseBar.heldBarDirection = -1;
    reverseBar.elapsedSeconds = 0.016f;
    ApplyEditorControls(longChart, scrollUi, reverseBar);
    ok &= Check(longChart.SelectedBeat() == releasedBeat - 4,
                "Ctrl+Left changes direction with a single initial step");
    for (int i = 0; i < 100; ++i) {
        reverseBar.previousBar = false;
        reverseBar.elapsedSeconds = 0.05f;
        ApplyEditorControls(longChart, scrollUi, reverseBar);
    }
    ok &= Check(longChart.SelectedBeat() == longChart.Data().route.firstBeat,
                "repeating Ctrl+Left clamps at earliest song beat");

    // Selecting extra empty ticks around notes must not shift the clipboard.
    ChartEditor padded;
    EditorUiState paddedUi;
    padded.SelectBeat(6);
    padded.PutTurn(Turn::Left);
    padded.SelectBeat(9);
    padded.PutTurn(Turn::Right);
    padded.SelectBeat(13);
    paddedUi.rangeFirst = 2;
    paddedUi.rangeLast = 13; // 4 empty beats before + 4 after.
    EditorControls paddedCopy;
    paddedCopy.copyRange = true;
    ApplyEditorControls(padded, paddedUi, paddedCopy);
    ok &= Check(paddedUi.rangeFirst == 6 && paddedUi.rangeLast == 9 &&
                paddedUi.clipboard.span == 4 &&
                paddedUi.clipboard.notes.size() == 2 &&
                paddedUi.clipboard.notes[0].beat == 0 &&
                paddedUi.clipboard.notes[1].beat == 3,
                "Ctrl+C trims empty range edges but keeps gaps inside pattern");
    padded.SelectBeat(15);
    EditorControls paddedPaste;
    paddedPaste.pasteRange = true;
    ApplyEditorControls(padded, paddedUi, paddedPaste);
    ok &= Check(padded.Data().route.turns[2].beat == 15 &&
                padded.Data().route.turns[3].beat == 18 &&
                paddedUi.rangeFirst == 15 && paddedUi.rangeLast == 18,
                "pasting aligns the first real note with the destination beat");

    // ROUTE consumes exactly the same four direction bindings as PLAY.
    // Here the gameplay keys are rebound to WASD; the editor must author
    // the same diagonal as Up+Right rather than using hard-coded J+K.
    GameSettings rebound;
    std::string bindError;
    ok &= Check(rebound.Rebind(GameAction::Left, KEY_A, bindError) &&
                rebound.Rebind(GameAction::Up, KEY_W, bindError) &&
                rebound.Rebind(GameAction::Right, KEY_D, bindError) &&
                rebound.Rebind(GameAction::Down, KEY_S, bindError),
                "all four gameplay direction bindings can be customized");
    ChartEditor sharedKeys;
    EditorUiState sharedUi;
    EditorControls sharedChord;
    sharedChord.directionKeysHeld =
        EditorDirectionMask({true, true, false, false}); // W + D
    sharedChord.directionChordWindowSeconds = rebound.chordWindowMs / 1000.0f;
    sharedChord.elapsedSeconds = 0.016f;
    const auto sharedApplied =
        ApplyEditorControls(sharedKeys, sharedUi, sharedChord);
    ok &= Check(sharedApplied.changedChart &&
                sharedKeys.SelectedTurn() &&
                SampleRouteStep(sharedKeys.Data().route, 0).heading ==
                    Heading::UpRight &&
                EditorDirectionShortcut(rebound, 12u) == "W+D",
                "rebound gameplay W+D inserts an UpRight editor note and updates label");

    // When Ctrl/Shift is introduced halfway through a chord, an unfinished
    // direction must be canceled rather than inserting a stray note.
    ChartEditor modified;
    EditorUiState modifiedUi;
    EditorControls singleDirection;
    singleDirection.directionKeysHeld =
        EditorDirectionMask({true, false, false, false}); // rebound W
    singleDirection.directionChordWindowSeconds = 0.16f;
    singleDirection.elapsedSeconds = 0.016f;
    ApplyEditorControls(modified, modifiedUi, singleDirection);
    EditorControls withModifier;
    withModifier.directionKeysSuppressed = true;
    ApplyEditorControls(modified, modifiedUi, withModifier);
    ApplyEditorControls(modified, modifiedUi, singleDirection);
    singleDirection.directionKeysHeld = 0;
    ApplyEditorControls(modified, modifiedUi, singleDirection);
    ok &= Check(!modified.SelectedTurn(),
                "modifier navigation cancels unfinished gameplay-key placement");

    // The editor's single-key grace window follows the gameplay option.
    ChartEditor customChordTime;
    EditorUiState chordTimeUi;
    EditorControls longWindow;
    longWindow.directionKeysHeld = EditorDirectionMask({true, false, false, false});
    longWindow.directionChordWindowSeconds = 0.16f;
    longWindow.elapsedSeconds = 0.03f;
    for (int frame = 0; frame < 5; ++frame)
        ApplyEditorControls(customChordTime, chordTimeUi, longWindow);
    ok &= Check(!customChordTime.SelectedTurn(),
                "160 ms game chord window also delays single editor key placement");
    for (int frame = 0; frame < 2; ++frame)
        ApplyEditorControls(customChordTime, chordTimeUi, longWindow);
    ok &= Check(customChordTime.SelectedTurn() &&
                SampleRouteStep(customChordTime.Data().route, 0).heading ==
                    Heading::Up,
                "single direction places once after configurable game chord window");

    // Absolute compass masks represent game Left/Down/Up/Right regardless
    // of which physical keys are bound in Settings.
    // The adjacent pairs place diagonals without an unwanted cardinal note.
    const struct { unsigned keys; Heading heading; } diagonals[] = {
        {1u | 4u, Heading::UpLeft}, {4u | 8u, Heading::UpRight},
        {1u | 2u, Heading::DownLeft}, {2u | 8u, Heading::DownRight}
    };
    for (const auto& pair : diagonals) {
        ChartEditor c;
        EditorUiState u;
        EditorControls keys;
        keys.directionKeysHeld = pair.keys;
        keys.elapsedSeconds = 0.016f;
        const auto placed = ApplyEditorControls(c, u, keys);
        ok &= Check(placed.changedChart && c.SelectedTurn() &&
                    SampleRouteStep(c.Data().route, 0).heading == pair.heading,
                    "simultaneous directional keys author diagonal compass heading");
        const auto beforeCount = c.Data().route.turns.size();
        for (int frame = 0; frame < 20; ++frame)
            ApplyEditorControls(c, u, keys);
        keys.directionKeysHeld = 0;
        ApplyEditorControls(c, u, keys);
        ok &= Check(c.Data().route.turns.size() == beforeCount,
                    "holding and releasing diagonal keys places one note");
    }
    {
        ChartEditor c;
        EditorUiState u;
        EditorControls keys;
        keys.directionKeysHeld = 4u; // J pressed first
        keys.elapsedSeconds = 0.016f;
        ApplyEditorControls(c, u, keys);
        ok &= Check(!c.SelectedTurn(), "single J waits briefly for diagonal partner");
        keys.directionKeysHeld = 4u | 8u; // K joins within the grace period
        ApplyEditorControls(c, u, keys);
        ok &= Check(c.SelectedTurn() &&
                    SampleRouteStep(c.Data().route, 0).heading == Heading::UpRight,
                    "closely staggered J+K places diagonal, not an initial up note");
    }
    {
        ChartEditor c;
        EditorUiState u;
        EditorControls tap;
        tap.directionKeysHeld = 2u; // F = down
        tap.elapsedSeconds = 0.016f;
        ApplyEditorControls(c, u, tap);
        tap.directionKeysHeld = 0u;
        const auto applied = ApplyEditorControls(c, u, tap);
        ok &= Check(applied.changedChart && c.SelectedTurn() &&
                    SampleRouteStep(c.Data().route, 0).heading == Heading::Down,
                    "a quickly tapped F still places one down note on release");
    }
    for (const unsigned invalid : {1u | 8u, 2u | 4u, 15u}) {
        ChartEditor c;
        EditorUiState u;
        EditorControls keys;
        keys.directionKeysHeld = invalid;
        const auto blocked = ApplyEditorControls(c, u, keys);
        ok &= Check(!blocked.changedChart && !c.SelectedTurn(),
                    "opposite or 3+ compass keys never create an ambiguous note");
    }
    for (const auto& cardinal : {std::pair<unsigned, Heading>{1u, Heading::Left},
                                 {8u, Heading::Right}}) {
        ChartEditor c;
        c.RotateHeading45(-2); // Start upward so neither is opposite.
        EditorUiState u;
        EditorControls keys;
        keys.directionKeysHeld = cardinal.first;
        keys.elapsedSeconds = 0.05f;
        ApplyEditorControls(c, u, keys);
        ApplyEditorControls(c, u, keys);
        ApplyEditorControls(c, u, keys);
        ok &= Check(c.SelectedTurn() &&
                    SampleRouteStep(c.Data().route, 0).heading == cardinal.second,
                    "D and K single keys select their absolute compass headings");
    }

    // Eight-way compass on ROUTE authors actual diagonal route notes.
    ChartEditor compassUi;
    EditorUiState diagonalUi;
    auto chooseNE = ApplyEditorControls(compassUi, diagonalUi, Click(882, 292));
    ok &= Check(chooseNE.changedChart &&
                compassUi.SelectedTurn() &&
                compassUi.SelectedTurn()->direction == Turn::Left45 &&
                SampleRouteStep(compassUi.Data().route, 0).heading == Heading::UpRight,
                "compass NE button authors the appropriate 45 degree turn");
    compassUi.SelectBeat(1);
    auto chooseEast = ApplyEditorControls(compassUi, diagonalUi, Click(882, 331));
    ok &= Check(chooseEast.changedChart &&
                compassUi.SelectedTurn() &&
                compassUi.SelectedTurn()->direction == Turn::Right45 &&
                SampleRouteStep(compassUi.Data().route, 1).heading == Heading::Right,
                "compass cardinal button supports diagonal-to-cardinal transitions");
    EditorControls halfTurn;
    halfTurn.addLeft135 = true;
    compassUi.SelectBeat(2);
    ApplyEditorControls(compassUi, diagonalUi, halfTurn);
    ok &= Check(compassUi.SelectedTurn() &&
                compassUi.SelectedTurn()->direction == Turn::Left135,
                "editor keyboard can author 135 degree turns");
    EditorControls rotateStart;
    rotateStart.headingRight = true;
    ApplyEditorControls(compassUi, diagonalUi, rotateStart);
    ok &= Check(compassUi.Data().route.initialHeading == Heading::DownRight,
                "Q/E start heading supports diagonal increments");

    // A compass direction opposite to the incoming route is an ordinary
    // note, whether clicked or pressed using the game direction binding.
    ChartEditor reverseUi;
    EditorUiState reverseState;
    const auto clickBack = ApplyEditorControls(
        reverseUi, reverseState, Click(766, 330)); // L from default Right.
    ok &= Check(clickBack.changedChart && reverseUi.SelectedTurn() &&
                reverseUi.SelectedTurn()->direction == Turn::Reverse &&
                SampleRouteStep(reverseUi.Data().route, 0).heading == Heading::Left,
                "direction pad authors a normal 180-degree note");
    reverseUi.SelectBeat(1);
    EditorControls tapRight;
    tapRight.directionKeysHeld = 8u; // K: opposite to current Left.
    tapRight.elapsedSeconds = 0.05f;
    ApplyEditorControls(reverseUi, reverseState, tapRight);
    ApplyEditorControls(reverseUi, reverseState, tapRight);
    const auto keyReverse = ApplyEditorControls(reverseUi, reverseState, tapRight);
    ok &= Check(keyReverse.changedChart && reverseUi.SelectedTurn() &&
                reverseUi.SelectedTurn()->direction == Turn::Reverse &&
                SampleRouteStep(reverseUi.Data().route, 1).heading == Heading::Right,
                "game direction key can author the return trip without a new control");

    // Placement subdivision controls are independent from zoom and BPM.
    ChartEditor snapChart;
    EditorUiState snapUi;
    snapChart.SetBpm(250);
    snapChart.SelectBeat(5);
    snapChart.PutTurn(Turn::Left);
    const auto originalTime = snapChart.Data().route.turns.front().timeMs;
    const auto half = ApplyEditorControls(snapChart, snapUi, Click(614, 430));
    const auto third = ApplyEditorControls(snapChart, snapUi, Click(614, 430));
    ok &= Check(half.changedChart && third.changedChart &&
                snapChart.Data().route.snapDivision == 3 &&
                snapChart.Data().route.ticksPerBeat == 6 &&
                snapChart.Data().route.bpm == 250 &&
                snapChart.Data().route.turns.front().timeMs == originalTime &&
                snapUi.timelineZoomIndex == 3,
                "MORE grid button reaches triplets without changing BPM, notes or zoom");
    const auto coarser = ApplyEditorControls(snapChart, snapUi, Click(541, 430));
    ok &= Check(coarser.changedChart &&
                snapChart.Data().route.snapDivision == 2 &&
                snapChart.Data().route.turns.front().timeMs == originalTime,
                "LESS grid button retains old note timestamp");
    EditorControls zoomOnly;
    zoomOnly.zoomIn = true;
    ApplyEditorControls(snapChart, snapUi, zoomOnly);
    ok &= Check(snapUi.timelineZoomIndex == 4 &&
                snapChart.Data().route.snapDivision == 2,
                "existing zoom buttons remain independent of snap division");

    if (!ok) return 1;
    std::cout << "EDITOR_UI_PASS tabs=3 grid=8x8 preview=1 zoom=1\n";
    return 0;
}
