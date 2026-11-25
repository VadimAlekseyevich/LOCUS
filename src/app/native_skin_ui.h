#pragma once
#include "app/skin_menu.h"
#include <cstdint>
#include <array>
#include <string>
#include <unordered_map>

namespace Rml { class ElementDocument; class Event; class ElementFormControlInput; }
namespace locus::app {
struct FrameInput;
struct MenuEvents;

// Skin values, validation and profile storage remain in their domain model.
// This presenter creates RmlUi rows from the existing SkinSetting catalog.
class NativeSkinUi final {
public:
    void Attach(Rml::ElementDocument* document) noexcept;
    void BeginFrame(Skin& skin, SkinUi& ui,
                    FrameInput& input, MenuEvents& events) noexcept;
    void ProcessEvent(Rml::Event& event);
    void FinishFocusedText(bool apply);
    void Sync();
    // Native pointer capture for the model-backed slider / HSV controls.
    void UpdatePointer(Rectangle wheelBounds);
    void EndFrame() noexcept;
private:
    void RebuildControls();
    void CommitText(Rml::ElementFormControlInput& element);
    void SetText(const char* id, const std::string& value);
    Rml::ElementDocument* document_ = nullptr;
    Skin* skin_ = nullptr;
    SkinUi* ui_ = nullptr;
    FrameInput* input_ = nullptr;
    MenuEvents* events_ = nullptr;
    int lastPage_ = -1;
    int lastColor_ = -1;
    int lastProfileSelection_ = -1;
    std::uint64_t lastProfilesRevision_ = ~std::uint64_t(0);
    bool lastModal_ = false;
    bool cancelingText_ = false;
    std::string initialText_;
    std::unordered_map<std::string,std::string> cache_;
    std::array<float, 8> sliderRatios_{};
    std::array<std::string, kSkinColorCount> swatchColors_{};
};
} // namespace locus::app
