#pragma once
#include "app/run_results.h"
#include "app/library_virtual_list.h"
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
    float ReadScrollDp(float density) const;
    void RestoreScrollDp(float position, float density);
    void ProcessEvent(Rml::Event& event);
    void EndFrame() noexcept;
private:
    void SetText(const char* id, const std::string& value);
    void SyncHistogram();
    Rml::ElementDocument* document_ = nullptr;
    const RunResult* selected_ = nullptr;
    const core::GradeCounts* grades_ = nullptr;
    const SongSelectUi* history_ = nullptr;
    FrameInput* input_ = nullptr;
    MenuEvents* events_ = nullptr;
    std::string chartTitle_;
    bool historical_ = false;
    core::ResultRank lastRank_ = core::ResultRank::None;
    bool rankValid_ = false;
    std::string lastHistogramId_;
    RunKey lastHistogramKey_;
    bool histogramValid_ = false;
    std::unordered_map<std::string, std::string> textCache_;
};
} // namespace locus::app
