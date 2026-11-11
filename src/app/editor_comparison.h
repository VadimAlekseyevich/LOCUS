#pragma once

#include "app/chart_editor_ui.h"
#include "app/route_preview.h"
#include "core/timed_route.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace locus::app {

struct EditorComparisonPane {
    const core::TimedRoute* route = nullptr; // Frame-local borrow, never persists.
    std::string title;
    bool editable = false;
    const RoutePreviewFrame* preview = nullptr; // Frame-only borrow.
    bool dirty = false; // Unsaved parked drafts remain visible when not focused.
};

struct EditorComparisonFrame {
    std::array<EditorComparisonPane, 4> panes{};
    // Aggregate counts for the entire active difficulty, cached by revision.
    std::array<int, 8> directionPressCounts{};
    int count = 0;
};

// Pure selection policy. Paths identify difficulties across library reorders;
// The focused draft occupies the first pane; other open panes can hold
// independent unsaved drafts that become editable when focused.
bool ToggleEditorComparison(EditorUiState& ui, const EditorSetView& set,
                            int optionIndex);
bool HasEditorComparison(const EditorUiState& ui,
                         const std::filesystem::path& file) noexcept;
// Drop missing/now-active references after file removal or library refresh.
void ReconcileEditorComparisons(EditorUiState& ui,
                                const EditorSetView& set);
// Handles a ROUTE picker click exclusively, including opening from
// COMPARE+. The caller must not pass the same click to note/transport input.
void HandleEditorComparisonControls(EditorUiState& ui,
                                    const EditorSetView& set,
                                    const EditorControls& input);

// Owns compiled routes for the focused draft and up to three other panes.
// Other drafts may be dirty in their separate EditorFileActions workspace;
// each pane uses the SAME one-song timestamp and audio stream.
class EditorComparisonCache {
public:
    EditorComparisonFrame Prepare(const core::ChartEditor& draft,
                                  const EditorUiState& ui,
                                  const EditorSetView& set,
                                  double songDurationSeconds,
                                  double songSeconds = 0.0,
                                  float lookaheadMs = 800.0f,
                                  std::function<const core::ChartEditor*(
                                      const std::filesystem::path&)> parkedDraft = {});
    void Reset() noexcept;

private:
    struct Cached {
        core::TimedRoute route;
        std::filesystem::path file;
        const core::LoadedChart* source = nullptr;
        std::uint64_t draftRevision = 0;
        std::uintmax_t fileSize = 0;
        std::filesystem::file_time_type modified{};
        double bpm = 0.0, offsetSeconds = 0.0, duration = 0.0;
        std::vector<core::TempoMarker> tempoMap;
        bool ready = false;
    };
    Cached active_;
    std::uint64_t activeRevision_ = ~std::uint64_t{0};
    std::array<int, 8> activeDirectionPressCounts_{};
    std::array<Cached, 3> reference_{};
    // Reused for every render frame; all panes use the same songSeconds.
    std::array<RoutePreviewFrame, 4> previewFrames_{};
};

} // namespace locus::app
