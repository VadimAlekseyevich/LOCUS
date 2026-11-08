#pragma once

#include "app/chart_editor_ui.h"
#include "app/music_picker.h"

#include <filesystem>
#include <optional>
#include <string>

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
};
bool EditorSetNeedsConfirmation(const EditorSetRequest& request) noexcept;

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
};

class EditorSetController {
public:
    EditorSetController(EditorSetContext context, EditorSetPickers pickers)
        : ctx_(context), pickers_(pickers) {}

    EditorSetView View() const;
    void HandleClick(Vector2 mouse, bool clicked, float wheel);
    void Request(EditorSetAction action);
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
