#pragma once

#include "app/rml_raylib_renderer.h"
#include "app/menu_controller.h"
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

    // Poll only while SongSelect is visible. Other screens keep raylib input.
    MenuEvents Update(const SongLibrary& library, SongSelectUi& ui,
                      FrameInput& input);
    void Hide();
    void Render();

private:
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
    const SongLibrary* currentLibrary_ = nullptr;
    SongSelectUi* currentUi_ = nullptr;
    MenuEvents pending_;
    bool visible_ = false;
    int lastWidth_ = -1, lastHeight_ = -1;
    std::string lastTreeSignature_, lastResultSignature_, lastSummarySignature_;
};

} // namespace locus::app
