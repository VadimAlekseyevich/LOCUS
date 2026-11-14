#pragma once

#include "app/rml_raylib_renderer.h"
#include "app/menu_controller.h"
#include "app/native_settings_ui.h"
#include "app/native_editor_ui.h"
#include <RmlUi/Core/EventListener.h>

#include <string>

namespace Rml {
class Context;
class ElementDocument;
class Element;
}

namespace locus::app {

// First native responsive RmlUi screen: PLAY song selection. State and
// actions remain owned by the existing SongSelectUi / MenuEvents / library.
// This object draws and routes events; it never loads or saves charts.
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
    bool ShowingHome() const noexcept;
    bool ShowingPlay() const noexcept;
    bool ShowingSettings() const noexcept;
    bool ShowingEditor() const noexcept;
    Rectangle HomeWordmarkBounds() const noexcept;
    // Share RmlUi input and rendering for HOME and PLAY; both retain fallback.
    void UpdateHome(FrameInput& input, const std::string& status);
    void UpdateEditor(EditorUiState& ui, FrameInput& input,
                      const std::string& title, bool dirty,
                      double rate, bool playing, long long songTimeMs);
    void SyncEditor(const EditorUiState& ui, const std::string& title,
                    bool dirty, double rate, bool playing, long long songTimeMs);
    void UpdateSettings(GameSettings& settings, SettingsUi& ui,
                        FrameInput& input, MenuEvents& events,
                        bool exclusiveKeyboardInput);

    // Poll only while SongSelect is visible. Other screens keep raylib input.
    MenuEvents Update(const SongLibrary& library, SongSelectUi& ui,
                      FrameInput& input);
    void Hide();
    void Render();

private:
    enum class Page { None, Home, Play, Settings, Editor };
    void Activate(Page page);
    void PumpInput(bool dispatchKeyboard = true);
    void ProcessEvent(Rml::Event& event) override;
    void Rebuild(const SongLibrary& library, SongSelectUi& ui);
    void UpdateSummary(const SongLibrary& library, SongSelectUi& ui);
    void RebuildRows(const SongLibrary& library, SongSelectUi& ui);
    void RebuildResults(SongSelectUi& ui);
    void SetText(const char* id, const std::string& value);
    bool IsSearchFocused() const;

    RmlRaylibSystem system_;
    RmlRaylibRenderer renderer_;
    Rml::Context* context_ = nullptr;
    Rml::ElementDocument* document_ = nullptr;
    Rml::ElementDocument* homeDocument_ = nullptr;
    Rml::ElementDocument* settingsDocument_ = nullptr;
    Rml::ElementDocument* editorDocument_ = nullptr;
    NativeSettingsUi settingsPresenter_;
    NativeEditorUi editorPresenter_;
    const SongLibrary* currentLibrary_ = nullptr;
    SongSelectUi* currentUi_ = nullptr;
    MenuEvents pending_;
    Page visiblePage_ = Page::None;
    int pendingHomeAction_ = -1;
    std::string lastHomeNotice_;
    int lastWidth_ = -1, lastHeight_ = -1;
    float lastDensity_ = -1.0f;
    int lastEditorWidth_ = -1, lastEditorHeight_ = -1;
    // Raylib pointer capture persists across the full press/drag/release
    // gesture begun on native editor chrome, even when the pointer moves.
    bool editorChromePressActive_ = false;
    std::string lastTreeSignature_, lastResultSignature_, lastSummarySignature_;
};

} // namespace locus::app
