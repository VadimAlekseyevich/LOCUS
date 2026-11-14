#pragma once
#include "app/run_results.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Rml { class ElementDocument; class Event; }
namespace locus::app {
struct FrameInput;
struct MenuEvents;
struct SongSelectUi;

// Read-only presenter: result state remains owned by the gameplay/session
// and history store. Navigation is emitted through existing app commands.
class NativeResultsUi final {
public:
    void Attach(Rml::ElementDocument* document) noexcept;
    void BeginFrame(const RunResult* selected, const core::GradeCounts& grades,
                    int total, const std::string& title, bool historical,
                    const SongSelectUi& history, FrameInput& input,
                    MenuEvents& events) noexcept;
    void Sync();
    void ProcessEvent(Rml::Event& event);
    void EndFrame() noexcept;
private:
    void SetText(const char* id, const std::string& value);
    Rml::ElementDocument* document_ = nullptr;
    const RunResult* selected_ = nullptr;
    const core::GradeCounts* grades_ = nullptr;
    const SongSelectUi* history_ = nullptr;
    FrameInput* input_ = nullptr;
    MenuEvents* events_ = nullptr;
    std::string chartTitle_;
    bool historical_ = false;
    int total_ = 0;
    std::uint64_t lastHistoryRevision_ = ~std::uint64_t(0);
    std::string lastSelectedId_;
    std::unordered_map<std::string, std::string> textCache_;
};
} // namespace locus::app
