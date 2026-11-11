#include "app/editor_comparison.h"
#include "app/chart_editor_layout.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace locus::app {

bool HasEditorComparison(const EditorUiState& ui,
                         const std::filesystem::path& file) noexcept {
    return !file.empty() && std::find(ui.comparisonFiles.begin(),
        ui.comparisonFiles.end(), file) != ui.comparisonFiles.end();
}

bool ToggleEditorComparison(EditorUiState& ui, const EditorSetView& set,
                            int optionIndex) {
    if (optionIndex < 0 ||
        optionIndex >= static_cast<int>(set.comparisonOptions.size()))
        return false;
    const auto& option = set.comparisonOptions[
        static_cast<std::size_t>(optionIndex)];
    if (!option.chart || option.file.empty() ||
        optionIndex == set.current)
        return false; // Never duplicate the editable chart.
    const auto selected = std::find(ui.comparisonFiles.begin(),
        ui.comparisonFiles.end(), option.file);
    if (selected != ui.comparisonFiles.end()) {
        const int removedIndex = static_cast<int>(
            selected - ui.comparisonFiles.begin());
        if (removedIndex < ui.comparisonFocusedSlot)
            --ui.comparisonFocusedSlot;
        ui.comparisonFiles.erase(selected);
        return true;
    }
    if (ui.comparisonFiles.size() >= 3) return false;
    ui.comparisonFiles.push_back(option.file);
    return true;
}

void ReconcileEditorComparisons(EditorUiState& ui,
                                const EditorSetView& set) {
    if (!set.managed) {
        ui.comparisonFiles.clear();
        ui.comparisonFocusedSlot = 0;
        ui.comparisonPickerOpen = false;
        ui.comparisonPickerFirst = 0;
        return;
    }
    std::vector<std::filesystem::path> valid;
    const int oldFocus = std::clamp(ui.comparisonFocusedSlot, 0,
                                   static_cast<int>(ui.comparisonFiles.size()));
    int focusedSlot = 0;
    for (std::size_t i = 0; i < ui.comparisonFiles.size(); ++i) {
        const auto& file = ui.comparisonFiles[i];
        const auto found = std::find_if(set.comparisonOptions.begin(),
            set.comparisonOptions.end(),
            [&](const EditorComparisonOption& option) {
                return option.file == file && option.chart;
            });
        if (found == set.comparisonOptions.end() ||
            found - set.comparisonOptions.begin() == set.current ||
            std::find(valid.begin(), valid.end(), file) != valid.end())
            continue;
        if (valid.size() == 3) break;
        if (static_cast<int>(i) < oldFocus) ++focusedSlot;
        valid.push_back(file);
    }
    ui.comparisonFiles = std::move(valid);
    ui.comparisonFocusedSlot = focusedSlot;
    ui.comparisonPickerFirst = std::clamp(ui.comparisonPickerFirst, 0,
        std::max(0, static_cast<int>(set.comparisonOptions.size()) - 5));
}

void HandleEditorComparisonControls(EditorUiState& ui,
                                    const EditorSetView& set,
                                    const EditorControls& input) {
    ReconcileEditorComparisons(ui, set);
    if (input.escape && ui.comparisonPickerOpen) {
        ui.comparisonPickerOpen = false;
        return;
    }
    if (!input.mouseClick) return;
    const Vector2 mouse = input.mouse;
    if (detail::Hit(detail::kCompareToggle, mouse)) {
        const bool alternatives = set.managed &&
            std::any_of(set.comparisonOptions.begin(),
                        set.comparisonOptions.end(),
                        [&](const EditorComparisonOption& option) {
                            const auto index = &option -
                                set.comparisonOptions.data();
                            return option.chart && index != set.current;
                        });
        // Always respond to the click: opening a picker with no managed
        // chart is misleading, so leave it closed until a set is selected.
        ui.comparisonPickerOpen = alternatives && !ui.comparisonPickerOpen;
        ui.comparisonPickerFirst = 0;
        if (!alternatives) {
            ui.feedback = "OPEN A SONG SET WITH MULTIPLE CHARTS";
            ui.feedbackError = true;
        }
        return;
    }
    if (!ui.comparisonPickerOpen) return;
    const int count = static_cast<int>(set.comparisonOptions.size());
    if (detail::Hit(detail::kComparePickerPrev, mouse)) {
        ui.comparisonPickerFirst = std::max(0, ui.comparisonPickerFirst - 5);
        return;
    }
    if (detail::Hit(detail::kComparePickerNext, mouse)) {
        ui.comparisonPickerFirst = std::min(std::max(0, count - 5),
                                           ui.comparisonPickerFirst + 5);
        return;
    }
    for (int row = 0; row < 5; ++row) {
        const int index = ui.comparisonPickerFirst + row;
        if (index >= count) break;
        if (!detail::Hit(detail::ComparePickerRow(row), mouse)) continue;
        if (!ToggleEditorComparison(ui, set, index)) {
            ui.feedback = "MAX FOUR ROUTES / ACTIVE CHART IS ALREADY SHOWN";
            ui.feedbackError = true;
        }
        return;
    }
    if (!detail::Hit(detail::kComparePopup, mouse))
        ui.comparisonPickerOpen = false;
}

