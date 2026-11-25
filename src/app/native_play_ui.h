#pragma once

#include "app/menu_controller.h"
#include "app/library_virtual_list.h"
#include <string>
#include <vector>

namespace Rml { class ElementDocument; class Event; }

namespace locus::app {

// PLAY owns its virtual-list DOM, cached presentation and typed commands.
// Library and selection remain borrowed only while a frame is dispatched.
class NativePlayUi final {
public:
    struct ScrollPosition { float treeDp = 0.0f; float historyDp = 0.0f; };

    void Attach(Rml::ElementDocument* document) noexcept;
    void SetSearch(const std::string& search);
    ScrollPosition ReadScrollPosition(float density) const;
    void RestoreScrollPosition(ScrollPosition position, float density);
    void BeginFrame(const SongLibrary& library, SongSelectUi& ui, float density) noexcept;
    void Sync();
    void ProcessEvent(Rml::Event& event);
    void ProcessKeyboard(FrameInput& input);
    MenuEvents EndFrame() noexcept;

private:
    void Rebuild(const SongLibrary& library, SongSelectUi& ui);
    void UpdateSummary(const SongLibrary& library, SongSelectUi& ui);
    void RebuildRows(const SongLibrary& library, SongSelectUi& ui);
    void SyncTreeSelection(const SongSelectUi& ui);
    void RebuildResults(SongSelectUi& ui);
    void SetText(const char* id, const std::string& value);
    bool IsSearchFocused() const;

    Rml::ElementDocument* document_ = nullptr;
    const SongLibrary* currentLibrary_ = nullptr;
    SongSelectUi* currentUi_ = nullptr;
    MenuEvents pending_;
    float lastDensity_ = 1.0f;
    std::string lastResultSignature_, lastSummarySignature_;
    const LibraryTreeCache* lastTreeCache_ = nullptr; // identity only
    std::uint64_t lastTreeGeneration_ = 0;
    std::vector<float> treeOffsets_;
    std::size_t treeFirst_ = static_cast<std::size_t>(-1);
    std::size_t treeEnd_ = static_cast<std::size_t>(-1);
    int lastSelectedSong_ = -1;
    int lastSelectedChart_ = -1;
    bool lastTreeChartFocused_ = false;
    std::size_t resultFirst_ = static_cast<std::size_t>(-1);
    std::size_t resultEnd_ = static_cast<std::size_t>(-1);
    float resultRowStep_ = 43.0f;
};

} // namespace locus::app
