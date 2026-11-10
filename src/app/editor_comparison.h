#pragma once

#include "app/chart_editor_ui.h"
#include "core/timed_route.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace locus::app {

struct EditorComparisonPane {
    const core::TimedRoute* route = nullptr; // Frame-local borrow, never persists.
    std::string title;
    bool editable = false;
};

struct EditorComparisonFrame {
    std::array<EditorComparisonPane, 4> panes{};
    int count = 0;
};

// Pure selection policy. Paths identify difficulties across library reorders;
// current unsaved draft is always the first (and only editable) pane.
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

// Owns compiled routes for one editable chart and up to three read-only
// difficulties. A playback frame supplies ONE song timestamp to all routes.
// No second audio stream, judgement queue or editable ChartEditor exists.
class EditorComparisonCache {
public:
    EditorComparisonFrame Prepare(const core::ChartEditor& draft,
                                  const EditorUiState& ui,
                                  const EditorSetView& set,
                                  double songDurationSeconds);
    void Reset() noexcept;

private:
    struct Cached {
        core::TimedRoute route;
        std::filesystem::path file;
        const core::LoadedChart* source = nullptr;
        std::uintmax_t fileSize = 0;
        std::filesystem::file_time_type modified{};
        double bpm = 0.0, offsetSeconds = 0.0, duration = 0.0;
        bool ready = false;
    };
    Cached active_;
    std::uint64_t activeRevision_ = ~std::uint64_t{0};
    std::array<Cached, 3> reference_{};
};

} // namespace locus::app
