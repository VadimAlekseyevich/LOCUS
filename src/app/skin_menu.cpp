#include "app/skin_menu.h"

#include "app/menu_ui.h"
#include "app/skin_controls.h"
#include "app/visual_style.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace locus::app {
namespace {
constexpr Color kText{225, 232, 240, 255};
constexpr Color kMuted{159, 181, 201, 255};
constexpr Color kAccent{183, 166, 243, 255};
constexpr Color kGold{244, 210, 137, 255};
constexpr Color kPanel{26, 30, 42, 255};

void Label(Font font, const std::string& label, float x, float y,
           float size = 16.0f, Color color = kText) {
    DrawTextEx(font, label.c_str(), {x, y}, size, 1.0f, color);
}

void Button(Font font, Rectangle r, const char* text, bool active = false) {
    DrawRectangleRounded(r, 0.12f, 4,
                         active ? Color{49, 69, 82, 255}
                                : Color{35, 41, 56, 255});
    DrawRectangleRoundedLinesEx(r, 0.12f, 4, 1.3f,
                                active ? kGold : kAccent);
    const auto size = MeasureTextEx(font, text, 15.0f, 1.0f);
    Label(font, text, r.x + (r.width - size.x) * 0.5f,
          r.y + (r.height - size.y) * 0.5f, 15.0f);
}

bool Hit(Vector2 mouse, Rectangle rect) {
    return CheckCollisionPointRec(mouse, rect);
}

Rectangle ColorRow(int row) {
    // Ten colours must fit above the skin action row at y=473.
    return {70.0f, 194.0f + row * 25.0f, 485.0f, 24.0f};
}
Rectangle NameField() { return {615.0f, 128.0f, 302.0f, 36.0f}; }
Rectangle SwitchRect(int row) {
    return {468.0f, 200.0f + row * 36.0f, 84.0f, 28.0f};
}

const char* RowName(int page, int row) {
    static constexpr const char* path[] = {
        "LOOKAHEAD (BEATS)", "PATH WIDTH", "OUTLINE SCALE",
        "FADE CURVE", "PATH OPACITY", "TURN SIZE", "TURN GLOW"
    };
    static constexpr const char* player[] = {
        "PLAYER SIZE", "CAMERA ZOOM", "PLAYER GLOW",
        "HIT FLASH SIZE", "SHOW HUD"
    };
    return page == 0 ? path[row] : player[row];
}

int RowCount(int page) { return page == 0 ? 7 : 5; }

bool ToggleValue(const Skin& skin, int page, int row) {
    return page == 0 ? skin.junctionGlow
         : row == 2 ? skin.playerGlow : skin.showHud;
}

void FlipToggle(Skin& skin, int page, int row) {
    if (page == 0 && row == 6) skin.junctionGlow = !skin.junctionGlow;
    else if (page == 1 && row == 2) skin.playerGlow = !skin.playerGlow;
    else if (page == 1 && row == 4) skin.showHud = !skin.showHud;
}

void DrawSwitch(Font font, Rectangle rect, bool enabled) {
    const Color fill = enabled ? Color{65, 114, 113, 255}
                               : Color{63, 67, 81, 255};
    DrawRectangleRounded(rect, 0.5f, 8, fill);
    DrawCircleV({enabled ? rect.x + rect.width - 16.0f : rect.x + 16.0f,
                 rect.y + rect.height * 0.5f}, 10.5f, kText);
    Label(font, enabled ? "ON" : "OFF", enabled ? rect.x + 10.0f : rect.x + 38.0f,
          rect.y + 6.0f, 13.0f, enabled ? Color{209, 246, 229, 255} : kMuted);
}

void Panel(Rectangle area) {
    DrawRectangleRounded(area, 0.05f, 6, kPanel);
    DrawRectangleRoundedLinesEx(area, 0.05f, 6, 1.0f,
                                Color{51, 62, 78, 255});
}

std::string FitText(Font font, std::string text, float width, float fontSize) {
    // Display the tail of a long edit buffer without cutting a UTF-8 codepoint.
    while (!text.empty() &&
           MeasureTextEx(font, text.c_str(), fontSize, 1.0f).x > width) {
        std::size_t advance = 1;
        while (advance < text.size() &&
               (static_cast<unsigned char>(text[advance]) & 0xc0) == 0x80)
            ++advance;
        text.erase(0, advance);
    }
    return text;
}

void DrawInput(Font font, Rectangle rect, const std::string& value,
               bool focused, bool selected = false) {
    DrawRectangleRounded(rect, 0.14f, 5,
                         focused ? Color{53, 62, 83, 255}
                                 : Color{31, 38, 52, 255});
    DrawRectangleRoundedLinesEx(rect, 0.14f, 5, focused ? 2.0f : 1.0f,
                                focused ? kGold : Color{89, 103, 119, 255});
    const std::string shown = FitText(font, value, rect.width - 17.0f, 15.0f);
    if (focused && selected) {
        DrawRectangle(static_cast<int>(rect.x + 6.0f),
                      static_cast<int>(rect.y + 5.0f),
                      static_cast<int>(rect.width - 12.0f), 21,
                      Color{85, 80, 121, 255});
    }
    Label(font, shown, rect.x + 8.0f, rect.y + 7.0f, 15.0f, kText);
    if (focused && !selected) {
        const float caretX = std::min(rect.x + rect.width - 7.0f,
            rect.x + 10.0f + MeasureTextEx(font, shown.c_str(), 15.0f, 1.0f).x);
        DrawLineV({caretX, rect.y + 6.0f},
                  {caretX, rect.y + rect.height - 6.0f}, kGold);
    }
}

Rectangle SliderHit(Rectangle rect) {
    return {rect.x - 7.0f, rect.y - 12.0f, rect.width + 14.0f, 30.0f};
}

void DrawSlider(Rectangle track, double value, const SkinNumberRange& range) {
    const double t = std::clamp(
        (value - range.sliderMin) / (range.sliderMax - range.sliderMin), 0.0, 1.0);
    DrawRectangleRounded(track, 0.9f, 5, Color{61, 70, 88, 255});
    DrawRectangleRounded({track.x, track.y, track.width * static_cast<float>(t),
                          track.height}, 0.9f, 5, kAccent);
    const Vector2 knob{track.x + track.width * static_cast<float>(t),
                       track.y + track.height * 0.5f};
    const bool outside = value < range.sliderMin || value > range.sliderMax;
    DrawCircleV(knob, 8.0f, outside ? kGold : kText);
    DrawCircleV(knob, 3.0f, Color{75, 69, 108, 255});
}

void PreviewPath(Font font, const Skin& skin) {
    const Rectangle area{605.0f, 193.0f, 298.0f, 248.0f};
    DrawRectangleRounded(area, 0.07f, 6, skin.background);
    DrawRectangleRoundedLinesEx(area, 0.07f, 6, 1.2f, kMuted);
    Label(font, "LIVE PREVIEW", 622, 206, 14.0f, kMuted);
    const auto point = [&](float beats) {
        const float progress = beats / skin.lookaheadBeats;
        return Vector2{630.0f + 241.0f * progress,
                       336.0f - 46.0f * std::sin(progress * 5.2f)};
    };
    for (int i = 95; i >= 0; --i) {
        const float a = skin.lookaheadBeats * i / 96.0f;
        const float b = skin.lookaheadBeats * (i + 1) / 96.0f;
        const float ahead = (a + b) * 0.5f;
        const float opacity = ScoredPathOpacity(ahead, skin.lookaheadBeats,
                                               skin.fadePower) * skin.pathOpacity;
        const float width = ScoredPathWidth(ahead, skin.lookaheadBeats,
                                            skin.pathWidth, skin.fadePower);
        DrawLineEx(point(a), point(b), width * skin.outlineScale,
                   WithOpacity(skin.outline, opacity));
        DrawLineEx(point(a), point(b), width,
                   WithOpacity(skin.trajectory, opacity));
    }
    const Vector2 marker = point(0.0f);
    const float radius = skin.playerRadius * 0.7f;
    if (skin.playerGlow)
        DrawCircleV(marker, radius * 1.65f, WithOpacity(skin.player, 0.2f));
    DrawCircleV(marker, radius, skin.player);
    DrawCircleV(marker, radius * 0.33f, skin.playerCore);
    DrawRectangleLinesEx({632.0f, 224.0f, 220.0f, 175.0f},
                         1.5f, skin.fieldBorder);
    Label(font, "SLIDER: EVERYDAY RANGE", 618, 402, 12, kMuted);
    Label(font, "TYPE: EXTENDED VALUES", 618, 420, 12, kMuted);
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
    if (point < 32 || point > 0x10ffff ||
        (point >= 0xd800 && point <= 0xdfff)) return {};
    std::string utf8;
    if (point < 0x80) utf8 += static_cast<char>(point);
    else if (point < 0x800) {
        utf8 += static_cast<char>(0xc0 | (point >> 6));
        utf8 += static_cast<char>(0x80 | (point & 0x3f));
    } else if (point < 0x10000) {
        utf8 += static_cast<char>(0xe0 | (point >> 12));
        utf8 += static_cast<char>(0x80 | ((point >> 6) & 0x3f));
        utf8 += static_cast<char>(0x80 | (point & 0x3f));
    } else {
        utf8 += static_cast<char>(0xf0 | (point >> 18));
        utf8 += static_cast<char>(0x80 | ((point >> 12) & 0x3f));
        utf8 += static_cast<char>(0x80 | ((point >> 6) & 0x3f));
        utf8 += static_cast<char>(0x80 | (point & 0x3f));
    }
    return utf8;
}

void DeleteLastCodepoint(std::string& text) {
    while (!text.empty() &&
           (static_cast<unsigned char>(text.back()) & 0xc0) == 0x80)
        text.pop_back();
    if (!text.empty()) text.pop_back();
}

} // namespace

