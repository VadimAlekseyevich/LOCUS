#pragma once

#include "app/music_picker.h"
#include "app/skin.h"
#include "app/skin_menu.h"

#include <filesystem>
#include <utility>
#include <string>

namespace locus::app {

// Platform file dialogs remain injected; the profile controller only manages
// draft/applied states and atomic on-disk saves. No chart or audio ownership.
struct SkinProfilePickers {
    MusicPickResult (*chooseLoad)();
    MusicPickResult (*chooseExport)(const std::wstring&);
};

class SkinProfileActions {
public:
    SkinProfileActions(Skin& active, Skin& draft, SkinUi& ui,
                       std::filesystem::path activeFile,
                       SkinProfilePickers pickers) noexcept
        : active_(active), draft_(draft), ui_(ui),
          activeFile_(std::move(activeFile)), pickers_(pickers) {}

    void Apply(SkinCommand command);
    bool RefreshProfiles();

private:
    bool SaveActive();
    Skin& active_;
    Skin& draft_;
    SkinUi& ui_;
    std::filesystem::path activeFile_;
    std::filesystem::path activeProfile_; // Separate selected file in Profiles.
    SkinProfilePickers pickers_;
};

} // namespace locus::app
