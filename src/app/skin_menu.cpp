#include "app/skin_menu.h"
#include "app/skin_menu_layout.h"
#include "core/utf8.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace locus::app {
using namespace skin_detail;
namespace {

void ChangeColorPointer(Vector2 mouse, Skin& skin, SkinUi& ui) {
    if (ui.draggingColor == 0) return;
    Color& color = SkinColor(skin, ui.selectedColor);
    SkinHsv hsv = SkinColorToHsv(color);
    if (hsv.s < 0.0001f) hsv.h = ui.pickerHue;
    if (ui.draggingColor == 1) {
        float angle = std::atan2(mouse.y - SkinColorWheelCenter().y,
                                 mouse.x - SkinColorWheelCenter().x);
        if (angle < 0.0f) angle += 2.0f * kPi;
        ui.pickerHue = angle / (2.0f * kPi);
        hsv.h = ui.pickerHue;
        if (hsv.s < 0.0001f) hsv.s = 0.8f;
        if (hsv.v < 0.001f) hsv.v = 1.0f;
    } else {
        const Rectangle square = SkinColorSquareRect();
        hsv.s = std::clamp((mouse.x - square.x) / square.width, 0.0f, 1.0f);
        hsv.v = 1.0f - std::clamp((mouse.y - square.y) / square.height,
                                 0.0f, 1.0f);
        hsv.h = ui.pickerHue;
    }
    const Color chosen = SkinColorFromHsv(hsv, color.a);
    if (chosen.r != color.r || chosen.g != color.g || chosen.b != color.b) {
        color = chosen;
        ui.dirty = true;
    }
    ui.notice = "PREVIEW UPDATED - SAVE TO APPLY / HEX SUPPORTS ALPHA";
    ui.noticeError = false;
}

bool InWheelRing(Vector2 p) {
    const auto center = SkinColorWheelCenter();
    const float dist = std::hypot(p.x - center.x, p.y - center.y);
    return dist >= kWheelInner && dist <= kWheelOuter;
}

void StartNumberEdit(const Skin& skin, SkinUi& ui, int row) {
    ui.editingNumberRow = row;
    ui.numberBuffer = FormatSkinNumber(
        GetSkinNumber(skin, ui.page, row, ui.selectedColor));
    ui.numberSelectAll = true;
    ui.draggingRow = -1;
    SkinNumberRange range;
    GetSkinNumberRange(ui.page, row, range);
    ui.notice = "INPUT: " + FormatSkinNumber(range.inputMin) + " .. " +
                FormatSkinNumber(range.inputMax) + "  /  ENTER APPLY  /  ESC CANCEL";
    ui.noticeError = false;
}

void SetSliderAt(Vector2 mouse, Skin& skin, SkinUi& ui) {
    if (ui.draggingRow < 0) return;
    SkinNumberRange range;
    if (!GetSkinNumberRange(ui.page, ui.draggingRow, range)) return;
    const Rectangle track = SkinSliderRect(ui.page, ui.draggingRow);
    const double t = std::clamp(
        static_cast<double>((mouse.x - track.x) / track.width), 0.0, 1.0);
    const double next = range.sliderMin + t * (range.sliderMax - range.sliderMin);
    std::string error;
    const double previous = GetSkinNumber(skin, ui.page,
                                          ui.draggingRow, ui.selectedColor);
    if (SetSkinNumber(skin, ui.page, ui.draggingRow, ui.selectedColor,
                      next, true, error) &&
        GetSkinNumber(skin, ui.page, ui.draggingRow, ui.selectedColor) != previous)
        ui.dirty = true;
}

std::string CodepointUtf8(int point) {
    return point < 32 ? std::string{} :
           core::utf8::Encode(static_cast<std::uint32_t>(point));
}

void DeleteLastCodepoint(std::string& text) {
    core::utf8::PopBack(text);
}


} // namespace

