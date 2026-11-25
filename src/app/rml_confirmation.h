#pragma once
#include "app/confirmation_view.h"
#include "app/rml_modal_focus.h"
#include "app/rml_ui_text.h"
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

namespace locus::app {

// A document declares the same confirmation template IDs. The component
// manages display, focus and typed choices without knowing any domain action.
class RmlConfirmation {
public:
    void Attach(Rml::ElementDocument* document) {
        document_ = document;
        view_ = {};
        choice_ = ConfirmationChoice::None;
        if (auto* layer = Element("confirmation-layer"))
            layer->SetProperty("display","none");
    }

    bool SamePrompt(const ConfirmationView& view) const noexcept {
        // A new prompt may replace an existing one without a hidden frame.
        // Notice/status updates do not invalidate a user's focus.
        return view.visible == view_.visible && (!view.visible ||
            (view.heading == view_.heading && view.message == view_.message &&
             view.approveLabel == view_.approveLabel));
    }

    bool Sync(const ConfirmationView& view) {
        if (!document_ || view == view_) return false;
        auto* panel = Panel();
        if (!SamePrompt(view)) {
            if (view.visible) document_->Focus();
            else ReleaseRmlModalFocus(panel);
            if (auto* layer = Element("confirmation-layer"))
                layer->SetProperty("display",view.visible ? "flex" : "none");
            choice_ = ConfirmationChoice::None;
        }
        if (view.visible) {
            if (view.heading != view_.heading || !view_.visible)
                UpdateRmlText(Element("confirmation-heading"),view.heading);
            if (view.message != view_.message || !view_.visible)
                UpdateRmlText(Element("confirmation-message"),view.message);
            if (view.notice != view_.notice || !view_.visible)
                UpdateRmlText(Element("confirmation-notice"),view.notice);
            if (view.approveLabel != view_.approveLabel || !view_.visible)
                UpdateRmlText(Element("confirmation-approve"),view.approveLabel);
            if (view.saveLabel != view_.saveLabel || !view_.visible)
                UpdateRmlText(Element("confirmation-save"),view.saveLabel);
            if (view.cancelLabel != view_.cancelLabel || !view_.visible)
                UpdateRmlText(Element("confirmation-cancel"),view.cancelLabel);
            if (view.canSave != view_.canSave || !view_.visible)
                if (auto* save = Element("confirmation-save")) {
                    if (!view.canSave && document_->GetContext()->GetFocusElement()==save)
                        document_->Focus();
                    save->SetProperty("display",view.canSave ? "inline-block" : "none");
                }
        }
        view_ = view;
        return true;
    }

    bool Visible() const noexcept { return view_.visible; }
    Rml::Element* Panel() const { return Element("confirmation-panel"); }
    void FocusCancelIfOutside() const {
        if (!document_ || !Visible()) return;
        if (!RmlFocusWithin(Panel(),document_->GetContext()->GetFocusElement()))
            if (auto* cancel = Element("confirmation-cancel")) cancel->Focus();
    }
    void Click(Rml::Element* target) {
        if (!Visible() || !RmlFocusWithin(Panel(),target)) return;
        while (target && target != Panel() && !target->HasAttribute("data-confirmation"))
            target = target->GetParentNode();
        if (!target || target->HasAttribute("disabled")) return;
        const auto action = target->GetAttribute<Rml::String>("data-confirmation","");
        const auto choice = ChooseConfirmation(view_,action=="save",action=="approve",action=="cancel");
        if (choice != ConfirmationChoice::None) choice_ = choice;
    }
    ConfirmationChoice TakeChoice() noexcept {
        const auto result = choice_;
        choice_ = ConfirmationChoice::None;
        return result;
    }
private:
    Rml::Element* Element(const char* id) const {
        return document_ ? document_->GetElementById(id) : nullptr;
    }
    Rml::ElementDocument* document_ = nullptr;
    ConfirmationView view_;
    ConfirmationChoice choice_ = ConfirmationChoice::None;
};
} // namespace locus::app
