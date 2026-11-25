#pragma once

#include "app/virtual_list.h"
#include <string>
#include <unordered_map>

namespace Rml { class ElementDocument; class Event; }
namespace locus::core { class ChartEditor; }
namespace locus::app {
struct EditorUiState;
struct FrameInput;
struct GameSettings;
struct EditorSetView;

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
              long long timeMs, const core::ChartEditor* draft = nullptr,
              const GameSettings* settings = nullptr,
              const EditorSetView* setView = nullptr);
    void EndFrame() noexcept;
    void CancelFocusedField();
private:
    void SetText(const char* id, const std::string& value);
    Rml::ElementDocument* document_ = nullptr;
    EditorUiState* ui_ = nullptr;
    FrameInput* input_ = nullptr;
    std::unordered_map<std::string, std::string> textCache_;
    int lastRatePercent_ = 100;
    std::string focusedInitialValue_;
    struct DifficultyList {
        LibraryRowWindow window;
        std::string identity,visibleSignature;
        float stepDp=0.0f;
        bool valid=false;
    };
    void SyncDifficultyList(const EditorUiState& ui,const EditorSetView& view,
                            const std::string& title,bool compare);
    DifficultyList setList_,compareList_;
    std::string activeSetId_;
    bool compareVisible_ = false;
    int lastTab_ = -1, lastTextEditing_ = -1;
    int tempoRows_ = -1;
};
} // namespace locus::app