Rectangle SkinTabButton(int index) {
    return {52.0f + index * 131.0f, 134.0f, 123.0f, 34.0f};
}
Rectangle SkinActionButton(int index) {
    return {52.0f + index * 135.0f, 473.0f, 125.0f, 34.0f};
}
Rectangle SkinNumberInputRect(int page, int row) {
    return page == 2 ? Rectangle{814.0f, 248.0f + row * 42.0f, 91.0f, 30.0f}
                     : Rectangle{438.0f, 199.0f + row * RowPitch(page), 116.0f, 30.0f};
}
Rectangle SkinSliderRect(int page, int row) {
    return {224.0f, 210.0f + row * RowPitch(page), 196.0f, 6.0f};
}
Rectangle SkinHexRect() { return {342, 411, 190, 35}; }
Rectangle SkinColorSquareRect() { return {388, 274, 82, 82}; }
Vector2 SkinColorWheelCenter() { return {429, 315}; }
Rectangle SkinProfileRow(int row) {
    return {69, 220.0f + row * 37.0f, 482, 32};
}
Rectangle SkinProfileActionRect(int index) {
    return {70.0f + index * 97.0f, 416, 92, 34};
}
Rectangle SkinProfilesFolderRect() { return {377, 184, 177, 28}; }
Rectangle SkinDeleteConfirmRect() { return {283, 310, 169, 42}; }
Rectangle SkinDeleteCancelRect() { return {506, 310, 169, 42}; }


SkinCommand SkinMenuClick(Vector2 mouse, Skin& skin, SkinUi& ui) {
    ui.draggingRow = -1;
    ui.draggingColor = 0;
    for (int i = 0; i < 4; ++i)
        if (Hit(mouse, SkinTabButton(i))) {
            ui.page = i;
            ui.editingNumberRow = -1;
            ui.editingHex = false;
            ui.draggingColor = 0;
            return SkinCommand::None;
        }
    if (Hit(mouse, NameField())) {
        ui.editingName = true;
        ui.nameSelectAll = true;
        ui.previousName = skin.name;
        ui.notice = "TYPE NAME / ENTER APPLY / ESC CANCEL";
        ui.noticeError = false;
        return SkinCommand::None;
    }
    for (int i = 0; i < 4; ++i)
        if (Hit(mouse, SkinActionButton(i)))
            return static_cast<SkinCommand>(static_cast<int>(SkinCommand::Load) + i);

    if (ui.page == 3) {
        if (Hit(mouse, SkinProfilesFolderRect()))
            return SkinCommand::OpenProfilesFolder;
        for (int row=0; row<5; ++row)
            if (ui.profileOffset + row < static_cast<int>(ui.profiles.size()) &&
                Hit(mouse, SkinProfileRow(row))) {
                ui.selectedProfile = ui.profileOffset + row;
                return SkinCommand::None;
            }
        if (Hit(mouse, SkinProfileActionRect(0))) return SkinCommand::ActivateProfile;
        if (Hit(mouse, SkinProfileActionRect(1))) return SkinCommand::StoreProfile;
        if (Hit(mouse, SkinProfileActionRect(2)))
            return SkinCommand::RefreshProfiles;
        if (Hit(mouse, SkinProfileActionRect(3))) {
            if (ui.selectedProfile < 0 ||
                ui.selectedProfile >= static_cast<int>(ui.profiles.size())) {
                ui.notice = "SELECT A PERSONAL SKIN TO DELETE";
                ui.noticeError = true;
            } else {
                ui.pendingDeleteProfile =
                    ui.profiles[static_cast<std::size_t>(ui.selectedProfile)].path;
                ui.noticeError = false;
            }
            return SkinCommand::None;
        }
        if (Hit(mouse, SkinProfileActionRect(4)))
            return SkinCommand::RestoreProfile;
        return SkinCommand::None;
    }
    if (ui.page == 2) {
        for (int i = 0; i < kSkinColorCount; ++i)
            if (Hit(mouse, ColorRow(i))) {
                ui.selectedColor = i;
                const SkinHsv hsv = SkinColorToHsv(SkinColor(skin, i));
                if (hsv.s >= 0.001f) ui.pickerHue = hsv.h;
                return SkinCommand::None;
            }
        if (Hit(mouse, SkinHexRect())) {
            ui.editingHex = true;
            ui.hexSelectAll = true;
            ui.hexBuffer = FormatSkinHex(SkinColor(skin, ui.selectedColor));
            ui.notice = "TYPE #RRGGBB OR #RRGGBBAA / ENTER APPLY";
            ui.noticeError = false;
            return SkinCommand::None;
        }
        if (InWheelRing(mouse)) ui.draggingColor = 1;
        else if (Hit(mouse, SkinColorSquareRect())) ui.draggingColor = 2;
        if (ui.draggingColor) {
            ChangeColorPointer(mouse, skin, ui);
            return SkinCommand::None;
        }
        return SkinCommand::None;
    } else {
        for (int row = 0; row < RowCount(ui.page); ++row) {
            SkinNumberRange range;
            if (GetSkinNumberRange(ui.page, row, range)) continue;
            if (Hit(mouse, {69.0f, 199.0f + row * RowPitch(ui.page),
                            489.0f, 30.0f})) {
                FlipToggle(skin, ui.page, row);
                ui.dirty = true;
                return SkinCommand::None;
            }
        }
    }
    const int count = RowCount(ui.page);
    for (int row = 0; row < count; ++row) {
        SkinNumberRange range;
        if (!GetSkinNumberRange(ui.page, row, range)) continue;
        if (Hit(mouse, SkinNumberInputRect(ui.page, row))) {
            StartNumberEdit(skin, ui, row);
            return SkinCommand::None;
        }
        if (Hit(mouse, SliderHit(SkinSliderRect(ui.page, row)))) {
            ui.draggingRow = row;
            SetSliderAt(mouse, skin, ui);
            ui.notice = "DRAG SLIDER / TYPE IN FIELD FOR EXTENDED VALUES";
            ui.noticeError = false;
            return SkinCommand::None;
        }
    }
    return SkinCommand::None;
}