void EditorComparisonCache::Reset() noexcept {
    active_ = Cached{};
    activeRevision_ = ~std::uint64_t{0};
    activeDirectionPressCounts_ = {};
    reference_ = {};
    previewFrames_ = {};
}

EditorComparisonFrame EditorComparisonCache::Prepare(
    const core::ChartEditor& draft, const EditorUiState& ui,
    const EditorSetView& set, double songDurationSeconds,
    double songSeconds, float lookaheadMs,
    std::function<const core::ChartEditor*(
        const std::filesystem::path&)> parkedDraft) {
    EditorComparisonFrame frame;
    // The editor can reset its history to revision 0 on Open or New. Use
    // monotonic RouteRevision instead of revision or a borrowed Chart* key.
    if (activeRevision_ != draft.RouteRevision()) {
        activeRevision_ = draft.RouteRevision();
        active_.ready = active_.route.Compile(
            draft.Data().route, draft.Data().offsetSeconds);
        activeDirectionPressCounts_ =
            detail::DirectionPressCounts(draft.Data().route);
    }
    frame.directionPressCounts = activeDirectionPressCounts_;
    // Build the visual slots from file identities, not from which chart is
    // currently being edited. A click only moves the EDIT marker, never a pane.
    std::vector<const EditorComparisonOption*> visible;
    int focusedSlot = 0;
    if (set.managed && std::isfinite(songDurationSeconds) &&
        songDurationSeconds > 0.0) {
        const int oldFocus = std::clamp(ui.comparisonFocusedSlot, 0,
                                       static_cast<int>(ui.comparisonFiles.size()));
        for (std::size_t i = 0; i < ui.comparisonFiles.size(); ++i) {
            const auto& file = ui.comparisonFiles[i];
            const auto found = std::find_if(set.comparisonOptions.begin(),
                set.comparisonOptions.end(),
                [&](const EditorComparisonOption& option) {
                    return option.file == file && option.chart;
                });
            if (found == set.comparisonOptions.end() ||
                static_cast<int>(found - set.comparisonOptions.begin()) ==
                    set.current ||
                visible.size() >= 3) continue;
            if (static_cast<int>(i) < oldFocus) ++focusedSlot;
            visible.push_back(&*found);
        }
    }
    frame.count = 1 + static_cast<int>(visible.size());
    auto& activePreview = previewFrames_[static_cast<std::size_t>(focusedSlot)];
    PrepareRoutePreview(active_.route, songSeconds, lookaheadMs, activePreview);
    frame.panes[static_cast<std::size_t>(focusedSlot)] = {
        active_.ready ? &active_.route : nullptr,
        draft.Data().title, true, &activePreview, draft.Dirty()};

    for (int i = 0; i < static_cast<int>(visible.size()); ++i) {
        const auto& option = *visible[static_cast<std::size_t>(i)];
        const auto& file = option.file;
        const core::ChartEditor* parked = parkedDraft
            ? parkedDraft(file) : nullptr;
        const core::LoadedChart* source = parked
            ? &parked->Data() : option.chart;
        const std::uint64_t revision = parked ? parked->RouteRevision() : 0;
        auto& cached = reference_[static_cast<std::size_t>(i)];
        const bool changed = !cached.ready || cached.file != file ||
            cached.source != source || cached.draftRevision != revision ||
            cached.fileSize != option.fileSize ||
            cached.modified != option.modified ||
            cached.bpm != source->route.bpm ||
            cached.tempoMap != source->route.tempoMap ||
            cached.offsetSeconds != source->offsetSeconds ||
            cached.duration != songDurationSeconds;
        if (changed) {
            cached.ready = false;
            cached.file = file;
            cached.source = source;
            cached.draftRevision = revision;
            cached.fileSize = option.fileSize;
            cached.modified = option.modified;
            cached.bpm = source->route.bpm;
            cached.tempoMap = source->route.tempoMap;
            cached.offsetSeconds = source->offsetSeconds;
            cached.duration = songDurationSeconds;
            // Compile only a copy of library data or parked editor data.
            core::LoadedChart prepared = *source;
            std::string error;
            if (core::PrepareChartForSong(prepared, songDurationSeconds, error))
                cached.ready = cached.route.Compile(
                    prepared.route, prepared.offsetSeconds);
        }
        const int paneSlot = ComparisonDisplaySlotForReference(i, focusedSlot);
        auto& paneFrame = previewFrames_[static_cast<std::size_t>(paneSlot)];
        if (cached.ready)
            PrepareRoutePreview(cached.route, songSeconds, lookaheadMs, paneFrame);
        frame.panes[static_cast<std::size_t>(paneSlot)] = {
            cached.ready ? &cached.route : nullptr, source->title, false,
            cached.ready ? &paneFrame : nullptr, parked && parked->Dirty()};
    }
    return frame;
}

} // namespace locus::app
