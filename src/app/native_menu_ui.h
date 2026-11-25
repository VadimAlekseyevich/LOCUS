#pragma once

#include "app/rml_raylib_renderer.h"
#include "app/editor_layout_snapshot.h"
#include "app/rml_native_files.h"
#include "app/menu_controller.h"
#include "app/native_play_ui.h"
#include "app/native_settings_ui.h"
#include "app/native_editor_ui.h"
#include "app/rml_confirmation.h"
#include "app/native_results_ui.h"
#include "app/native_skin_ui.h"
#include <RmlUi/Core/EventListener.h>

#include <string>
#include <vector>

namespace Rml {
class Context;
class ElementDocument;
class Element;
}

namespace locus::app {

// Shared native UI host: contexts, document lifetime, frame routing and
// custom-canvas geometry. Screen presenters own their DOM and commands.
class NativeMenuUi final : private Rml::EventListener {
public:
    NativeMenuUi() = default;
    ~NativeMenuUi();

    bool Initialize();
    void Shutdown();
    bool Available() const noexcept { return document_ != nullptr; }
    bool HomeAvailable() const noexcept { return homeDocument_ != nullptr; }
    bool SettingsAvailable() const noexcept { return settingsDocument_ != nullptr; }
    bool EditorAvailable() const noexcept { return editorDocument_ != nullptr; }
    bool ResultsAvailable() const noexcept { return resultsDocument_ != nullptr; }
    bool SkinAvailable() const noexcept { return skinDocument_ != nullptr; }
    bool ShowingHome() const noexcept;
    bool ShowingPlay() const noexcept;
    bool ShowingSettings() const noexcept;
    bool ShowingEditor() const noexcept;
    bool EditorTextFocused() const noexcept;
    bool ShowingResults() const noexcept;
    bool ShowingSkin() const noexcept;
    Rectangle SkinPreviewBounds() const noexcept;
    Rectangle SkinColorWheelBounds() const noexcept;
    Rectangle SkinColorWheelClipBounds() const noexcept;
    Rectangle HomeWordmarkBounds() const noexcept;
    // Share RmlUi input and rendering for HOME and PLAY; both retain fallback.
    void UpdateHome(FrameInput& input, const std::string& status);
    void UpdateEditor(EditorUiState& ui, FrameInput& input,
                      const std::string& title, bool dirty,
                      double rate, bool playing, long long songTimeMs,
                      const core::ChartEditor& draft,
                      const GameSettings& settings,
                      const EditorSetView& setView);
    void SyncEditor(EditorUiState& ui, const std::string& title,
                    bool dirty, double rate, bool playing, long long songTimeMs,
                    const core::ChartEditor& draft,
                    const GameSettings& settings,
                    const EditorSetView& setView);
    void UpdateSettings(GameSettings& settings, SettingsUi& ui,
                        FrameInput& input, MenuEvents& events,
                        bool exclusiveKeyboardInput);

    MenuEvents UpdateSkin(Skin& draft, SkinUi& ui, FrameInput& input);
    MenuEvents UpdateResults(FrameInput& input, const RunResult* selected,
        const core::GradeCounts& grades, int total, const std::string& chartTitle,
        bool historical, const SongSelectUi& history);

    // Poll only while SongSelect is visible. Other screens keep raylib input.
    MenuEvents Update(const SongLibrary& library, SongSelectUi& ui,
                      FrameInput& input);
    void Hide();
    void Render();
    bool ConfirmationAvailable() const noexcept { return confirmationDocument_ != nullptr; }
    bool ShowingConfirmation() const noexcept { return confirmation_.Visible(); }
    void SyncConfirmation(const ConfirmationView& view);
    ConfirmationChoice UpdateConfirmation(const ConfirmationView& view);
    void RenderConfirmation();

private:
    enum class Page { None, Home, Play, Settings, Editor, Results, Skin };
    void Activate(Page page);
    bool RefreshLayoutMetrics();
    void PumpInput(bool dispatchKeyboard = true, bool forceLayout = false);
    void DispatchInput(bool dispatchKeyboard, bool dispatchButtons, bool dispatchWheel);
    void ReadEditorLayout(EditorUiState& ui) const noexcept;
    void ProcessEvent(Rml::Event& event) override;
    RmlNativeFiles files_;
    RmlRaylibSystem system_;
    RmlRaylibRenderer renderer_;
    Rml::Context* context_ = nullptr;
    Rml::Context* confirmationContext_ = nullptr;
    Rml::ElementDocument* confirmationDocument_ = nullptr;
    RmlConfirmation confirmation_;
    bool confirmationPressed_ = false;
    Rml::ElementDocument* document_ = nullptr;
    Rml::ElementDocument* homeDocument_ = nullptr;
    Rml::ElementDocument* settingsDocument_ = nullptr;
    Rml::ElementDocument* editorDocument_ = nullptr;
    Rml::ElementDocument* resultsDocument_ = nullptr;
    Rml::ElementDocument* skinDocument_ = nullptr;
    NativeSettingsUi settingsPresenter_;
    NativeEditorUi editorPresenter_;
    NativeResultsUi resultsPresenter_;
    NativeSkinUi skinPresenter_;
    NativePlayUi playPresenter_;
    Page visiblePage_ = Page::None;
    int pendingHomeAction_ = -1;
    std::string lastHomeNotice_;
    int lastWidth_ = -1, lastHeight_ = -1;
    float lastDensity_ = -1.0f;
    int lastEditorLayoutTab_ = -1;
    EditorPointerRouter editorPointerRouter_;
};

} // namespace locus::app
