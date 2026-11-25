#pragma once
#include <string>

namespace locus::app {
enum class ConfirmationChoice { None, Save, Approve, Cancel };

struct ConfirmationView {
    bool visible = false;
    bool canSave = false;
    std::string heading, message, notice;
    std::string approveLabel = "Y / CONFIRM";
};

// Escape/cancel takes precedence over destructive actions on coincident keys.
inline ConfirmationChoice ChooseConfirmation(const ConfirmationView& view,
    bool save, bool approve, bool cancel) noexcept {
    if (!view.visible) return ConfirmationChoice::None;
    if (cancel) return ConfirmationChoice::Cancel;
    if (save && view.canSave) return ConfirmationChoice::Save;
    return approve ? ConfirmationChoice::Approve : ConfirmationChoice::None;
}
} // namespace locus::app
