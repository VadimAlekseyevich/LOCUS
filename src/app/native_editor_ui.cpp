#include "app/native_editor_ui.h"

#include "app/app_input.h"
#include "app/chart_editor_ui.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace locus::app {
namespace {
std::string EscapeRml(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}
constexpr std::array<double, 5> kPreviewRates{1.0, 0.75, 0.5, 0.25, 0.1};
constexpr std::array<const char*, 5> kPreviewLabels{
    "100%", "75%", "50%", "25%", "10%"
};
} // namespace

void NativeEditorUi::Attach(Rml::ElementDocument* document) noexcept {
    document_ = document;
    textCache_.clear();
    rateOpen_ = false;
    lastTab_ = -1;
    lastTextEditing_ = -1;
}
void NativeEditorUi::BeginFrame(EditorUiState& ui, FrameInput& input) noexcept {
    ui_ = &ui;
    input_ = &input;
    consumedPointer_ = false;
}
void NativeEditorUi::EndFrame() noexcept {
    ui_ = nullptr;
    input_ = nullptr;
}
void NativeEditorUi::SetText(const char* id, const std::string& value) {
    if (!document_) return;
    const auto old = textCache_.find(id);
    if (old != textCache_.end() && old->second == value) return;
    if (auto* element = document_->GetElementById(id)) {
        element->SetInnerRML(EscapeRml(value));
        textCache_[id] = value;
    }
}
void NativeEditorUi::Sync(const EditorUiState& ui, const std::string& title,
                          bool dirty, double rate, bool running,
                          long long timeMs) {
    if (!document_) return;
    SetText("editor-chart-name", title);
    SetText("editor-dirty", dirty ? "*" : "");
    SetText("editor-preview-play", running ? "PAUSE" : "PLAY");
    int choice = 0;
    for (int i = 0; i < static_cast<int>(kPreviewRates.size()); ++i) {
        if (std::abs(rate - kPreviewRates[static_cast<std::size_t>(i)]) < 0.005)
            choice = i;
    }
    SetText("editor-preview-rate", kPreviewLabels[static_cast<std::size_t>(choice)]);
    SetText("editor-preview-time", std::to_string(timeMs) + " MS");
    if (lastTab_ != static_cast<int>(ui.tab)) {
        const auto setSelected = [&](const char* id, bool selected) {
            if (auto* element = document_->GetElementById(id))
                element->SetClass("selected", selected);
        };
        setSelected("editor-tab-set", ui.tab == EditorTab::Set);
        setSelected("editor-tab-route", ui.tab == EditorTab::Route);
        setSelected("editor-tab-audio", ui.tab == EditorTab::Audio);
        lastTab_ = static_cast<int>(ui.tab);
    }
    if (rateOpen_ != ui.rateDropdownOpen) {
        if (auto* popup = document_->GetElementById("editor-rate-popup"))
            popup->SetProperty("display",
                ui.rateDropdownOpen ? "flex" : "none");
        rateOpen_ = ui.rateDropdownOpen;
    }
    const bool textEditing = ui.EditingText();
    if (lastTextEditing_ != static_cast<int>(textEditing)) {
        for (const char* id : {"editor-tab-set", "editor-tab-route",
                               "editor-tab-audio", "editor-preview-play",
                               "editor-preview-restart", "editor-preview-rate",
                               "editor-preview-time"}) {
            if (auto* element = document_->GetElementById(id)) {
                if (textEditing) element->SetAttribute("disabled", "");
                else element->RemoveAttribute("disabled");
            }
        }
        lastTextEditing_ = static_cast<int>(textEditing);
    }
}
void NativeEditorUi::ProcessEvent(Rml::Event& event) {
    if (event.GetType() != "click" || !ui_ || !input_) return;
    auto* target = event.GetTargetElement();
    while (target && !target->HasAttribute("data-action"))
        target = target->GetParentNode();
    if (!target || target->HasAttribute("disabled")) return;
    const std::string action = target->GetAttribute<Rml::String>("data-action", "");
    if (action.empty()) return;
    consumedPointer_ = true;
    if (ui_->EditingText()) return;
    if (action == "back") {
        input_->backPressed = true;
    } else if (action == "tab-set" || action == "tab-route" ||
               action == "tab-audio") {
        ui_->tab = action == "tab-set" ? EditorTab::Set
                 : action == "tab-audio" ? EditorTab::Audio : EditorTab::Route;
        ui_->rateDropdownOpen = false;
        ui_->comparisonPickerOpen = false;
    } else if (action == "preview-play") {
        ui_->rateDropdownOpen = false;
        input_->editor.previewToggle = true;
    } else if (action == "preview-restart") {
        ui_->rateDropdownOpen = false;
        input_->editor.previewRestart = true;
    } else if (action == "rate-toggle") {
        ui_->rateDropdownOpen = !ui_->rateDropdownOpen;
    } else if (action == "rate") {
        const int index = target->GetAttribute<int>("data-index", -1);
        if (index >= 0 && index < static_cast<int>(kPreviewRates.size())) {
            input_->editor.speedIndex = index;
            ui_->rateDropdownOpen = false;
        }
    } else if (action == "time-copy") {
        ui_->rateDropdownOpen = false;
        input_->editor.copySongTime = true;
    }
}
} // namespace locus::app