Rectangle SkinTabButton(int index) {
    return {52.0f + index * 158.0f, 134.0f, 147.0f, 34.0f};
}
Rectangle SkinActionButton(int index) {
    return {52.0f + index * 135.0f, 473.0f, 125.0f, 34.0f};
}
Rectangle SkinNumberInputRect(int page, int row) {
    return page == 2 ? Rectangle{814.0f, 248.0f + row * 42.0f, 91.0f, 30.0f}
                     : Rectangle{438.0f, 199.0f + row * 36.0f, 116.0f, 30.0f};
}
Rectangle SkinSliderRect(int page, int row) {
    return page == 2 ? Rectangle{687.0f, 259.0f + row * 42.0f, 113.0f, 6.0f}
                     : Rectangle{224.0f, 210.0f + row * 36.0f, 196.0f, 6.0f};
}

void DrawSkinMenu(Font font, const Skin& skin, const SkinUi& ui) {
    Label(font, "SKIN  /  GAMEPLAY APPEARANCE", 52, 88, 27, kAccent);
    Button(font, MenuBackButton(), "BACK / CANCEL");
    const char* tabs[] = {"PATH", "PLAYER / HUD", "COLORS"};
    for (int i = 0; i < 3; ++i)
        Button(font, SkinTabButton(i), tabs[i], ui.page == i);

    Panel({52, 181, 519, 276});
    Panel({590, 181, 329, 276});
    if (ui.page != 2) {
        for (int row = 0; row < RowCount(ui.page); ++row) {
            Label(font, RowName(ui.page, row), 70.0f, 206.0f + row * 36.0f,
                  14.0f, kMuted);
            SkinNumberRange range;
            if (GetSkinNumberRange(ui.page, row, range)) {
                const double value = GetSkinNumber(skin, ui.page, row, ui.selectedColor);
                DrawSlider(SkinSliderRect(ui.page, row), value, range);
                const bool editing = ui.editingNumberRow == row;
                DrawInput(font, SkinNumberInputRect(ui.page, row),
                          editing ? ui.numberBuffer : FormatSkinNumber(value),
                          editing, editing && ui.numberSelectAll);
            } else {
                DrawSwitch(font, SwitchRect(row), ToggleValue(skin, ui.page, row));
            }
        }
        PreviewPath(font, skin);
    } else {
        for (int index = 0; index < kSkinColorCount; ++index) {
            const Rectangle row = ColorRow(index);
            if (index == ui.selectedColor)
                DrawRectangleRounded(row, 0.08f, 3, Color{43, 59, 72, 255});
            Label(font, SkinColorLabel(index), row.x + 9, row.y + 5, 13,
                  index == ui.selectedColor ? kGold : kText);
            DrawRectangle(static_cast<int>(row.x + 393),
                          static_cast<int>(row.y + 4), 62, 17,
                          SkinColor(skin, index));
        }
        Label(font, SkinColorLabel(ui.selectedColor), 611, 198, 17, kAccent);
        const char* labels[] = {"R", "G", "B", "A"};
        for (int i = 0; i < 4; ++i) {
            Label(font, labels[i], 612, 255.0f + i * 42.0f, 16, kMuted);
            SkinNumberRange range;
            GetSkinNumberRange(2, i, range);
            const double value = GetSkinNumber(skin, 2, i, ui.selectedColor);
            DrawSlider(SkinSliderRect(2, i), value, range);
            const bool editing = ui.editingNumberRow == i;
            DrawInput(font, SkinNumberInputRect(2, i),
                      editing ? ui.numberBuffer : FormatSkinNumber(value),
                      editing, editing && ui.numberSelectAll);
        }
        Label(font, "RGBA: 0 .. 255", 613, 427, 13, kMuted);
    }
    Label(font, "SKIN NAME", 616, 111, 12, kMuted);
    DrawInput(font, NameField(), skin.name, ui.editingName,
              ui.editingName && ui.nameSelectAll);
    Label(font, ui.dirty ? "UNSAVED CHANGES" : "ACTIVE PROFILE",
          630, 171, 12, ui.dirty ? kGold : kMuted);
    const char* actions[] = {"LOAD", "SAVE", "SAVE AS", "RESET"};
    for (int i = 0; i < 4; ++i)
        Button(font, SkinActionButton(i), actions[i]);
    Label(font, ui.notice.substr(0, 112), 53, 515, 13,
          ui.noticeError ? Color{245, 150, 150, 255} : kGold);
}

