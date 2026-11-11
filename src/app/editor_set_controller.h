#pragma once

#include "app/chart_editor_ui.h"
#include "app/music_picker.h"

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
    MusicPickResult (*chooseImport)();
    MusicPickResult (*chooseExport)(const std::wstring&);
    MusicPickResult (*chooseBackground)() = nullptr;
};

class EditorSetController {
public:
    EditorSetController(EditorSetContext context, EditorSetPickers pickers)
        : ctx_(context), pickers_(pickers) {}

    EditorSetView View() const;
    void HandleControls(const EditorControls& input);
    void Request(EditorSetAction action);
    bool SyncSongTiming(); // Called after chart timing undo/edits.
    bool ApplyTempoMap(const std::vector<core::TempoMarker>& markers);
    bool HasPending() const noexcept { return pending_.has_value(); }
    const EditorSetRequest* Pending() const noexcept {
        return pending_ ? &*pending_ : nullptr;
    }
    void Confirm(bool approve, bool cancel);
    void CancelPending() noexcept { pending_.reset(); }

private:
    bool Execute(const EditorSetRequest& request);
    void Notice(const std::string& message, bool failed = false);
    EditorSetContext ctx_;
    EditorSetPickers pickers_;
    std::optional<EditorSetRequest> pending_;
};

} // namespace locus::app
