#include "app/editor_comparison.h"
#include "app/chart_editor_layout.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace locus::app {

bool PlanEditorComparisonFocus(const EditorUiState& ui,
                               const std::filesystem::path& active,
                               const std::filesystem::path& target,
                               EditorComparisonFocusOrder& result) {
    if (active.empty() || target.empty() || active == target ||
        ui.comparisonFiles.size() > 2 ||
        std::find(ui.comparisonFiles.begin(), ui.comparisonFiles.end(),
                  active) != ui.comparisonFiles.end()) return false;

    EditorComparisonFocusOrder candidate;
    candidate.references = ui.comparisonFiles;
    const int currentSlot = std::clamp(ui.comparisonFocusedSlot, 0,
                          static_cast<int>(candidate.references.size()));
    const auto targetRef = std::find(candidate.references.begin(),
                                     candidate.references.end(), target);
    if (targetRef == candidate.references.end()) {
        if (candidate.references.size() >= 2) return false;
        candidate.focusedSlot = static_cast<int>(candidate.references.size()) + 1;
    } else {
        candidate.focusedSlot = ComparisonDisplaySlotForReference(
            static_cast<int>(targetRef - candidate.references.begin()),
            currentSlot);
    }
    const bool alreadyVisible = targetRef != candidate.references.end();
    candidate.references.insert(candidate.references.begin() + currentSlot,
                                active);
    if (alreadyVisible) {
        const auto chosen = std::find(candidate.references.begin(),
                                      candidate.references.end(), target);
        if (chosen == candidate.references.end()) return false;
        candidate.references.erase(chosen);
    }
    // An unseen target becomes the focused final pane after the existing
    // visual slots, provided the three-chart limit is not exceeded.
    result = std::move(candidate);
    return true;
}

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
    if (!option.available || option.file.empty() ||
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
    if (ui.comparisonFiles.size() >= 2) return false;
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
                return option.file == file && option.available;
            });
        if (found == set.comparisonOptions.end() ||
            found - set.comparisonOptions.begin() == set.current ||
            std::find(valid.begin(), valid.end(), file) != valid.end())
            continue;
        if (valid.size() == 2) break;
        if (static_cast<int>(i) < oldFocus) ++focusedSlot;
        valid.push_back(file);
    }
    ui.comparisonFiles = std::move(valid);
    ui.comparisonFocusedSlot = focusedSlot;
    ui.comparisonPickerFirst = std::clamp(ui.comparisonPickerFirst, 0,
        std::max(0, static_cast<int>(set.comparisonOptions.size()) - 5));
}

int EditorComparisonIndex(const EditorSetView& set,
                          const std::filesystem::path& file) noexcept {
    if (file.empty()) return -1;
    const auto found = std::find_if(set.comparisonOptions.begin(),
        set.comparisonOptions.end(), [&](const EditorComparisonOption& option) {
            return option.file == file && option.available;
        });
    return found == set.comparisonOptions.end() ? -1 :
        static_cast<int>(found - set.comparisonOptions.begin());
}

bool CanOpenEditorComparison(const EditorSetView& set) noexcept {
    if (!set.managed) return false;
    for (std::size_t i=0; i<set.comparisonOptions.size(); ++i)
        if (static_cast<int>(i) != set.current && set.comparisonOptions[i].available &&
            !set.comparisonOptions[i].file.empty()) return true;
    return false;
}