void UpdateSkinSliderDrag(Vector2 mouse, bool leftHeld, Skin& skin, SkinUi& ui) {
    if (!leftHeld) {
        ui.draggingRow = -1;
        ui.draggingColor = 0;
        return;
    }
    if (ui.page == 2 && ui.draggingColor && !SkinMenuEditingText(ui)) {
        ChangeColorPointer(mouse, skin, ui);
        return;
    }
    if (ui.draggingRow >= 0 && !SkinMenuEditingText(ui))
        SetSliderAt(mouse, skin, ui);
}

bool SkinMenuEditingText(const SkinUi& ui) noexcept {
    return ui.editingName || ui.editingNumberRow >= 0 || ui.editingHex;
}

bool FinishSkinTextInput(Skin& skin, SkinUi& ui, bool apply) {
    if (ui.editingName) {
        if (apply) {
            std::string error;
            if (!skin.Valid(error)) {
                ui.notice = "INVALID NAME: " + error;
                ui.noticeError = true;
                return false;
            }
            ui.dirty |= skin.name != ui.previousName;
        } else {
            skin.name = ui.previousName;
        }
        ui.editingName = false;
        ui.nameSelectAll = false;
    } else if (ui.editingHex) {
        if (apply) {
            const Color old = SkinColor(skin, ui.selectedColor);
            Color parsed = old;
            if (!ParseSkinHex(ui.hexBuffer, old, parsed)) {
                ui.notice = "INVALID HEX - USE #RRGGBB OR #RRGGBBAA";
                ui.noticeError = true;
                return false;
            }
            ui.dirty |= parsed.r != old.r || parsed.g != old.g ||
                        parsed.b != old.b || parsed.a != old.a;
            SkinColor(skin, ui.selectedColor) = parsed;
            const SkinHsv hsv = SkinColorToHsv(parsed);
            if (hsv.s > 0.001f) ui.pickerHue = hsv.h;
        }
        ui.editingHex = false;
        ui.hexBuffer.clear();
        ui.hexSelectAll = false;
    } else if (ui.editingNumberRow >= 0) {
        if (apply) {
            double number = 0.0;
            if (!ParseSkinNumber(ui.numberBuffer, number)) {
                ui.notice = "INVALID NUMBER - ENTER A FINITE NUMERIC VALUE";
                ui.noticeError = true;
                return false;
            }
            const double previous = GetSkinNumber(
                skin, ui.page, ui.editingNumberRow, ui.selectedColor);
            std::string error;
            if (!SetSkinNumber(skin, ui.page, ui.editingNumberRow,
                               ui.selectedColor, number, false, error)) {
                ui.notice = error;
                ui.noticeError = true;
                return false;
            }
            ui.dirty |= GetSkinNumber(skin, ui.page, ui.editingNumberRow,
                                      ui.selectedColor) != previous;
        }
        ui.editingNumberRow = -1;
        ui.numberBuffer.clear();
        ui.numberSelectAll = false;
    }
    ui.noticeError = false;
    ui.notice = apply ? "VALUE APPLIED TO PREVIEW - SAVE TO USE IN PLAY"
                      : "EDIT CANCELED";
    return true;
}