SkinCommand SkinMenuClick(Vector2 mouse, Skin& skin, SkinUi& ui) {
    ui.draggingRow = -1;
    for (int i = 0; i < 3; ++i)
        if (Hit(mouse, SkinTabButton(i))) {
            ui.page = i;
            ui.editingNumberRow = -1;
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

    if (ui.page == 2) {
        for (int i = 0; i < kSkinColorCount; ++i)
            if (Hit(mouse, ColorRow(i))) {
                ui.selectedColor = i;
                return SkinCommand::None;
            }
    } else {
        for (int row = 0; row < RowCount(ui.page); ++row) {
            SkinNumberRange range;
            if (GetSkinNumberRange(ui.page, row, range)) continue;
            if (Hit(mouse, {69.0f, 199.0f + row * 36.0f, 489.0f, 30.0f})) {
                FlipToggle(skin, ui.page, row);
                ui.dirty = true;
                return SkinCommand::None;
            }
        }
    }
    const int count = ui.page == 2 ? 4 : RowCount(ui.page);
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
        return;
    }
    if (ui.draggingRow >= 0 && !SkinMenuEditingText(ui))
        SetSliderAt(mouse, skin, ui);
}

bool SkinMenuEditingText(const SkinUi& ui) noexcept {
    return ui.editingName || ui.editingNumberRow >= 0;
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
    if (ctrl && IsKeyPressed(KEY_A)) {
        if (ui.editingName) ui.nameSelectAll = true;
        else ui.numberSelectAll = true;
        return;
    }

    std::string& buffer = ui.editingName ? skin.name : ui.numberBuffer;
    bool& selected = ui.editingName ? ui.nameSelectAll : ui.numberSelectAll;
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
        if (!ui.editingName && character.find_first_not_of("0123456789+-.,eE")
                                != std::string::npos) continue;
        const std::size_t limit = ui.editingName ? 64 : 32;
        const std::size_t used = selected ? 0 : buffer.size();
        if (used + character.size() > limit) continue;
        if (selected) buffer.clear();
        buffer += character;
        selected = false;
    }
}

} // namespace locus::app
