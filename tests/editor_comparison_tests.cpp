#include "app/editor_comparison.h"
#include "core/chart_editor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using namespace locus::app;
using namespace locus::core;

namespace {
bool Check(bool yes, const char* message) {
    if (!yes) std::cerr << "EDITOR_COMPARE_FAIL " << message << '\n';
    return yes;
}
bool Near(double a, double b) {
    return std::abs(a - b) < 1e-7;
}
LoadedChart Reference(const char* title, Heading turn, int timeMs) {
    LoadedChart loaded;
    loaded.title = title;
    loaded.route.bpm = 120.0;
    loaded.route.ticksPerBeat = 1;
    loaded.route.firstBeat = 0;
    loaded.route.lengthBeats = 1; // Unprepared library JSON, not yet decoded.
    loaded.route.start = {0, 0}; // Relative-position expectations below.
    loaded.route.turns = {{timeMs / 500, turn, timeMs}};
    return loaded;
}
} // namespace

int main() {
    bool ok = true;
    ChartEditor editor;
    ok &= Check(editor.SetSongDuration(10.0), "attach active song duration");
    EditorUiState ui;
    EditorSetView view;
    view.managed = true;
    view.current = 0;
    LoadedChart original = Reference("CURRENT SAVED", Heading::Up, 500);
    LoadedChart first = Reference("EASY", Heading::Up, 500);
    LoadedChart second = Reference("NORMAL", Heading::Down, 750);
    LoadedChart third = Reference("HARD", Heading::Left, 1000);
    LoadedChart fourth = Reference("EXTRA", Heading::UpRight, 1250);
    view.comparisonOptions = {
        {"/songs/set/current.json", "CURRENT SAVED", &original},
        {"/songs/set/easy.json", "EASY", &first},
        {"/songs/set/normal.json", "NORMAL", &second},
        {"/songs/set/hard.json", "HARD", &third},
        {"/songs/set/extra.json", "EXTRA", &fourth}
    };

    // Visual order is independent of edit focus. Selecting a different
    // difficulty cannot move it into the large first pane.
    EditorUiState stable;
    stable.comparisonFiles = {
        view.comparisonOptions[1].file,
        view.comparisonOptions[2].file,
        view.comparisonOptions[3].file
    };
    std::filesystem::path activeFile = view.comparisonOptions[0].file;
    const std::vector<std::filesystem::path> initialOrder = {
        view.comparisonOptions[0].file, view.comparisonOptions[1].file,
        view.comparisonOptions[2].file, view.comparisonOptions[3].file
    };
    const auto visualOrder = [](const EditorUiState& state,
                                const std::filesystem::path& active) {
        auto files = state.comparisonFiles;
        const auto slot = std::clamp(state.comparisonFocusedSlot, 0,
                                    static_cast<int>(files.size()));
        files.insert(files.begin() + slot, active);
        return files;
    };
    for (int target : {2, 1, 3, 0}) {
        EditorComparisonFocusOrder plan;
        const auto file = view.comparisonOptions[
            static_cast<std::size_t>(target)].file;
        ok &= Check(PlanEditorComparisonFocus(stable, activeFile, file, plan),
                    "visible difficulty can take focus");
        stable.comparisonFiles = std::move(plan.references);
        stable.comparisonFocusedSlot = plan.focusedSlot;
        activeFile = file;
        ok &= Check(visualOrder(stable, activeFile) == initialOrder &&
                    stable.comparisonFocusedSlot == target,
                    "repeated focus changes keep all four physical pane slots");
        for (int slot = 0; slot < 4; ++slot) {
            const int ref = ComparisonReferenceIndexForSlot(
                slot, stable.comparisonFocusedSlot);
            ok &= Check(ref == -1
                        ? slot == stable.comparisonFocusedSlot
                        : ComparisonDisplaySlotForReference(
                              ref, stable.comparisonFocusedSlot) == slot,
                        "focus hit-test mapping follows stable pane order");
        }
    }
    EditorComparisonFocusOrder rejected;
    stable.comparisonFocusedSlot = 1;
    activeFile = view.comparisonOptions[1].file;
    stable.comparisonFiles = {view.comparisonOptions[0].file,
                               view.comparisonOptions[2].file,
                               view.comparisonOptions[3].file};
    ok &= Check(!PlanEditorComparisonFocus(stable, activeFile,
                     view.comparisonOptions[4].file, rejected) &&
                visualOrder(stable, activeFile) == initialOrder,
                "fifth chart cannot change or shuffle four occupied slots");

    EditorUiState renderUi;
    renderUi.comparisonFiles = {
        view.comparisonOptions[0].file, view.comparisonOptions[1].file,
        view.comparisonOptions[3].file};
    renderUi.comparisonFocusedSlot = 2;
    EditorSetView renderView = view;
    renderView.current = 2;
    ChartEditor renderDraft;
    renderDraft.Load(second);
    ok &= Check(renderDraft.SetSongDuration(10.0),
                "prepare differently focused chart for pane placement");
    EditorComparisonCache orderedCache;
    const auto ordered = orderedCache.Prepare(
        renderDraft, renderUi, renderView, 10.0, 0.75);
    ok &= Check(ordered.count == 4 &&
                ordered.panes[0].title == "CURRENT SAVED" &&
                ordered.panes[1].title == "EASY" &&
                ordered.panes[2].title == "NORMAL" &&
                ordered.panes[2].editable &&
                ordered.panes[3].title == "HARD" &&
                !ordered.panes[0].editable,
                "focused chart renders in original third slot, not the first");

    // Removing an earlier pane compacts only the gap. The focused chart
    // remains at its new relative position without moving past other panes.
    EditorSetView removedView = view;
    removedView.current = 2;
    removedView.comparisonOptions.erase(
        removedView.comparisonOptions.begin()); // delete first pane
    removedView.current = 1;
    ReconcileEditorComparisons(renderUi, removedView);
    ok &= Check(renderUi.comparisonFocusedSlot == 1 &&
                renderUi.comparisonFiles.size() == 2 &&
                renderUi.comparisonFiles[0] == view.comparisonOptions[1].file,
                "reconcile compacts removed pane before focused slot");
    EditorUiState toggled = stable;
    EditorSetView toggleView = view;
    toggleView.current = 1;
    ok &= Check(ToggleEditorComparison(toggled, toggleView, 0) &&
                toggled.comparisonFocusedSlot == 0 &&
                visualOrder(toggled, activeFile) ==
                    std::vector<std::filesystem::path>({
                        view.comparisonOptions[1].file,
                        view.comparisonOptions[2].file,
                        view.comparisonOptions[3].file}),
                "closing a pane before focus preserves order of survivors");

    // Clicking COMPARE+ must be routed to the popup BEFORE pure editor
    // actions. Its rows change comparison selection, not the draft.
    EditorUiState popup;
    EditorControls open;
    open.mouseClick = true;
    open.mouse = {665.0f, 216.0f};
    HandleEditorComparisonControls(popup, view, open);
    ok &= Check(popup.comparisonPickerOpen &&
                popup.comparisonFiles.empty(),
                "COMPARE+ click opens Song Set picker");
    EditorControls choose;
    choose.mouseClick = true;
    choose.mouse = {555.0f, 307.0f}; // second difficulty row
    HandleEditorComparisonControls(popup, view, choose);
    ok &= Check(popup.comparisonPickerOpen &&
                popup.comparisonFiles.size() == 1 &&
                popup.comparisonFiles.front() ==
                    view.comparisonOptions[1].file,
                "picker row adds a different difficulty by one click");
    HandleEditorComparisonControls(popup, view, choose);
    ok &= Check(popup.comparisonFiles.empty(),
                "picker row toggles a selected reference off");
    EditorControls dismiss;
    dismiss.mouseClick = true;
    dismiss.mouse = {748.0f, 405.0f}; // outside picker
    HandleEditorComparisonControls(popup, view, dismiss);
    ok &= Check(!popup.comparisonPickerOpen,
                "outside click dismisses picker");
    view.managed = false;
    HandleEditorComparisonControls(popup, view, open);
    ok &= Check(!popup.comparisonPickerOpen,
                "COMPARE+ remains closed without managed Song Set");
    view.managed = true;

    const auto revision = editor.RouteRevision();
    ok &= Check(!ToggleEditorComparison(ui, view, 0) &&
                ui.comparisonFiles.empty() &&
                ToggleEditorComparison(ui, view, 1) &&
                ToggleEditorComparison(ui, view, 2) &&
                ToggleEditorComparison(ui, view, 3) &&
                !ToggleEditorComparison(ui, view, 4) &&
                ui.comparisonFiles.size() == 3 &&
                editor.RouteRevision() == revision &&
                !editor.Dirty(),
                "one focused route and three optional comparison slots");

    EditorComparisonCache cache;
    const auto four = cache.Prepare(editor, ui, view, 10.0, 0.75, 1200.0f);
    ok &= Check(four.directionPressCounts == std::array<int, 8>{} &&
                four.count == 4 &&
                four.panes[0].editable &&
                four.panes[1].route && four.panes[2].route &&
                four.panes[3].route &&
                !four.panes[1].editable && !four.panes[3].editable &&
                four.panes[0].route->Ready() &&
                four.panes[0].preview && four.panes[0].preview->ready,
                "four indexed previews share one active draft and one song duration");
    for (int i = 0; i < four.count; ++i) {
        const auto& pane = four.panes[static_cast<std::size_t>(i)];
        ok &= Check(pane.preview && pane.preview->ready &&
                    Near(pane.preview->songSeconds, 0.75) &&
                    Near(pane.preview->horizonSeconds, 1.2) &&
                    Near(pane.preview->marker.x,
                         pane.route->PositionAt(0.75).x),
                    "each comparison pane shares one exact-time route snapshot");
    }

    // All windows query the SAME time, never a per-chart snapped beat.
    // At 750ms reference 1 has turned up, reference 2 just turns down.
    const auto at1 = four.panes[1].route->PositionAt(0.75);
    const auto at2 = four.panes[2].route->PositionAt(0.75);
    const auto at3 = four.panes[3].route->PositionAt(0.75);
    ok &= Check(Near(at1.x, 1.0) && Near(at1.y, -0.5) &&
                Near(at2.x, 1.5) && Near(at2.y, 0.0) &&
                Near(at3.x, 1.5) && Near(at3.y, 0.0),
                "references advance at their own note timestamps, common audio clock");
    const auto before = cache.Prepare(editor, ui, view, 10.0);
    ok &= Check(before.panes[1].route == four.panes[1].route,
                "unmodified Song Set references reuse compiled timed routes");

    editor.SelectBeat(2);
    editor.SetOutgoingHeading(Heading::Left);
    const auto changed = cache.Prepare(editor, ui, view, 10.0);
    ok &= Check(editor.Dirty() && changed.panes[0].route &&
                changed.panes[0].route->Turns().size() == 1 &&
                changed.directionPressCounts[static_cast<int>(Heading::Left)] == 1 &&
                changed.panes[1].route == four.panes[1].route,
                "live draft rebuild does not mutate or reload read-only references");
    ok &= Check(editor.Undo() &&
                cache.Prepare(editor, ui, view, 10.0)
                    .panes[0].route->Turns().empty() &&
                cache.Prepare(editor, ui, view, 10.0)
                    .directionPressCounts == std::array<int, 8>{},
                "undo invalidates both route and direction-count caches");

    first.route.turns[0] = {2, Heading::Up, 1000};
    ++view.comparisonOptions[1].fileSize;
    const auto revised = cache.Prepare(editor, ui, view, 10.0);
    ok &= Check(Near(revised.panes[1].route->Turns()[0].songSeconds, 1.0),
                "library file revision invalidates only changed reference");

    // An inactive pane can now hold an unsaved, independently undoable
    // editor. Its compiled preview must use this live draft rather than the
    // library's saved chart and must rebuild when the draft changes.
    ChartEditor parked;
    LoadedChart draftSource = first;
    parked.Load(draftSource);
    ok &= Check(parked.SetSongDuration(10.0) &&
                parked.SelectBeat(3), "prepare parked comparison draft");
    parked.PutTurn(Heading::DownLeft);
    const auto lookup = [&](const std::filesystem::path& path)
        -> const ChartEditor* {
        return path == view.comparisonOptions[1].file ? &parked : nullptr;
    };
    const auto unsaved = cache.Prepare(editor, ui, view, 10.0, 0.75,
                                      800.0f, lookup);
    ok &= Check(parked.Dirty() && !unsaved.panes[1].editable &&
                unsaved.panes[1].dirty &&
                unsaved.panes[1].route->Turns().size() == 2 &&
                Near(unsaved.panes[1].preview->songSeconds, 0.75),
                "parked dirty route appears in synchronized comparison preview");
    ok &= Check(parked.Undo() &&
                cache.Prepare(editor, ui, view, 10.0, 0.75, 800.0f, lookup)
                    .panes[1].route->Turns().size() == 1,
                "background chart undo invalidates only its own preview");

    ok &= Check(ToggleEditorComparison(ui, view, 2) &&
                ui.comparisonFiles.size() == 2 &&
                cache.Prepare(editor, ui, view, 10.0).count == 3,
                "individual reference removes without touching active chart");
    view.comparisonOptions.erase(view.comparisonOptions.begin() + 1);
    const auto missing = cache.Prepare(editor, ui, view, 10.0);
    ok &= Check(missing.count == 2,
                "deleted or missing reference is never rendered");
    view.managed = false;
    ReconcileEditorComparisons(ui, view);
    ok &= Check(ui.comparisonFiles.empty() && !ui.comparisonPickerOpen,
                "comparison selection resets when Song Set disappears");
    ok &= Check(cache.Prepare(editor, ui, view, 10.0).count == 1,
                "no Song Set means single editable chart");
    if (ok) std::cout << "EDITOR_COMPARE_PASS exact_time=1 panes=4 dirty_previews=1\n";
    return ok ? 0 : 1;
}
