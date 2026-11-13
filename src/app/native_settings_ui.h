#pragma once

#include <string>
#include <unordered_map>

namespace Rml { class ElementDocument; class Event; }
namespace locus::app {
struct GameSettings;
struct SettingsUi;
struct FrameInput;
struct MenuEvents;

// Presenter owns no settings data: one screen's DOM is synchronized with the
// existing GameSettings/SettingsUi source of truth and emits MenuEvents.
class NativeSettingsUi final {
public:
    void Attach(Rml::ElementDocument* document) noexcept;
    void BeginFrame(GameSettings& settings, SettingsUi& ui,
                    FrameInput& input, MenuEvents& events) noexcept;
    void ProcessEvent(Rml::Event& event);
    void Sync();
    void EndFrame() noexcept;

private:
    void SetText(const char* id, const std::string& value);
    void UpdateBindings();
    Rml::ElementDocument* document_ = nullptr;
    GameSettings* settings_ = nullptr;
    SettingsUi* ui_ = nullptr;
    FrameInput* input_ = nullptr;
    MenuEvents* events_ = nullptr;
    std::unordered_map<std::string, std::string> textCache_;
    std::string bindingSignature_;
    int lastPage_ = -1;
    int lastGroup_ = -1;
    bool lastCalibration_ = false;
};
} // namespace locus::app
