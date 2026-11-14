#include "app/native_settings_ui.h"

#include "app/menu_controller.h"
#include "app/menu_ui.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace locus::app {
namespace {
std::string EscapeRml(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}
int GlobalBindingIndex(int group, int local) {
    return group == 0 ? local : kGameActionCount +
        (group == 1 ? local : kNavigationShortcutCount + local);
}
} // namespace

void NativeSettingsUi::Attach(Rml::ElementDocument* document) noexcept {
    document_ = document;
    bindingSignature_.clear();
    textCache_.clear();
    lastPage_ = -1;
    lastGroup_ = -1;
    lastCalibration_ = false;
}
void NativeSettingsUi::BeginFrame(GameSettings& settings, SettingsUi& ui,
                                  FrameInput& input, MenuEvents& events) noexcept {
    settings_ = &settings;
    ui_ = &ui;
    input_ = &input;
    events_ = &events;
}
void NativeSettingsUi::EndFrame() noexcept {
    settings_ = nullptr;
    ui_ = nullptr;
    input_ = nullptr;
    events_ = nullptr;
}
void NativeSettingsUi::SetText(const char* id, const std::string& value) {
    if (!document_) return;
    const auto found = textCache_.find(id);
    if (found != textCache_.end() && found->second == value) return;
    if (auto* element = document_->GetElementById(id)) {
        const std::string content = EscapeRml(value);
        element->SetInnerRML(element->GetTagName() == "button"
            ? "<span class='button-label'>" + content + "</span>"
            : content);
        textCache_[id] = value;
    }
}
void NativeSettingsUi::UpdateBindings() {
    if (!document_ || !settings_ || !ui_) return;
    std::ostringstream html, signature;
    const int group = ui_->bindingGroup;
    const int count = SettingsBindingCount(group);
    signature << group << ':' << ui_->capturing << ':';
    for (int local = 0; local < count; ++local) {
        const int index = GlobalBindingIndex(group, local);
        const bool game = index < kGameActionCount;
        const std::string name = game
            ? ActionName(static_cast<GameAction>(index))
            : ShortcutName(static_cast<ShortcutAction>(index - kGameActionCount));
        const std::string key = ui_->capturing == index ? "PRESS KEY..."
            : game ? KeyboardKeyName(settings_->Key(static_cast<GameAction>(index)))
            : KeyboardChordName(settings_->Shortcut(
                static_cast<ShortcutAction>(index - kGameActionCount)));
        signature << index << '/' << key << ';';
        html << "<div class='binding-row'><span class='binding-label'>"
             << EscapeRml(name) << "</span><button class='binding-key"
             << (ui_->capturing == index ? " capturing" : "")
             << "' data-action='bind' data-index='" << index << "'>"
             << "<span class='button-label'>" << EscapeRml(key)
             << "</span></button></div>";
    }
    if (signature.str() != bindingSignature_) {
        if (auto* list = document_->GetElementById("bind-list"))
            list->SetInnerRML(html.str());
        bindingSignature_ = signature.str();
        if (group != lastGroup_) {
            if (auto* scroll = document_->GetElementById("bind-scroll"))
                scroll->SetScrollTop(0.0f);
            lastGroup_ = group;
        }
    }
}
void NativeSettingsUi::Sync() {
    if (!document_ || !settings_ || !ui_) return;
    const bool display = ui_->page == SettingsPage::Display;
    if (lastPage_ != static_cast<int>(ui_->page)) {
        if (auto* elem = document_->GetElementById("settings-display"))
            elem->SetProperty("display", display ? "flex" : "none");
        if (auto* elem = document_->GetElementById("settings-audio"))
            elem->SetProperty("display", display ? "none" : "flex");
        if (auto* elem = document_->GetElementById("tab-display"))
            elem->SetClass("selected", display);
        if (auto* elem = document_->GetElementById("tab-audio"))
            elem->SetClass("selected", !display);
        lastPage_ = static_cast<int>(ui_->page);
    }
    for (int group = 0; group < 3; ++group) {
        const char* id = group == 0 ? "group-play" :
                         group == 1 ? "group-menus" : "group-editor";
        if (auto* elem = document_->GetElementById(id))
            elem->SetClass("selected", ui_->bindingGroup == group);
    }
    SetText("value-vsync", settings_->vsync ? "ON" : "OFF");
    SetText("value-fps", settings_->FpsLimitLabel());
    SetText("value-show-fps", settings_->showFps ? "ON" : "OFF");
    SetText("value-fullscreen", settings_->fullscreen ? "ON" : "OFF");
    SetText("value-volume", std::to_string(
        static_cast<int>(std::lround(settings_->volume * 100.0f))) + "%");
    const std::string offset = ui_->editingGlobalOffset
        ? ui_->globalOffsetBuffer + "_"
        : (settings_->globalOffsetMs > 0 ? "+" : "") +
              std::to_string(settings_->globalOffsetMs) + " MS";
    SetText("value-offset", offset);
    SetText("settings-notice", ui_->notice);
    if (auto* elem = document_->GetElementById("settings-notice"))
        elem->SetClass("error", ui_->noticeError);
    UpdateBindings();
    if (ui_->calibrationOpen != lastCalibration_) {
        if (auto* modal = document_->GetElementById("settings-calibration"))
            modal->SetProperty("display",
                ui_->calibrationOpen ? "flex" : "none");
        lastCalibration_ = ui_->calibrationOpen;
    }
    if (!ui_->calibrationOpen) return;
    const auto& calibration = ui_->calibration;
    SetText("calibration-progress", std::to_string(calibration.Count()) +
        " / " + std::to_string(GlobalOffsetCalibrator::kRequiredTaps) + " TAPS");
    const bool reliable = calibration.Complete() && calibration.Reliable();
    if (reliable)
        SetText("calibration-feedback", "SUGGESTED OFFSET: " +
            std::to_string(calibration.SuggestedMs()) + " MS / VARIATION " +
            std::to_string(calibration.JitterMs()) + " MS");
    else if (calibration.Complete() || ui_->calibrationExpired)
        SetText("calibration-feedback", "INCONSISTENT TAPS - RETRY");
    else
        SetText("calibration-feedback", "KEEP TAPPING IN TIME WITH THE BEAT");
    if (auto* elem = document_->GetElementById("calibration-apply")) {
        if (reliable) elem->RemoveAttribute("disabled");
        else elem->SetAttribute("disabled", "");
    }
}
void NativeSettingsUi::ProcessEvent(Rml::Event& event) {
    if (!document_ || !ui_ || !settings_ || !input_ || !events_ ||
        event.GetType() != "click") return;
    Rml::Element* target = event.GetTargetElement();
    while (target && !target->HasAttribute("data-action"))
        target = target->GetParentNode();
    if (!target) return;
    const std::string action = target->GetAttribute<Rml::String>("data-action", "");
    if (ui_->calibrationOpen) {
        if (action == "calibration-cancel") {
            ui_->calibrationOpen = false;
            events_->calibrationStop = true;
        } else if (action == "calibration-retry") {
            ui_->calibration.Start();
            ui_->calibrationExpired = false;
            events_->calibrationStart = true;
        } else if (action == "calibration-apply" &&
                   ui_->calibration.Complete() && ui_->calibration.Reliable()) {
            settings_->globalOffsetMs = ui_->calibration.SuggestedMs();
            ui_->calibrationOpen = false;
            ui_->notice = "GLOBAL OFFSET CALIBRATED: " +
                std::to_string(settings_->globalOffsetMs) + " MS";
            ui_->noticeError = false;
            events_->saveSettings = true;
            events_->calibrationStop = true;
        }
        return;
    }
    if (ui_->capturing >= 0 || ui_->editingGlobalOffset) return;
    if (action == "back") input_->backPressed = true;
    else if (action == "page-display") ui_->page = SettingsPage::Display;
    else if (action == "page-audio") ui_->page = SettingsPage::Game;
    else if (action == "group-play" || action == "group-menus" ||
             action == "group-editor") {
        ui_->bindingGroup = action == "group-play" ? 0 :
                            action == "group-menus" ? 1 : 2;
        ui_->firstBinding = 0;
    } else if (action == "bind") {
        const int index = target->GetAttribute<int>("data-index", -1);
        if (index >= 0 && index < kGameActionCount + kShortcutActionCount) {
            ui_->capturing = index;
            ui_->notice = "PRESS KEY (+CTRL/SHIFT) / RIGHT-CLICK CANCEL";
            ui_->noticeError = false;
        }
    } else if (action == "vsync") {
        settings_->vsync = !settings_->vsync;
        events_->applyDisplaySettings = true;
        events_->saveSettings = true;
    } else if (action == "fps-minus" || action == "fps-plus") {
        const int before = settings_->fpsLimit;
        settings_->StepFpsLimit(action == "fps-plus" ? 1 : -1);
        if (settings_->fpsLimit != before) {
            events_->applyDisplaySettings = true;
            events_->saveSettings = true;
        }
    } else if (action == "show-fps") {
        settings_->showFps = !settings_->showFps;
        events_->saveSettings = true;
    } else if (action == "fullscreen") {
        settings_->fullscreen = !settings_->fullscreen;
        events_->toggleFullscreen = true;
        events_->saveSettings = true;
    } else if (action == "volume-minus" || action == "volume-plus") {
        const int current = static_cast<int>(
            std::lround(settings_->volume * 10.0f));
        settings_->volume = std::clamp(current +
            (action == "volume-plus" ? 1 : -1), 0, 10) / 10.0f;
        events_->saveSettings = true;
    } else if (action == "edit-offset") {
        ui_->editingGlobalOffset = true;
        ui_->globalOffsetBuffer = std::to_string(settings_->globalOffsetMs);
        ui_->notice = "TYPE OFFSET MS, ENTER SAVE, ESC CANCEL";
        ui_->noticeError = false;
    } else if (action == "calibrate") {
        ui_->calibrationOpen = true;
        ui_->calibrationExpired = false;
        ui_->calibration.Start();
        events_->calibrationStart = true;
    }
}
} // namespace locus::app
