#include "app/skin_profile_actions.h"

#include <string>

namespace locus::app {

bool SkinProfileActions::SaveActive() {
    std::string error;
    if (!SaveSkin(activeFile_, draft_, error)) {
        ui_.notice = "SAVE FAILED: " + error;
        ui_.noticeError = true;
        return false;
    }
    active_ = draft_;
    ui_.saved = active_;
    ui_.dirty = false;
    ui_.notice = "SKIN SAVED AND APPLIED TO ALL CHARTS";
    ui_.noticeError = false;
    return true;
}

void SkinProfileActions::Apply(SkinCommand command) {
    switch (command) {
        case SkinCommand::Load: {
            const auto picked = pickers_.chooseLoad();
            if (picked.path.empty()) {
                if (!picked.canceled) {
                    ui_.notice = picked.error;
                    ui_.noticeError = true;
                }
                return;
            }
            Skin selected;
            std::string error;
            // Unlike startup defaults, an explicit picker must reject a
            // vanished file. Never replace the draft with silent defaults.
            if (!LoadSkin(picked.path, selected, error, false)) {
                ui_.notice = "LOAD FAILED: " + error;
                ui_.noticeError = true;
                return;
            }
            draft_ = selected;
            ui_.dirty = true;
            ui_.notice = "SKIN LOADED - SAVE TO ACTIVATE";
            ui_.noticeError = false;
            break;
        }
        case SkinCommand::Save:
            SaveActive();
            break;
        case SkinCommand::SaveAs: {
            const auto picked =
                pickers_.chooseExport(L"My-Skin.locus-skin.json");
            if (picked.path.empty()) {
                if (!picked.canceled) {
                    ui_.notice = picked.error;
                    ui_.noticeError = true;
                }
                return;
            }
            std::string error;
            if (!SaveSkin(picked.path, draft_, error)) {
                ui_.notice = "EXPORT FAILED: " + error;
                ui_.noticeError = true;
                return;
            }
            // Preserve historical behavior: exporting also applies the
            // draft to the one global profile used by every screen.
            SaveActive();
            break;
        }
        case SkinCommand::Reset:
            draft_ = Skin{};
            ui_.dirty = true;
            ui_.notice = "DEFAULT SKIN - SAVE TO ACTIVATE";
            ui_.noticeError = false;
            break;
        case SkinCommand::None: break;
    }
}

} // namespace locus::app
