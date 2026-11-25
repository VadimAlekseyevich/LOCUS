#include "app/native_skin_ui.h"
#include "app/app_input.h"
#include "app/menu_controller.h"
#include "app/skin_controls.h"
#include "app/rml_ui_text.h"
#include "core/utf8.h"
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Context.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace locus::app {
namespace {
std::string Escape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
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
std::string HexColor(Color color) {
    char value[12];
    std::snprintf(value, sizeof(value), "#%02X%02X%02X%02X",
        color.r, color.g, color.b, color.a);
    return value;
}
const char* TabId(int i) {
    static const char* ids[] = {"skin-tab-path", "skin-tab-player",
                               "skin-tab-colors", "skin-tab-profiles"};
    return ids[std::clamp(i, 0, 3)];
}
} // namespace

void NativeSkinUi::Attach(Rml::ElementDocument* document) noexcept {
    document_ = document;
    lastPage_ = -1;
    lastColor_ = -1;
    lastProfileSelection_ = -1;
    lastProfilesRevision_ = ~std::uint64_t(0);
    lastModal_ = false;
    cache_.clear();
}
void NativeSkinUi::BeginFrame(Skin& skin, SkinUi& ui,
                              FrameInput& input, MenuEvents& events) noexcept {
    skin_ = &skin;
    ui_ = &ui;
    input_ = &input;
    events_ = &events;
}
void NativeSkinUi::EndFrame() noexcept {
    skin_ = nullptr;
    ui_ = nullptr;
    input_ = nullptr;
    events_ = nullptr;
}
void NativeSkinUi::SetText(const char* id, const std::string& value) {
    if (!document_) return;
    const auto it = cache_.find(id);
    if (it != cache_.end() && it->second == value) return;
    if (auto* element = document_->GetElementById(id)) {
        UpdateRmlText(element, value);
        cache_[id] = value;
    }
}
void NativeSkinUi::RebuildControls() {
    if (!skin_ || !ui_ || !document_) return;
    std::ostringstream html;
    if (ui_->page < 2) {
        for (int row = 0; row < SkinSettingCount(ui_->page); ++row) {
            const auto* setting = GetSkinSetting(ui_->page, row);
            if (!setting) continue;
            html << "<div class='skin-setting-row'><div class='skin-setting-name'>"
                 << Escape(setting->label) << "</div>";
            if (setting->toggle) {
                html << "<button data-action='toggle' data-index='" << row
                     << "'><span class='button-label'>"
                     << (GetSkinToggle(*skin_, ui_->page, row) ? "ON" : "OFF")
                     << "</span></button>";
            } else {
                const double value = GetSkinNumber(*skin_, ui_->page, row,
                                                   ui_->selectedColor);
                html << "<div class='skin-slider' id='skin-slider-" << row
                     << "'><div class='skin-slider-track' id='skin-slider-track-" << row
                     << "'><div class='skin-slider-fill' id='skin-slider-fill-" << row
                     << "'></div><div class='skin-slider-knob' id='skin-slider-knob-"
                     << row << "'></div></div></div>"
                     << "<input type='text' class='skin-number' id='skin-number-"
                     << row << "' data-kind='number' data-index='" << row
                     << "' value='" << Escape(FormatSkinNumber(value)) << "' />";
            }
            html << "</div>";
        }
    } else if (ui_->page == 2) {
        html << "<div class='skin-swatches'>";
        for (int i = 0; i < kSkinColorCount; ++i) {
            html << "<button class='skin-swatch"
                 << (i == ui_->selectedColor ? " selected" : "")
                 << "' data-action='swatch' data-index='" << i
                 << "'><span class='button-label'>" << Escape(SkinColorLabel(i))
                 << "</span><span id='skin-swatch-chip-" << i
                 << "' class='skin-swatch-chip' style='background-color:"
                 << HexColor(SkinColor(*skin_, i)) << "'></span></button>";
        }
        html << "</div><div class='skin-wheel-title'>HUE / SATURATION / VALUE</div>"
                "<div id='skin-wheel-canvas'></div>";
    } else {
        html << "<div class='skin-profile-list'>";
        for (std::size_t i = 0; i < ui_->profiles.size(); ++i) {
            html << "<button class='skin-profile"
                 << (static_cast<int>(i) == ui_->selectedProfile ? " selected" : "")
                 << "' data-action='profile' data-index='" << i
                 << "'><span class='button-label'>"
                 << Escape(ui_->profiles[i].name) << "</span></button>";
        }
        if (ui_->profiles.empty())
            html << "<div class='empty'>NO PERSONAL SKINS YET</div>";
        html << "</div><div class='skin-profile-actions'>"
                "<button data-action='activate'><span class='button-label'>ACTIVATE</span></button>"
                "<button data-action='store'><span class='button-label'>STORE</span></button>"
                "<button data-action='refresh'><span class='button-label'>REFRESH</span></button>"
                "<button data-action='delete'><span class='button-label'>DELETE</span></button>"
                "<button data-action='restore'><span class='button-label'>UNDO DELETE</span></button>"
                "<button data-action='folder'><span class='button-label'>OPEN FOLDER</span></button>"
                "</div>";
    }
    if (auto* list = document_->GetElementById("skin-controls"))
        list->SetInnerRML(html.str());
    sliderRatios_.fill(-1.0f);
    swatchColors_.fill("");
}
void NativeSkinUi::Sync() {
    if (!document_ || !skin_ || !ui_) return;
    const bool rebuild = lastPage_ != ui_->page ||
        (ui_->page == 3 && (lastProfilesRevision_ != ui_->profilesRevision ||
                           lastProfileSelection_ != ui_->selectedProfile));
    if (rebuild) {
        RebuildControls();
        lastPage_ = ui_->page;
        lastProfilesRevision_ = ui_->profilesRevision;
        lastProfileSelection_ = ui_->selectedProfile;
    }
    for (int i = 0; i < 4; ++i)
        if (auto* tab = document_->GetElementById(TabId(i)))
            tab->SetClass("selected", i == ui_->page);
    auto* focus = document_->GetContext()->GetFocusElement();
    const auto syncInput = [&](const char* id, const std::string& value) {
        auto* element = dynamic_cast<Rml::ElementFormControlInput*>(
            document_->GetElementById(id));
        if (element && element != focus && element->GetValue() != value)
            element->SetValue(value);
    };
    syncInput("skin-name", skin_->name);
    SetText("skin-dirty", ui_->dirty ? "UNSAVED" : "");
    SetText("skin-notice", ui_->notice);
    if (auto* notice = document_->GetElementById("skin-notice"))
        notice->SetClass("error", ui_->noticeError);
    const bool modal = !ui_->pendingDeleteProfile.empty();
    if (auto* elem = document_->GetElementById("skin-delete-modal"))
        elem->SetProperty("display", modal ? "flex" : "none");
    if (modal != lastModal_) lastModal_ = modal;
    if (modal)
        SetText("skin-delete-title",
            "DELETE " + ui_->pendingDeleteProfile.filename().u8string() + "?");
    if (auto* colorEdit = document_->GetElementById("skin-color-edit"))
        colorEdit->SetProperty("display", ui_->page == 2 ? "block" : "none");
    if (ui_->page == 2) {
        if (lastColor_ != ui_->selectedColor) {
            RebuildControls();
            lastColor_ = ui_->selectedColor;
        }
        syncInput("skin-hex", FormatSkinHex(
            SkinColor(*skin_, ui_->selectedColor)));
    }
    if (ui_->page == 2) {
        for (int i = 0; i < kSkinColorCount; ++i) {
            const std::string id = "skin-swatch-chip-" + std::to_string(i);
            const std::string hex = HexColor(SkinColor(*skin_, i));
            if (hex == swatchColors_[static_cast<std::size_t>(i)]) continue;
            if (auto* chip = document_->GetElementById(id))
                chip->SetProperty("background-color", hex);
            swatchColors_[static_cast<std::size_t>(i)] = hex;
        }
    }
    if (ui_->page < 2) {
        const int count = SkinSettingCount(ui_->page);
        for (int row = 0; row < count; ++row) {
            if (ui_->page < 2 && GetSkinSetting(ui_->page, row)->toggle) {
                // Toggling reuses the native element, preserving focus.
                const std::string id = "skin-toggle-" + std::to_string(row);
                (void)id;
                continue;
            }
            const double value = GetSkinNumber(
                *skin_, ui_->page, row, ui_->selectedColor);
            const std::string name = "skin-number-" + std::to_string(row);
            syncInput(name.c_str(), FormatSkinNumber(value));
            const auto& range = GetSkinSetting(ui_->page, row)->numberRange;
            const float ratio = range.sliderMax > range.sliderMin
                ? static_cast<float>(std::clamp(
                    (value - range.sliderMin) /
                    (range.sliderMax - range.sliderMin), 0.0, 1.0))
                : 0.0f;
            if (std::abs(sliderRatios_[static_cast<std::size_t>(row)] - ratio)
                > 0.00001f) {
                const std::string amount = std::to_string(ratio * 100.0f) + "%";
                const std::string suffix = std::to_string(row);
                if (auto* fill = document_->GetElementById("skin-slider-fill-" + suffix))
                    fill->SetProperty("width", amount);
                if (auto* knob = document_->GetElementById("skin-slider-knob-" + suffix))
                    knob->SetProperty("left", amount);
                sliderRatios_[static_cast<std::size_t>(row)] = ratio;
            }
        }
    }
}

void NativeSkinUi::UpdatePointer(Rectangle wheelBounds) {
    if (!document_ || !skin_ || !ui_) return;
    const bool held = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (!ui_->pendingDeleteProfile.empty()) {
        ui_->draggingRow = -1;
        ui_->draggingColor = 0;
        return;
    }
    const Vector2 mouse = GetMousePosition();
    auto* controls = document_->GetElementById("skin-controls");
    auto* panel = controls ? controls->GetParentNode() : nullptr;
    const Rectangle clip = panel
        ? Rectangle{panel->GetAbsoluteLeft(), panel->GetAbsoluteTop(),
                    static_cast<float>(panel->GetOffsetWidth()),
                    static_cast<float>(panel->GetOffsetHeight())}
        : Rectangle{};
    const bool inPanel = clip.width > 0.0f &&
        CheckCollisionPointRec(mouse, clip);
    if (inPanel && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (ui_->page < 2) {
            for (int row = 0; row < SkinSettingCount(ui_->page); ++row) {
                const auto* setting = GetSkinSetting(ui_->page, row);
                if (!setting || setting->toggle) continue;
                if (auto* slider = document_->GetElementById(
                        "skin-slider-" + std::to_string(row))) {
                    const Rectangle rect{slider->GetAbsoluteLeft(),
                        slider->GetAbsoluteTop(),
                        static_cast<float>(slider->GetOffsetWidth()),
                        static_cast<float>(slider->GetOffsetHeight())};
                    if (CheckCollisionPointRec(mouse, rect)) {
                        ui_->draggingRow = row;
                        break;
                    }
                }
            }
        } else if (ui_->page == 2 && wheelBounds.width > 0.0f &&
                   wheelBounds.height > 0.0f &&
                   CheckCollisionPointRec(mouse, wheelBounds)) {
            BeginSkinColorDrag(
                SkinColorWheelViewport(wheelBounds).ToVirtual(mouse),
                *skin_, *ui_);
        }
    }
    if (!held) {
        ui_->draggingRow = -1;
        ui_->draggingColor = 0;
        return;
    }
    if (ui_->page < 2 && ui_->draggingRow >= 0) {
        const int row = ui_->draggingRow;
        if (auto* track = document_->GetElementById(
                "skin-slider-track-" + std::to_string(row))) {
            const float width = static_cast<float>(track->GetOffsetWidth());
            if (width > 0.0f) {
                const SkinSetting* setting = GetSkinSetting(ui_->page, row);
                if (setting && !setting->toggle) {
                    const double ratio = std::clamp(
                        (mouse.x - track->GetAbsoluteLeft()) / width, 0.0f, 1.0f);
                    const auto& range = setting->numberRange;
                    const double value = range.sliderMin +
                        ratio * (range.sliderMax - range.sliderMin);
                    const double previous = GetSkinNumber(*skin_, ui_->page,
                                                           row, ui_->selectedColor);
                    std::string error;
                    if (SetSkinNumber(*skin_, ui_->page, row, ui_->selectedColor,
                                      value, true, error))
                        ui_->dirty |= previous != GetSkinNumber(*skin_, ui_->page,
                                                                 row, ui_->selectedColor);
                }
            }
        }
    } else if (ui_->page == 2 && ui_->draggingColor > 0) {
        UpdateSkinColorDrag(
            SkinColorWheelViewport(wheelBounds).ToVirtual(mouse), *skin_, *ui_);
    }
}

void NativeSkinUi::CommitText(Rml::ElementFormControlInput& element) {
    if (!skin_ || !ui_) return;
    const std::string kind = element.GetAttribute<Rml::String>("data-kind", "");
    const std::string value = element.GetValue();
    if (kind == "name") {
        Skin candidate = *skin_;
        candidate.name = value;
        std::string error;
        if (!candidate.Valid(error)) {
            ui_->notice = "INVALID SKIN NAME: " + error;
            ui_->noticeError = true;
            return;
        }
        ui_->dirty |= skin_->name != value;
        skin_->name = value;
    } else if (kind == "hex") {
        const auto before = SkinColor(*skin_, ui_->selectedColor);
        Color parsed = before;
        if (!ParseSkinHex(value, before, parsed)) {
            ui_->notice = "HEX MUST BE #RRGGBB OR #RRGGBBAA";
            ui_->noticeError = true;
            return;
        }
        ui_->dirty |= parsed.r != before.r || parsed.g != before.g ||
                      parsed.b != before.b || parsed.a != before.a;
        SkinColor(*skin_, ui_->selectedColor) = parsed;
        const auto hsv = SkinColorToHsv(parsed);
        if (hsv.s > 0.001f) ui_->pickerHue = hsv.h;
        // Swatch colors change with the same authoritative RGBA values.
        RebuildControls();
    } else if (kind == "number") {
        const int row = element.GetAttribute<int>("data-index", -1);
        double parsed = 0.0;
        std::string error;
        if (!ParseSkinNumber(value, parsed) ||
            !SetSkinNumber(*skin_, ui_->page, row, ui_->selectedColor,
                           parsed, false, error)) {
            ui_->notice = error.empty() ? "INVALID SKIN NUMBER" : error;
            ui_->noticeError = true;
            return;
        }
        ui_->dirty = true;
    } else return;
    ui_->notice = "PREVIEW UPDATED - SAVE TO APPLY";
    ui_->noticeError = false;
}
void NativeSkinUi::ProcessEvent(Rml::Event& event) {
    if (!document_ || !ui_ || !skin_ || !input_ || !events_) return;
    auto* target = event.GetTargetElement();
    if (!target) return;
    const std::string type = event.GetType();
    if (type == "change" || type == "input") {
        auto* element = dynamic_cast<Rml::ElementFormControlInput*>(target);
        if (!element) return;
        const std::string kind = element->GetAttribute<Rml::String>("data-kind", "");
        if (type == "change") CommitText(*element);
        return;
    }
    if (type != "click") return;
    while (target && !target->HasAttribute("data-action"))
        target = target->GetParentNode();
    if (!target) return;
    const std::string action =
        target->GetAttribute<Rml::String>("data-action", "");
    if (!ui_->pendingDeleteProfile.empty()) {
        if (action == "delete-confirm")
            events_->skinCommand = SkinCommand::DeleteProfile;
        else if (action == "delete-cancel") {
            ui_->pendingDeleteProfile.clear();
            ui_->notice = "SKIN DELETION CANCELED";
            ui_->noticeError = false;
        }
        return;
    }
    if (action == "tab") {
        ui_->page = std::clamp(target->GetAttribute<int>("data-index", 0), 0, 3);
        ui_->draggingRow = -1;
        ui_->draggingColor = 0;
    } else if (action == "toggle") {
        const int row = target->GetAttribute<int>("data-index", -1);
        if (FlipSkinToggle(*skin_, ui_->page, row)) {
            ui_->dirty = true;
            // The data catalog still owns the value; refresh only the row.
            if (auto* label = target->GetChild(0))
                UpdateRmlText(label, GetSkinToggle(*skin_, ui_->page, row)
                              ? "ON" : "OFF");
        }
    } else if (action == "swatch") {
        const int color = target->GetAttribute<int>("data-index", -1);
        if (color >= 0 && color < kSkinColorCount) {
            ui_->selectedColor = color;
            const SkinHsv hsv = SkinColorToHsv(SkinColor(*skin_, color));
            if (hsv.s > 0.001f) ui_->pickerHue = hsv.h;
        }
    } else if (action == "profile") {
        const int index = target->GetAttribute<int>("data-index", -1);
        if (index >= 0 && index < static_cast<int>(ui_->profiles.size()))
            ui_->selectedProfile = index;
    } else if (action == "delete") {
        const int index = ui_->selectedProfile;
        if (index < 0 || index >= static_cast<int>(ui_->profiles.size())) {
            ui_->notice = "SELECT A PROFILE TO DELETE";
            ui_->noticeError = true;
        } else ui_->pendingDeleteProfile =
            ui_->profiles[static_cast<std::size_t>(index)].path;
    } else if (action == "load") events_->skinCommand = SkinCommand::Load;
    else if (action == "save") events_->skinCommand = SkinCommand::Save;
    else if (action == "saveas") events_->skinCommand = SkinCommand::SaveAs;
    else if (action == "reset") events_->skinCommand = SkinCommand::Reset;
    else if (action == "activate") events_->skinCommand = SkinCommand::ActivateProfile;
    else if (action == "store") events_->skinCommand = SkinCommand::StoreProfile;
    else if (action == "refresh") events_->skinCommand = SkinCommand::RefreshProfiles;
    else if (action == "restore") events_->skinCommand = SkinCommand::RestoreProfile;
    else if (action == "folder") events_->skinCommand = SkinCommand::OpenProfilesFolder;
}
} // namespace locus::app
