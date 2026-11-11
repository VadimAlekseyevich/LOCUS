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

    // Exactly three visual slots: clicking a pane changes the EDIT flag,
    // never the chart ordering or the fixed dimensions of the panes.
    EditorUiState stable;
    stable.comparisonFiles = {
        view.comparisonOptions[1].file, view.comparisonOptions[2].file
    };
    std::filesystem::path activeFile = view.comparisonOptions[0].file;
    const std::vector<std::filesystem::path> initialOrder = {
        view.comparisonOptions[0].file,
        view.comparisonOptions[1].file,
        view.comparisonOptions[2].file
    };
    const auto visualOrder = [](const EditorUiState& state,
                                const std::filesystem::path& active) {
        auto files = state.comparisonFiles;
        const int slot = std::clamp(
            state.comparisonFocusedSlot, 0, static_cast<int>(files.size()));
        files.insert(files.begin() + slot, active);
        return files;
    };
    for (int target : {2, 1, 0, 2}) {
        EditorComparisonFocusOrder plan;
        const auto file = view.comparisonOptions[
            static_cast<std::size_t>(target)].file;
        ok &= Check(PlanEditorComparisonFocus(stable, activeFile, file, plan),
                    "switching to an already visible difficulty succeeds");
        stable.comparisonFiles = std::move(plan.references);
        stable.comparisonFocusedSlot = plan.focusedSlot;
        activeFile = file;
        ok &= Check(visualOrder(stable, activeFile) == initialOrder &&
                    stable.comparisonFocusedSlot == target,
                    "multiple focus changes preserve all three visual slots");
        for (int slot = 0; slot < 3; ++slot) {
            const int reference = ComparisonReferenceIndexForSlot(
                slot, stable.comparisonFocusedSlot);
            ok &= Check(reference == -1
                ? slot == stable.comparisonFocusedSlot
                : ComparisonDisplaySlotForReference(
                    reference, stable.comparisonFocusedSlot) == slot,
                "click hit-testing matches physical preview slot");
        }
    }
    EditorComparisonFocusOrder rejected;
    ok &= Check(!PlanEditorComparisonFocus(stable, activeFile,
                     view.comparisonOptions[3].file, rejected) &&
                visualOrder(stable, activeFile) == initialOrder,
                "a fourth chart cannot replace or move a three-chart workspace");

    EditorUiState expanded;
    expanded.comparisonFiles = {view.comparisonOptions[1].file};
    EditorComparisonFocusOrder appended;
    ok &= Check(PlanEditorComparisonFocus(
                    expanded, view.comparisonOptions[0].file,
                    view.comparisonOptions[2].file, appended) &&
                appended.focusedSlot == 2 &&
                appended.references.size() == 2 &&
                appended.references[0] == view.comparisonOptions[0].file &&
                appended.references[1] == view.comparisonOptions[1].file,
                "SET appends a third chart instead of shuffling existing ones");

    EditorUiState renderUi;
    renderUi.comparisonFiles = {
        view.comparisonOptions[0].file, view.comparisonOptions[2].file};
    renderUi.comparisonFocusedSlot = 1;
    EditorSetView renderView = view;
    renderView.current = 1;
    ChartEditor renderDraft;
    renderDraft.Load(first);
    ok &= Check(renderDraft.SetSongDuration(10.0),
                "prepare focused second-slot chart");
    EditorComparisonCache orderedCache;
    const auto ordered = orderedCache.Prepare(
        renderDraft, renderUi, renderView, 10.0, 0.75);
    ok &= Check(ordered.count == 3 &&
                ordered.panes[0].title == "CURRENT SAVED" &&
                ordered.panes[1].title == "EASY" &&
                ordered.panes[1].editable &&
                ordered.panes[2].title == "NORMAL" &&
                !ordered.panes[0].editable,
                "focused chart renders in the original second physical pane");

    // Removing a reference to the left compacts the gap but leaves other
    // charts in their relative order.
    renderView.comparisonOptions.erase(
        renderView.comparisonOptions.begin());
    renderView.current = 0;
    ReconcileEditorComparisons(renderUi, renderView);
    ok &= Check(renderUi.comparisonFocusedSlot == 0 &&
                renderUi.comparisonFiles.size() == 1 &&
                renderUi.comparisonFiles[0] == view.comparisonOptions[2].file,
                "removed pane to the left updates focus slot correctly");

    EditorUiState toggled;
    toggled.comparisonFiles = {view.comparisonOptions[0].file,
                                view.comparisonOptions[2].file};
    toggled.comparisonFocusedSlot = 1;
    EditorSetView toggleView = view;
    toggleView.current = 1;
    ok &= Check(ToggleEditorComparison(toggled, toggleView, 0) &&
                toggled.comparisonFocusedSlot == 0 &&
                toggled.comparisonFiles.size() == 1 &&
                toggled.comparisonFiles.front() ==
                    view.comparisonOptions[2].file,
                "closing a reference before focus preserves remaining order");

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
                ToggleEditorComparison(ui, view, 1) &&
                ToggleEditorComparison(ui, view, 2) &&
                !ToggleEditorComparison(ui, view, 3) &&
                ui.comparisonFiles.size() == 2 &&
                editor.RouteRevision() == revision && !editor.Dirty(),
                "active chart plus two optional references is the hard limit");

    EditorComparisonCache cache;
    const auto three = cache.Prepare(editor, ui, view, 10.0, 0.75, 1200.0f);
    ok &= Check(three.directionPressCounts == std::array<int, 8>{} &&
                three.count == 3 &&
                three.panes[0].editable &&
                three.panes[1].route && three.panes[2].route &&
                !three.panes[1].editable && !three.panes[2].editable &&
                three.panes[0].route->Ready() &&
                three.panes[0].preview && three.panes[0].preview->ready,
                "three synchronized previews share one active draft");
    for (int i = 0; i < three.count; ++i) {
        const auto& pane = three.panes[static_cast<std::size_t>(i)];
        ok &= Check(pane.preview && pane.preview->ready &&
                    Near(pane.preview->songSeconds, 0.75) &&
                    Near(pane.preview->horizonSeconds, 1.2) &&
                    Near(pane.preview->marker.x,
                         pane.route->PositionAt(0.75).x),
                    "all three previews use a single exact-time song cursor");
    }

    const auto at1 = three.panes[1].route->PositionAt(0.75);
    const auto at2 = three.panes[2].route->PositionAt(0.75);
    ok &= Check(Near(at1.x, 1.0) && Near(at1.y, -0.5) &&
                Near(at2.x, 1.5) && Near(at2.y, 0.0),
                "reference charts use their own absolute note timestamps");
    const auto before = cache.Prepare(editor, ui, view, 10.0);
    ok &= Check(before.panes[1].route == three.panes[1].route,
                "unchanged Song Set references reuse compiled routes");

    editor.SelectBeat(2);
    editor.SetOutgoingHeading(Heading::Left);
    const auto changed = cache.Prepare(editor, ui, view, 10.0);
    ok &= Check(editor.Dirty() && changed.panes[0].route &&
                changed.panes[0].route->Turns().size() == 1 &&
                changed.directionPressCounts[static_cast<int>(Heading::Left)] == 1 &&
                changed.panes[1].route == three.panes[1].route,
                "active draft edits do not mutate other charts' routes");
    ok &= Check(editor.Undo() &&
                cache.Prepare(editor, ui, view, 10.0)
                    .panes[0].route->Turns().empty(),
                "undo invalidates the focused compiled route");

    first.route.turns[0] = {2, Heading::Up, 1000};
    ++view.comparisonOptions[1].fileSize;
    const auto revised = cache.Prepare(editor, ui, view, 10.0);
    ok &= Check(Near(revised.panes[1].route->Turns()[0].songSeconds, 1.0),
                "library change invalidates only the modified reference");

    ChartEditor parked;
    parked.Load(first);
    ok &= Check(parked.SetSongDuration(10.0) &&
                parked.SelectBeat(3), "prepare nonfocused unsaved draft");
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
                "unsaved background chart remains visible and synchronized");
    ok &= Check(parked.Undo() &&
                cache.Prepare(editor, ui, view, 10.0, 0.75, 800.0f, lookup)
                    .panes[1].route->Turns().size() == 1,
                "background draft Undo invalidates its own preview cache");

    ok &= Check(ToggleEditorComparison(ui, view, 2) &&
                ui.comparisonFiles.size() == 1 &&
                cache.Prepare(editor, ui, view, 10.0).count == 2,
                "closing a comparison removes only its own preview");
    view.comparisonOptions.erase(view.comparisonOptions.begin() + 1);
    const auto missing = cache.Prepare(editor, ui, view, 10.0);
    ok &= Check(missing.count == 1,
                "deleted reference is omitted without moving active draft");
    view.managed = false;
    ReconcileEditorComparisons(ui, view);
    ok &= Check(ui.comparisonFiles.empty() &&
                ui.comparisonFocusedSlot == 0 && !ui.comparisonPickerOpen &&
                cache.Prepare(editor, ui, view, 10.0).count == 1,
                "unmanaged set drops inactive comparisons safely");

    if (ok) std::cout << "EDITOR_COMPARE_PASS exact_time=1 panes=3 dirty_previews=1\n";
    return ok ? 0 : 1;
}