void HandleEditorComparisonControls(EditorUiState& ui,
                                    const EditorSetView& set,
                                    const EditorControls& input) {
    ReconcileEditorComparisons(ui, set);
    if ((input.escape || input.compareAction == ComparePickerAction::Dismiss) &&
        ui.comparisonPickerOpen) {
        ui.comparisonPickerOpen = false;
        return;
    }
    if (input.compareAction == ComparePickerAction::Toggle) {
        const bool alternatives = CanOpenEditorComparison(set);
        ui.comparisonPickerOpen = alternatives && !ui.comparisonPickerOpen;
        ui.comparisonPickerFirst = 0;
        if (!alternatives) {
            ui.feedback = "OPEN A SONG SET WITH MULTIPLE CHARTS";
            ui.feedbackError = true;
        }
        return;
    }
    if (input.compareAction == ComparePickerAction::Select) {
        if (!ui.comparisonPickerOpen) return;
        const int index = EditorComparisonIndex(set, input.compareFile);
        if (!ToggleEditorComparison(ui, set, index)) {
            ui.feedback = "MAX THREE ROUTES / ACTIVE CHART IS ALREADY SHOWN";
            ui.feedbackError = true;
        }
        return;
    }
    if (input.nativeChrome) return; // No legacy picker hitboxes in native UI.
    const Vector2 mouse = input.mouse;
    if (ui.comparisonPickerOpen && input.mouseWheel != 0.0f &&
        detail::Hit(detail::ComparePopupBounds(ui), mouse)) {
        ui.comparisonPickerFirst = std::clamp(ui.comparisonPickerFirst +
            (input.mouseWheel > 0.0f ? -1 : 1), 0,
            std::max(0, static_cast<int>(set.comparisonOptions.size()) - 5));
        return;
    }
    if (!input.mouseClick) return;
    if (detail::Hit(detail::CompareToggleBounds(ui), mouse)) {
        const bool alternatives = CanOpenEditorComparison(set);
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
    for (int row = 0; row < 5; ++row) {
        const int index = ui.comparisonPickerFirst + row;
        if (index >= count) break;
        if (!detail::Hit(detail::ComparePickerRow(ui, row), mouse)) continue;
        if (!ToggleEditorComparison(ui, set, index)) {
            ui.feedback = "MAX THREE ROUTES / ACTIVE CHART IS ALREADY SHOWN";
            ui.feedbackError = true;
        }
        return;
    }
    if (!detail::Hit(detail::ComparePopupBounds(ui), mouse))
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
                    return option.file == file && option.available;
                });
            if (found == set.comparisonOptions.end() ||
                static_cast<int>(found - set.comparisonOptions.begin()) ==
                    set.current ||
                visible.size() >= 2) continue;
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
        const std::uint64_t revision = parked ? parked->RouteRevision() : 0;
        auto& cached = reference_[static_cast<std::size_t>(i)];
        const bool changed = !cached.attempted || cached.file != file ||
            cached.parked != parked || cached.draftRevision != revision ||
            cached.fileSize != option.fileSize ||
            cached.modified != option.modified ||
            cached.contentFingerprint != option.contentFingerprint ||
            cached.gameplayRevision != option.gameplayRevision ||
            cached.duration != songDurationSeconds;
        if (changed) {
            cached.ready = false;
            cached.attempted = true;
            cached.file = file;
            cached.parked = parked;
            cached.draftRevision = revision;
            cached.fileSize = option.fileSize;
            cached.modified = option.modified;
            cached.contentFingerprint = option.contentFingerprint;
            cached.gameplayRevision = option.gameplayRevision;
            cached.duration = songDurationSeconds;
            core::LoadedChart prepared;
            std::string error;
            // Unsaved documents win over disk. A saved reference is loaded
            // only when its identity/revision changes, never on render ticks.
            const bool loaded = parked
                ? (prepared = parked->Data(),true)
                : set.loadComparison && set.loadComparison(file,prepared,error);
            if (loaded &&
                core::PrepareChartForSong(prepared,songDurationSeconds,error))
                cached.ready = cached.route.Compile(
                    prepared.route,prepared.offsetSeconds);
        }
        const int paneSlot = ComparisonDisplaySlotForReference(i, focusedSlot);
        auto& paneFrame = previewFrames_[static_cast<std::size_t>(paneSlot)];
        if (cached.ready)
            PrepareRoutePreview(cached.route, songSeconds, lookaheadMs, paneFrame);
        frame.panes[static_cast<std::size_t>(paneSlot)] = {
            cached.ready ? &cached.route : nullptr,
            parked ? parked->Data().title : option.title, false,
            cached.ready ? &paneFrame : nullptr, parked && parked->Dirty()};
    }
    // A closed pane releases compiled route storage and failed attempts.
    for (std::size_t i=visible.size();i<reference_.size();++i)
        if (reference_[i].attempted) reference_[i] = Cached{};
    return frame;
}

} // namespace locus::app
