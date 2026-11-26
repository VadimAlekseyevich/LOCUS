#pragma once

#include "app/chart_editor_ui.h"
#include "app/music_picker.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace locus::core { class ChartEditor; }
namespace locus::app {
class SongLibrary;
class EditorFileActions;
struct SongSelectUi;

// A request captures filesystem identity before the library is rescanned.
// Never persist a LibrarySong/LibraryChart pointer across a mutation.
struct EditorSetRequest {
    EditorSetAction action = EditorSetAction::None;
    std::filesystem::path folder;
    std::filesystem::path chart;
    bool discardsDraft = false;
    int previewMs = 0;
};
bool EditorSetNeedsConfirmation(const EditorSetRequest& request) noexcept;
// Parse a whole-millisecond Song Set preview position without touching the chart.
bool ParseSongPreviewMs(std::string_view input, int& value) noexcept;

struct EditorSetContext {
    SongLibrary& library;
    EditorFileActions& files;
    core::ChartEditor& editor;
    EditorUiState& ui;
    const std::wstring& chartPath;
    std::string& editorStatus;
    SongSelectUi& libraryUi;
    std::string& menuStatus;
};

struct EditorSetPickers {
    ImportPickResult (*chooseImport)();
    MusicPickResult (*chooseExport)(const std::wstring&);
    MusicPickResult (*chooseBackground)() = nullptr;
};

class EditorSetController {
public:
    EditorSetController(EditorSetContext context, EditorSetPickers pickers)
        : ctx_(context), pickers_(pickers) {}

    // Frame-only borrow; invalidated by the next FrameView/View call after
    // structural changes. Commands that need a stable snapshot use View().
    const EditorSetView& FrameView() const;
    EditorSetView View() const { return FrameView(); }
    void HandleControls(const EditorControls& input);
    void Request(EditorSetAction action);
    // The native SET panel and legacy authoring handler share one validated
    // metadata publication path. The identity prevents stale-focus writes.
    bool CommitMetadata(EditorSetField field, const std::string& value,
                        const std::string& expectedSetId);
    bool FocusDifficulty(int index);
    bool SelectDifficultyId(const std::string& id);
    void ClickDraft(double& now);
    bool EnterChart(double& now);
    bool CommitDifficultyTitle(const std::string& value, double& now);
    // Persist a chart's tempo edit; if publication fails, reverse only the
    // operation that caused it (Undo must be reversed by Redo, not Undo).
    bool SyncSongTiming(EditorHistoryOperation operation =
                            EditorHistoryOperation::None);
    bool ApplyTempoMap(const std::vector<core::TempoMarker>& markers);
    bool HasPending() const noexcept { return pending_.has_value(); }
    const EditorSetRequest* Pending() const noexcept {
        return pending_ ? &*pending_ : nullptr;
    }
    void Confirm(bool approve, bool cancel);
    void CancelPending() noexcept { pending_.reset(); }

private:
    EditorSetView BuildView() const;
    void UpdateViewSelection(EditorSetView& view) const;
    struct ViewCache {
        EditorSetView view;
        bool ready=false;
        std::uint64_t catalogRevision=0,workspaceRevision=0;
        std::wstring chartPath;
        std::string title,selectedId;
        bool hasAudio=false;
        int selected=-1,firstVisible=0;
    };
    mutable ViewCache viewCache_;
    bool Execute(const EditorSetRequest& request);
    void Notice(const std::string& message, bool failed = false);
    EditorSetContext ctx_;
    EditorSetPickers pickers_;
    std::optional<EditorSetRequest> pending_;
};

} // namespace locus::app