void HandleSkinTextInput(Skin& skin, SkinUi& ui) {
    if (!SkinMenuEditingText(ui)) return;
    if (IsKeyPressed(KEY_ESCAPE)) {
        FinishSkinTextInput(skin, ui, false);
        return;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        FinishSkinTextInput(skin, ui, true);
        return;
    }
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && ui.editingName && IsKeyPressed(KEY_V)) {
        if (const char* clipboard = GetClipboardText()) {
            std::string next = ui.nameSelectAll ? "" : skin.name;
            if (core::utf8::AppendLimited(next, clipboard, 64, 256)) {
                skin.name = std::move(next);
                ui.nameSelectAll = false;
            }
        }
        return;
    }
    if (ctrl && ui.editingHex && IsKeyPressed(KEY_V)) {
        const char* clipboard = GetClipboardText();
        if (clipboard) {
            Color parsed{};
            if (ParseSkinHex(clipboard, SkinColor(skin, ui.selectedColor), parsed)) {
                ui.hexBuffer = clipboard;
                ui.hexSelectAll = false;
                ui.notice = "HEX PASTED - ENTER TO APPLY";
                ui.noticeError = false;
            } else {
                ui.notice = "CLIPBOARD DOES NOT CONTAIN #RRGGBB OR #RRGGBBAA";
                ui.noticeError = true;
            }
        }
        return;
    }
    if (ctrl && ui.editingHex && IsKeyPressed(KEY_C)) {
        SetClipboardText(FormatSkinHex(SkinColor(skin, ui.selectedColor)).c_str());
        ui.notice = "CURRENT HEX COLOR COPIED";
        ui.noticeError = false;
        return;
    }
    if (ctrl && IsKeyPressed(KEY_A)) {
        if (ui.editingName) ui.nameSelectAll = true;
        else if (ui.editingHex) ui.hexSelectAll = true;
        else ui.numberSelectAll = true;
        return;
    }

    std::string& buffer = ui.editingName ? skin.name
        : ui.editingHex ? ui.hexBuffer : ui.numberBuffer;
    bool& selected = ui.editingName ? ui.nameSelectAll
        : ui.editingHex ? ui.hexSelectAll : ui.numberSelectAll;
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (selected) buffer.clear();
        else DeleteLastCodepoint(buffer);
        selected = false;
    }
    int point = 0;
    while ((point = GetCharPressed()) != 0) {
        if (ctrl) continue;
        const std::string character = CodepointUtf8(point);
        if (character.empty()) continue;
        if (ui.editingHex) {
            if (character.find_first_not_of("#0123456789aAbBcCdDeEfF") !=
                std::string::npos) continue;
        } else if (!ui.editingName &&
                   character.find_first_not_of("0123456789+-.,eE") !=
                   std::string::npos) continue;
        if (ui.editingName) {
            std::string next = selected ? "" : buffer;
            if (core::utf8::AppendLimited(next, character, 64, 256)) {
                buffer = std::move(next);
                selected = false;
            }
        } else {
            const std::size_t limit = ui.editingHex ? 9 : 32;
            const std::size_t used = selected ? 0 : buffer.size();
            if (used + character.size() > limit) continue;
            if (selected) buffer.clear();
            buffer += character;
            selected = false;
        }
    }
}

} // namespace locus::app
