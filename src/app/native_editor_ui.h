#pragma once

#include <string>
#include <unordered_map>

namespace Rml { class ElementDocument; class Event; }
namespace locus::app {
struct EditorUiState;
struct FrameInput;

// Shared editor chrome presenter. RmlUi handles the menu header and
// transport buttons; chart authoring, waveform, route preview and timelines
// remain under their existing raylib/core controllers.
class NativeEditorUi final {
public:
    void Attach(Rml::ElementDocument* document) noexcept;
    void BeginFrame(EditorUiState& ui, FrameInput& input) noexcept;
    void ProcessEvent(Rml::Event& event);
    void Sync(const EditorUiState& ui, const std::string& chartTitle,
              bool dirty, double playbackRate, bool previewRunning,
              long long timeMs);
    bool ConsumedPointer() const noexcept { return consumedPointer_; }
    void SuppressCurrentClick() noexcept { suppressClick_ = true; }
    bool SuppressingClick() const noexcept { return suppressClick_; }
    void ReleaseClickSuppression() noexcept { suppressClick_ = false; }
    void EndFrame() noexcept;
private:
    void SetText(const char* id, const std::string& value);
    Rml::ElementDocument* document_ = nullptr;
    EditorUiState* ui_ = nullptr;
    FrameInput* input_ = nullptr;
    bool consumedPointer_ = false;
    // Keeps an outside-popup press from activating the UI underneath on
    // its later mouse-up (RmlUi dispatches clicks on release).
    bool suppressClick_ = false;
    std::unordered_map<std::string, std::string> textCache_;
    bool rateOpen_ = false;
    int lastTab_ = -1, lastTextEditing_ = -1;
};
} // namespace locus::app
