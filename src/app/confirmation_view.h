#pragma once
#include <string>

namespace locus::app {
enum class ConfirmationChoice { None, Save, Approve, Cancel };

struct ConfirmationView {
    bool visible = false;
    bool canSave = false;
    std::string heading, message, notice;
    std::string approveLabel = "Y / CONFIRM";
    std::string saveLabel = "S / SAVE", cancelLabel = "N / CANCEL";
    bool operator==(const ConfirmationView& other) const noexcept {
        return visible == other.visible && canSave == other.canSave &&
            heading == other.heading && message == other.message && notice == other.notice &&
            approveLabel == other.approveLabel && saveLabel == other.saveLabel && cancelLabel == other.cancelLabel;
    }
};

// Escape/cancel takes precedence over destructive actions on coincident keys.
inline ConfirmationChoice ChooseConfirmation(const ConfirmationView& view,
    bool save, bool approve, bool cancel) noexcept {
    if (!view.visible) return ConfirmationChoice::None;
    if (cancel) return ConfirmationChoice::Cancel;
    if (save && view.canSave) return ConfirmationChoice::Save;
    return approve ? ConfirmationChoice::Approve : ConfirmationChoice::None;
}
// Global lifecycle prompts share the editor's presentation contract.
// The caller retains ownership of save operations and screen transitions.
inline ConfirmationView GlobalConfirmationView(bool quit, bool leaveLevel,
    bool dirtyCharts, bool dirtySkin, bool dirtyText, const std::string& status) {
    ConfirmationView view;
    if (!quit && !leaveLevel) return view;
    view.visible = true;
    view.cancelLabel = "N / STAY";
    if (!quit) {
        view.heading = "LEAVE THIS LEVEL?";
        view.message = "Unfinished run progress will be lost.";
        view.approveLabel = "Y / LEAVE";
        return view;
    }
    view.heading = "EXIT LOCUS?";
    view.message = "This will close the game.";
    view.approveLabel = "Y / QUIT";
    view.saveLabel = "S / SAVE + QUIT";
    view.canSave = dirtyCharts && !dirtySkin;
    if ((dirtyCharts || dirtyText) && dirtySkin)
        view.message += " Unsaved chart and skin changes will be lost.";
    else if (dirtyCharts || dirtyText)
        view.message += " Unsaved chart changes will be lost.";
    else if (dirtySkin)
        view.message += " Unsaved skin changes will be lost.";
    if (dirtySkin && dirtyCharts) view.notice = "Cancel quit and save the skin first.";
    else if (view.canSave && status.find("SAVE FAILED") != std::string::npos)
        view.notice = status;
    return view;
}
} // namespace locus::app
