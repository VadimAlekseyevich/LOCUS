#include "app/chart_editor_ui.h"
#include "core/utf8.h"
#include "app/chart_editor_layout.h"
#include "app/editor_tempo_controls.h"
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
    ok &= Check(detail::ClickedSetAction({280, 283}) ==
                EditorSetAction::OpenFolder &&
                detail::ClickedSetAction({290, 314}) ==
                EditorSetAction::RemoveBackground,
                "Set folder shortcut owns an isolated clickable hitbox");
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
    // Diagonals count both their physical cardinal presses as well.
    Chart counterFixture;
    counterFixture.turns = {
        {0, Heading::Up}, {1, Heading::UpRight},
        {2, Heading::DownLeft}, {3, Heading::UpRight},
        {4, Heading::Left}, {5, Heading::DownRight},
        {6, Heading::DownRight}, {7, Heading::Down}
    };
    const auto counters = detail::DirectionPressCounts(counterFixture);
    ok &= Check(counters == std::array<int, 8>{3, 4, 4, 2, 2, 2, 1, 0},
                "direction pad counts cardinals plus diagonal components");

    ChartEditor chart;
    EditorUiState ui;
    ok &= Check(ui.tab == EditorTab::Route &&
                chart.Data().route.gridWidth == 8 &&
                chart.Data().route.gridHeight == 8 &&
                chart.Data().route.start.x == 3 &&
                chart.Data().route.start.y == 3,
                "editor opens with 8x8 square view and automatic route origin");
    // ROUTE preview has independent speed/lookahead controls; chart grid
    // metadata never changes the visible geometry or playback time.
    // These strips must remain spatially separate from toolbar and footer.
    ok &= Check(detail::kTimelineY == 115.0f &&
                detail::kTimelineY + detail::kTimelineHeight <=
                    detail::kRouteTest.y &&
                detail::kSongOverview.y > detail::kRouteTimeValue.y +
                    detail::kRouteTimeValue.height &&
                detail::kSongOverview.y + detail::kSongOverview.height <=
                    detail::RouteSpeedBounds(0).y &&
                std::abs(detail::SongOverviewX(30.0, 120.0) -
                    (detail::kSongOverview.x + detail::kSongOverview.width * .25f))
                    < 0.001f &&
                std::abs(detail::SongOverviewSeconds(
                    detail::kSongOverview.x +
                    detail::kSongOverview.width * .25f, 120.0) - 30.0)
                    < 0.001,
                "top note ruler and bottom song overview have independent geometry");

    const auto referenceSquare = detail::ComparisonPreviewBounds(3, 0);
    ok &= Check(referenceSquare.width == 222.0f &&
                referenceSquare.height == 222.0f &&
                referenceSquare.y >= detail::kTimelineY +
                    detail::kTimelineHeight &&
                referenceSquare.y + referenceSquare.height <
                    detail::kSongOverview.y,
                "three enlarged preview panes fit between toolbar and overview");

    // Audio settings are no longer stacked on top of the route grid.
    ApplyEditorControls(chart, ui, Click(612, 89));
    ok &= Check(ui.tab == EditorTab::Audio, "Audio tab opens");
    // BPM/offset editing is owned by the shared tempo table, not
    // the old single-difficulty numeric text fields.
    const auto noOldField = ApplyEditorControls(chart, ui, Click(145, 312));
    ok &= Check(ui.active == EditorField::None &&
                !noOldField.changedChart,
                "retired standalone BPM field cannot be opened");

    auto applyTempo = [&](const std::vector<TempoMarker>& map) {
        return chart.SetTempoMap(map);
    };
    const auto currentTempo = [&]() {
        return EffectiveTempoMap(chart.Data().route, chart.Data().offsetSeconds);
    };
    EditorControls enter;
    enter.enter = true;
    HandleTempoTable(ui, Click(400, 298), currentTempo(), chart, applyTempo);
    ok &= Check(ui.tempoEditingRow == 0 && ui.tempoEditingBpm,
                "AUDIO tempo table opens BPM cell");
    HandleTempoTable(ui, Type("400,5"), currentTempo(), chart, applyTempo);
    HandleTempoTable(ui, enter, currentTempo(), chart, applyTempo);
    ok &= Check(!ui.EditingText() &&
                std::abs(chart.Data().route.bpm - 400.5) < 1e-9,
                "tempo table accepts decimal BPM");

    HandleTempoTable(ui, Click(145, 298), currentTempo(), chart, applyTempo);
    HandleTempoTable(ui, Type("-175"), currentTempo(), chart, applyTempo);
    HandleTempoTable(ui, enter, currentTempo(), chart, applyTempo);
    ok &= Check(std::abs(chart.Data().offsetSeconds + 0.175) < 1e-10 &&
                currentTempo().size() == 1,
                "first tempo marker defines shared signed offset and initial BPM");

    ApplyEditorControls(chart, ui, Click(145, 226));
    ApplyEditorControls(chart, ui, Type("My rhythm chart"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().title == "My rhythm chart",
                "chart title can be typed in Audio tab");
    ApplyEditorControls(chart, ui, Click(145, 226));
    ApplyEditorControls(chart, ui, Type(u8"Новый чарт 🎵"));
    EditorControls backspaceUtf8;
    backspaceUtf8.backspace = true;
    ApplyEditorControls(chart, ui, backspaceUtf8);
    ApplyEditorControls(chart, ui, Type(u8"ёж"));
    ApplyEditorControls(chart, ui, enter);
    ok &= Check(chart.Data().title == u8"Новый чарт ёж" &&
                utf8::Valid(chart.Data().title),
                "title field input and backspace preserve UTF-8 characters");

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
    ok &= Check(detail::kAudioPreviewMs.x <= 650 &&
                detail::kAudioPreviewMs.y <= 220 &&
                detail::kAudioPreviewMs.x + detail::kAudioPreviewMs.width > 650 &&
                detail::kAudioPreviewMs.y + detail::kAudioPreviewMs.height > 220 &&
                detail::ClickedSetAction({168, 280}) == EditorSetAction::None,
                "Song preview lives in AUDIO and has no increment buttons");
    ok &= Check(detail::ClickedSetAction({85, 317}) ==
                    EditorSetAction::AttachBackground &&
                detail::ClickedSetAction({285, 317}) ==
                    EditorSetAction::RemoveBackground,
                "background actions remain in SET");
    // Ruler labels are real music time; renderer/click detection agree
    // even at fractional selected positions and different zoom values.
    ChartEditor ruler;
    ok &= Check(detail::RouteTimeMs(ruler, 4.0) == 2000 &&
                detail::RouteTimeMs(ruler, 4.5) == 2250 &&
                detail::kRouteTimeValue.x <= 770 &&
                detail::kRouteTimeValue.y <= 267 &&
                detail::kRouteTimeValue.x +
                    detail::kRouteTimeValue.width > 770 &&
                detail::kRouteTimeValue.y +
                    detail::kRouteTimeValue.height > 267,
                "clickable Route time uses fractional beat audio milliseconds");
    EditorUiState timeCopyUi;
    EditorControls timeCopyClick = Click(770, 267);
    timeCopyClick.timelineViewBeat = 4.5;
    const auto timeCopyAction = ApplyEditorControls(ruler, timeCopyUi, timeCopyClick);
    ok &= Check(timeCopyAction.clipboardWrite == "2250" &&
                timeCopyUi.feedback == "2250 MS COPIED",
                "copy timestamp is a pure command without a native clipboard call");
    // FIELD contains only the starting compass direction. Old grid-size
    // text entry is gone, and heading changes require clicking the controls.
    ApplyEditorControls(chart, ui, Click(783, 89));
    ok &= Check(ui.tab == EditorTab::Field, "FIELD tab opens");
    const auto oldGrid = chart.Data().route.gridWidth;
    const auto previousHeading = chart.Data().route.initialHeading;
    const auto rotateForward =
        ApplyEditorControls(chart, ui, Click(425, 253));
    const auto rotateBackward =
        ApplyEditorControls(chart, ui, Click(122, 253));
    ok &= Check(rotateForward.changedChart && rotateBackward.changedChart &&
                chart.Data().route.initialHeading == previousHeading &&
                chart.Data().route.gridWidth == oldGrid &&
                !ui.EditingText(),
                "FIELD controls change initial heading but never grid size");
    const auto blank = ApplyEditorControls(chart, ui, Click(523, 248));
    ok &= Check(!blank.changedChart && !ui.EditingText(),
                "removed visible-square field is not clickable");

    ok &= Check(chart.SetSongDuration(10.0) &&
                chart.Data().route.lengthBeats > 0,
                "loading audio derives timeline length");
    // Map UI: select the song position, add a segment, edit its BPM,
    // reject a conflicting offset, and delete it without modifying notes.
    ui.tab = EditorTab::Audio;
    chart.SelectBeat(20);
    const auto beforeAdd = currentTempo();
    HandleTempoTable(ui, Click(740, 447), beforeAdd, chart, applyTempo);
    ok &= Check(currentTempo().size() == 2 &&
                currentTempo()[1].bpm == currentTempo()[0].bpm,
                "AUDIO add marker uses selected audio position and prior BPM");
    HandleTempoTable(ui, Click(400, 327), currentTempo(), chart, applyTempo);
    HandleTempoTable(ui, Type("255.5"), currentTempo(), chart, applyTempo);
    HandleTempoTable(ui, enter, currentTempo(), chart, applyTempo);
    ok &= Check(currentTempo().size() == 2 &&
                currentTempo()[1].bpm == 255.5,
                "second BPM table row commits a tempo change");
    HandleTempoTable(ui, Click(145, 327), currentTempo(), chart, applyTempo);
    HandleTempoTable(ui, Type("-175"), currentTempo(), chart, applyTempo);
    HandleTempoTable(ui, enter, currentTempo(), chart, applyTempo);
    ok &= Check(ui.tempoEditingRow == 1 &&
                currentTempo()[1].timeMs != currentTempo()[0].timeMs,
                "tempo table rejects duplicate millisecond offsets");
    EditorControls cancelTempo;
    cancelTempo.escape = true;
    HandleTempoTable(ui, cancelTempo, currentTempo(), chart, applyTempo);
    HandleTempoTable(ui, Click(635, 327), currentTempo(), chart, applyTempo);
    ok &= Check(currentTempo().size() == 1 &&
                !ui.EditingText(),
                "AUDIO delete removes only the chosen later BPM marker");
    ui.tab = EditorTab::Field;

    // Multiple BPM segments preserve absolute note times, including undo.
    const double initialBpm = chart.Data().route.bpm;
    const auto segmentTime = static_cast<std::int64_t>(2500);
    const auto active = currentTempo();
    std::vector<TempoMarker> two = active;
    two.push_back({segmentTime, 200.0});
    ok &= Check(chart.SetTempoMap(two) &&
                currentTempo().size() == 2 &&
                chart.Data().route.bpm == initialBpm &&
                chart.Undo() && currentTempo().size() == 1,
                "adding a tempo change is undoable without resetting song zero");


    // Transport is shared by all tabs.
    const auto song = ApplyEditorControls(chart, ui, Click(113, 519));
    const auto play = ApplyEditorControls(chart, ui, Click(210, 519));
    const auto restart = ApplyEditorControls(chart, ui, Click(324, 519));
    const auto test = ApplyEditorControls(chart, ui, Click(445, 519));
    ok &= Check(song.chooseAudio && play.previewToggle &&
                    restart.previewRestart && test.testPlay,
                "song, play, restart and test remain on every tab");

    ApplyEditorControls(chart, ui, Click(450, 89));
    ok &= Check(ui.tab == EditorTab::Route, "Route tab restores the large grid");
    const auto normal = ApplyEditorControls(chart, ui, Click(98, 519));
    const auto slow = ApplyEditorControls(chart, ui, Click(505, 519));
    ok &= Check(normal.speedIndex == 0 && slow.speedIndex == 4,
                "compact practice speed 100 and 10 percent are clickable on Route");
    // A modal comparison picker consumes clicks and editor shortcuts:
    // dismissing it must not place a note or execute hidden toolbar actions.
    ChartEditor popupChart;
    EditorUiState popupUi;
    popupUi.comparisonPickerOpen = true;
    const auto blockedNote = ApplyEditorControls(
        popupChart, popupUi, Click(766, 292));
    const auto blockedPlay = ApplyEditorControls(
        popupChart, popupUi, Click(195, 129));
    EditorControls blockedDelete;
    blockedDelete.removeTurn = true;
    const auto blockedEdit = ApplyEditorControls(
        popupChart, popupUi, blockedDelete);
    ok &= Check(!blockedNote.changedChart &&
                !blockedPlay.previewToggle && !blockedEdit.changedChart &&
                !popupChart.Dirty() && popupChart.Data().route.turns.empty(),
                "open comparison picker blocks hidden editor actions");

    const auto compactTest = ApplyEditorControls(chart, ui, Click(109, 180));
    const auto unusedTransport = ApplyEditorControls(chart, ui, Click(590, 263));
    ok &= Check(compactTest.testPlay &&
                !unusedTransport.previewToggle &&
                !unusedTransport.previewRestart,
                "ROUTE has TEST but no obsolete PLAY/RESTART/SONG toolbar");

    for (int n = 1; n <= 3; ++n) {
        const auto firstPane = detail::ComparisonPreviewBounds(n, 0);
        ok &= Check(firstPane.width == 222.0f &&
                    firstPane.height == 222.0f &&
                    firstPane.x >= 52.0f &&
                    firstPane.x + firstPane.width <= 734.0f,
                    "editor panes are enlarged and equal-sized");
        for (int i = 1; i < n; ++i) {
            const auto previous = detail::ComparisonPreviewBounds(n, i - 1);
            const auto pane = detail::ComparisonPreviewBounds(n, i);
            ok &= Check(pane.width == firstPane.width &&
                        pane.height == firstPane.height &&
                        pane.x >= previous.x + previous.width + 7.9f &&
                        pane.x + pane.width <= 734.0f &&
                        pane.y + pane.height < detail::kSongOverview.y,
                        "up to three equal previews fit without overlaying overview");
        }
    }
    ok &= Check(detail::ComparisonPreviewBounds(4, 3).width == 0.0f &&
                detail::EditorSpeedControlDelta({861, 405}) == -1 &&
                detail::EditorSpeedControlDelta({909, 405}) == 1 &&
                detail::EditorPathControlDelta({861, 438}) == -1 &&
                detail::EditorPathControlDelta({909, 438}) == 1 &&
                detail::EditorPathControlDelta({861, 405}) == 0 &&
                detail::kEditorSpeedMinus.y > 
                    detail::DirectionPadBounds(2, 0).y +
                    detail::DirectionPadBounds(2, 0).height &&
                detail::kEditorPathPlus.y + detail::kEditorPathPlus.height <
                    detail::kSongOverview.y,
                "separate editor controls do not overlap pad or song overview");

    // At x = 50 + 860/3, the selected beat stays under the fixed playhead.
    constexpr float cursor = 50.0f + 860.0f / 3.0f;
    chart.SelectBeat(15);
    ApplyEditorControls(chart, ui, Click(cursor + 36, 140));
    ok &= Check(chart.SelectedBeat() == 16,
                "timeline crosses 15/16 without any page jump");
    ApplyEditorControls(chart, ui, Click(cursor - 36 * 3, 140));
    ok &= Check(chart.SelectedBeat() == 13,
                "clicking behind the one-third playhead selects a past beat");

    chart.SelectBeat(15);
    EditorControls moving = Click(cursor + 18, 140);
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
    ApplyEditorControls(chart, ui, Click(cursor + 52, 140));
    ok &= Check(chart.SelectedBeat() == 17,
                "zoomed ruler maps clicks to nearby beat ticks");
    ApplyEditorControls(chart, ui, Click(785, 216));
    ok &= Check(ui.timelineZoomIndex == 3,
                "zoom-out button restores the original scale");
    EditorControls zoomOut;
    zoomOut.zoomOut = true;
    for (int n = 0; n < 10; ++n) ApplyEditorControls(chart, ui, zoomOut);
    ok &= Check(ui.timelineZoomIndex == 0, "zoom-out clamps");
    for (int n = 0; n < 10; ++n) ApplyEditorControls(chart, ui, zoomIn);
    ok &= Check(ui.timelineZoomIndex == 6, "zoom-in clamps");
    chart.SelectBeat(chart.Data().route.firstBeat);
    ApplyEditorControls(chart, ui, Click(60, 140));
    ok &= Check(chart.SelectedBeat() == chart.Data().route.firstBeat,
                "timeline navigation clamps to derived earliest song beat");

    // Song-wide overview seeks by actual audio length, not note zoom,
    // BPM pixels/beat or number of authored notes.
    ChartEditor songWide;
    EditorUiState songWideUi;
    const auto noAudioClick =
        ApplyEditorControls(songWide, songWideUi, Click(704, 485));
    ok &= Check(!noAudioClick.changedChart &&
                songWide.SelectedBeat() == 0 && !songWide.Dirty(),
                "overview is inert without a real song duration");
    ok &= Check(songWide.SetSongDuration(120.0),
                "overview fixture has 120s audio");
    const float overviewX = detail::kSongOverview.x;
    const float overviewWidth = detail::kSongOverview.width;
    const float overviewY = detail::kSongOverview.y + 17.0f;
    const auto seek75 = ApplyEditorControls(songWide, songWideUi,
        Click(overviewX + overviewWidth * .75f, overviewY));
    ok &= Check(!seek75.changedChart &&
                songWide.SelectedBeat() == 180 &&
                songWide.Data().route.turns.empty() &&
                !songWideUi.HasRange() && !songWide.Dirty(),
                "75 percent of 120-second audio seeks to song second 90");
    songWideUi.timelineZoomIndex = detail::kZoomMax;
    const auto seek25 = ApplyEditorControls(songWide, songWideUi,
        Click(overviewX + overviewWidth * .25f, overviewY));
    ok &= Check(!seek25.changedChart && songWide.SelectedBeat() == 60,
                "overview seek stays unchanged at maximum note-ruler zoom");
    EditorControls scrub = Click(overviewX + overviewWidth * .1f, overviewY);
    scrub.mouseHeld = true;
    ApplyEditorControls(songWide, songWideUi, scrub);
    EditorControls moveScrub;
    moveScrub.mouseHeld = true;
    moveScrub.mouse = {overviewX + overviewWidth * .5f, overviewY};
    ApplyEditorControls(songWide, songWideUi, moveScrub);
    const int duringScrub = songWide.SelectedBeat();
    EditorControls endScrub;
    endScrub.mouseReleased = true;
    endScrub.mouse = {overviewX + overviewWidth, overviewY};
    ApplyEditorControls(songWide, songWideUi, endScrub);
    ok &= Check(duringScrub == 120 &&
                songWide.SelectedBeat() == songWide.Data().route.lengthBeats - 1 &&
                !songWideUi.draggingSongOverview &&
                !songWide.Dirty() && songWide.Data().route.turns.empty(),
                "overview drag scrubs across whole song without editing notes");
    songWideUi.tab = EditorTab::Set;
    ApplyEditorControls(songWide, songWideUi, Click(
        overviewX + overviewWidth * .5f, overviewY));
    ok &= Check(songWide.SelectedBeat() == 120 &&
                !songWide.Dirty(),
                "song overview navigates from SET without enabling note editing");

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
    chart.PutTurn(Heading::Left);
    chart.SelectBeat(8);
    chart.PutTurn(Heading::Right);
    chart.SelectBeat(6);

    EditorControls press = Click(cursor, 140);
    press.mouseHeld = true;
    press.timelineViewBeat = 6.0;
    ApplyEditorControls(chart, ui, press);
    EditorControls drag;
    drag.mouseHeld = true;
    drag.mouse = {cursor + 72.0f, 140};
    drag.timelineViewBeat = 8.0;
    ApplyEditorControls(chart, ui, drag);
    EditorControls release;
    release.mouseReleased = true;
    release.mouse = {cursor + 72.0f, 140};
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

    ApplyEditorControls(chart, ui, Click(cursor + 36.0f * 4.0f, 140));
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
    EditorControls extend = Click(cursor + 72, 140);
    extend.shiftHeld = true;
    ApplyEditorControls(chart, ui, extend);
    ok &= Check(ui.HasRange() && ui.rangeFirst == 12 && ui.rangeLast == 14,
                "Shift+click extends selection without opening another tab");
    EditorControls invert;
    invert.mirrorHorizontal = true;
    const auto flipped = ApplyEditorControls(chart, ui, invert);
    ok &= Check(flipped.changedChart &&
                chart.Data().route.turns[2].direction == Heading::Right &&
                chart.Data().route.turns[3].direction == Heading::Left,
                "I mirrors left/right components of selected absolute headings");
    EditorControls undo;
    undo.undo = true;
    ApplyEditorControls(chart, ui, undo);
    ok &= Check(chart.Data().route.turns[2].direction == Heading::Left,
                "Ctrl+Z undoes the entire reflection");
    EditorControls redo;
    redo.redo = true;
    ApplyEditorControls(chart, ui, redo);
    ok &= Check(chart.Data().route.turns[2].direction == Heading::Right,
                "Ctrl+Y redoes mirrored group");
    // Rotation buttons share the selected interval with FLIP H / FLIP V.
    const auto rotateRight = ApplyEditorControls(chart, ui, Click(520, 217));
    ok &= Check(rotateRight.changedChart && ui.HasRange() &&
                chart.Data().route.turns[2].direction == Heading::Down &&
                chart.Data().route.turns[3].direction == Heading::Up,
                "ROT +90 button rotates the selected pattern without shifting beats");
    const auto rotateLeft = ApplyEditorControls(chart, ui, Click(390, 217));
    ok &= Check(rotateLeft.changedChart &&
                chart.Data().route.turns[2].direction == Heading::Right &&
                chart.Data().route.turns[3].direction == Heading::Left,
                "ROT -90 button reverses rotation on the same selection");
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
    const auto clickedPaste = ApplyEditorControls(chart, ui, Click(342, 180));
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
    EditorControls beginDrag = Click(cursor, 140);
    beginDrag.mouseHeld = true;
    beginDrag.elapsedSeconds = 1.0f / 60.0f;
    ApplyEditorControls(longChart, scrollUi, beginDrag);
    const int firstVisibleEdge = longChart.SelectedBeat() + 16;
    EditorControls atRightEdge;
    atRightEdge.mouse = {909, 140};
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
    stopDrag.mouse = {909, 140};
    ApplyEditorControls(longChart, scrollUi, stopDrag);
    const int finishedSelection = longChart.SelectedBeat();
    for (int i = 0; i < 20; ++i) {
        EditorControls idle;
        idle.mouse = {909, 140};
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
    atLeftEdge.mouse = {51, 140};
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
    padded.PutTurn(Heading::Left);
    padded.SelectBeat(9);
    padded.PutTurn(Heading::Right);
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

    // Keyboard timeline controls may never create a direction note.
    ChartEditor keyboardOnly;
    EditorUiState keyboardUi;
    EditorControls navigationOnly;
    navigationOnly.previousBeat = true;
    navigationOnly.nextBeat = true;
    ApplyEditorControls(keyboardOnly, keyboardUi, navigationOnly);
    ok &= Check(keyboardOnly.Data().route.turns.empty(),
                "keyboard navigation cannot create notes");

    // Eight-way compass on ROUTE authors actual diagonal route notes.
    ChartEditor compassUi;
    EditorUiState diagonalUi;
    auto chooseNE = ApplyEditorControls(compassUi, diagonalUi, Click(882, 292));
    ok &= Check(chooseNE.changedChart &&
                compassUi.SelectedTurn() &&
                compassUi.SelectedTurn()->direction == Heading::UpRight &&
                SampleRouteStep(compassUi.Data().route, 0).heading == Heading::UpRight,
                "compass NE button authors absolute up-right");
    compassUi.SelectBeat(1);
    auto chooseEast = ApplyEditorControls(compassUi, diagonalUi, Click(882, 331));
    ok &= Check(chooseEast.changedChart &&
                compassUi.SelectedTurn() &&
                compassUi.SelectedTurn()->direction == Heading::Right &&
                SampleRouteStep(compassUi.Data().route, 1).heading == Heading::Right,
                "compass cardinal button supports diagonal-to-cardinal transitions");
    compassUi.SelectBeat(2);
    ApplyEditorControls(compassUi, diagonalUi, Click(766, 292));
    ok &= Check(compassUi.SelectedTurn() &&
                compassUi.SelectedTurn()->direction == Heading::UpLeft,
                "compass button authors an absolute up-left heading");
    ApplyEditorControls(compassUi, diagonalUi, Click(785, 90));
    ApplyEditorControls(compassUi, diagonalUi, Click(425, 253));
    ok &= Check(compassUi.Data().route.initialHeading == Heading::DownRight,
                "initial heading rotates only with the FIELD control");

    // A compass direction opposite to the incoming route is an ordinary
    // note, whether clicked or pressed using the game direction binding.
    ChartEditor reverseUi;
    EditorUiState reverseState;
    const auto clickBack = ApplyEditorControls(
        reverseUi, reverseState, Click(766, 330)); // L from default Right.
    ok &= Check(clickBack.changedChart && reverseUi.SelectedTurn() &&
                reverseUi.SelectedTurn()->direction == Heading::Left &&
                SampleRouteStep(reverseUi.Data().route, 0).heading == Heading::Left,
                "direction pad authors a normal 180-degree note");
    reverseUi.SelectBeat(1);
    const auto keyReverse = ApplyEditorControls(
        reverseUi, reverseState, Click(882, 331));
    ok &= Check(keyReverse.changedChart && reverseUi.SelectedTurn() &&
                reverseUi.SelectedTurn()->direction == Heading::Right &&
                SampleRouteStep(reverseUi.Data().route, 1).heading == Heading::Right,
                "right compass button authors the return trip");

    // Placement subdivision controls are independent from zoom and BPM.
    ChartEditor snapChart;
    EditorUiState snapUi;
    snapChart.SetBpm(250);
    snapChart.SelectBeat(5);
    snapChart.PutTurn(Heading::Left);
    const auto originalTime = snapChart.Data().route.turns.front().timeMs;
    const auto half = ApplyEditorControls(snapChart, snapUi, Click(865, 180));
    const auto third = ApplyEditorControls(snapChart, snapUi, Click(865, 180));
    ok &= Check(half.changedChart && third.changedChart &&
                snapChart.Data().route.snapDivision == 3 &&
                snapChart.Data().route.ticksPerBeat == 6 &&
                snapChart.Data().route.bpm == 250 &&
                snapChart.Data().route.turns.front().timeMs == originalTime &&
                snapUi.timelineZoomIndex == 3,
                "MORE grid button reaches triplets without changing BPM, notes or zoom");
    const auto coarser = ApplyEditorControls(snapChart, snapUi, Click(780, 180));
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

    // Button Undo must clear the selected range after undoing a tick
    // resolution expansion, just like Ctrl+Z. Old code had two paths.
    ChartEditor undoChart;
    EditorUiState undoUi;
    undoChart.SelectBeat(2);
    undoChart.PutTurn(Heading::Up);
    undoChart.SetSnapDivision(2);
    undoUi.rangeFirst = 4;
    undoUi.rangeLast = 6;
    const auto mouseUndo = ApplyEditorControls(undoChart, undoUi, Click(548, 180));
    ok &= Check(mouseUndo.changedChart &&
                undoChart.Data().route.ticksPerBeat == 1 &&
                !undoUi.HasRange(),
                "Undo button clears old tick-domain selection");
    undoUi.rangeFirst = 2;
    undoUi.rangeLast = 3;
    EditorControls redoInput;
    redoInput.redo = true;
    const auto keyboardRedo = ApplyEditorControls(undoChart, undoUi, redoInput);
    ok &= Check(keyboardRedo.changedChart &&
                undoChart.Data().route.ticksPerBeat == 2 &&
                !undoUi.HasRange(),
                "Ctrl+Y and button Undo share one selection cleanup path");

    if (!ok) return 1;
    std::cout << "EDITOR_UI_PASS route_controls=1 field_heading=1 zoom=1\n";
    return 0;
}
