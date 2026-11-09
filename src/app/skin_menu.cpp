#include "app/skin_menu.h"
#include "core/utf8.h"

#include "app/menu_ui.h"
#include "app/skin_controls.h"
#include "app/route_renderer.h"
#include "app/visual_style.h"
#include "app/viewport.h"

#include <algorithm>
#include <cstdio>
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
    return {70.0f, 194.0f + row * 25.0f, 205.0f, 24.0f};
}
Rectangle NameField() { return {615.0f, 128.0f, 302.0f, 36.0f}; }
constexpr float RowPitch(int page) noexcept {
    return page == 0 ? 31.0f : 36.0f;
}
Rectangle SwitchRect(int page, int row) {
    return {468.0f, 200.0f + row * RowPitch(page), 84.0f, 28.0f};
}

const char* RowName(int page, int row) {
    static constexpr const char* path[] = {
        "PREVIEW TIME (MS)", "PATH WIDTH", "OUTLINE SCALE",
        "FADE CURVE", "PATH OPACITY", "TURN SIZE", "TURN GLOW",
        "REVERSE CAP SIZE"
    };
    static constexpr const char* player[] = {
        "PLAYER SIZE", "SPEED (PX/S)", "PLAYER GLOW",
        "HIT FLASH SIZE", "SHOW HUD"
    };
    return page == 0 ? path[row] : player[row];
}

int RowCount(int page) { return page == 0 ? 8 : 5; }

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
    const Rectangle frame{641.0f, 220.0f, 191.0f, 200.0f};
    DrawRectangleRounded(area, 0.07f, 6, skin.background);
    DrawRectangleRoundedLinesEx(area, 0.07f, 6, 1.2f, kMuted);
    Label(font, "SPEED PREVIEW / 120 BPM", 622, 202, 14.0f, kMuted);

    // The SKIN preview clips exactly like the real centred gameplay window,
    // including thick outline strokes and partially visible junctions.
    const auto clip = ViewportTransform::Menu(
        GetScreenWidth(), GetScreenHeight()).Scissor(frame);
    BeginScissorMode(clip.x, clip.y, clip.width, clip.height);
    const float pixelsPerBeat =
        StepForFixedSpeed(skin.baseSpeedPxPerSecond, 120.0, 1) *
        frame.width / kVirtualHeight;
    const float horizon = static_cast<float>(
        ScoredPreviewTicks(skin.lookaheadMs, 120.0, 1));
    const auto point = [&](float beats) {
        const float progress = beats / horizon;
        return Vector2{687.0f + beats * pixelsPerBeat,
                       335.0f - 46.0f * std::sin(progress * 5.2f)};
    };
    for (int i = 95; i >= 0; --i) {
        const float a = horizon * i / 96.0f;
        const float b = horizon * (i + 1) / 96.0f;
        const float ahead = (a + b) * 0.5f;
        const float opacity = ScoredPathOpacity(ahead, horizon,
                                               skin.fadePower) * skin.pathOpacity;
        const float width = ScoredPathWidth(ahead, horizon,
                                            skin.pathWidth, skin.fadePower);
        DrawLineEx(point(a), point(b), width * skin.outlineScale,
                   WithOpacity(skin.outline, opacity));
        DrawLineEx(point(a), point(b), width,
                   WithOpacity(skin.trajectory, opacity));
    }
    const Vector2 marker = point(0.0f);
    const float radius = skin.playerRadius * 0.7f;
    DrawCircleV(marker, std::max(radius + 3.0f, radius * skin.hitFlashScale * 1.5f),
                WithOpacity(skin.hitFlash, 0.32f));
    if (skin.playerGlow)
        DrawCircleV(marker, radius * 1.65f, WithOpacity(skin.player, 0.2f));
    DrawCircleV(marker, radius, skin.player);
    DrawCircleV(marker, radius * 0.33f, skin.playerCore);

    // Show the actual adjustable semicircular end cap on an overlapping
    // right -> left route segment. The same helper draws PLAY and EDITOR.
    const Vector2 turnPoint{784.0f, 385.0f};
    const float proximity = ScoredPathOpacity(
        horizon * 0.35f, horizon, skin.fadePower);
    const float opacity = proximity * skin.pathOpacity;
    const float junctionRadius = std::min(11.0f, skin.junctionRadius * 0.7f)
        * ScoredTurnSize(proximity);
    DrawLineEx({723.0f, turnPoint.y}, turnPoint,
               std::max(1.0f, skin.pathWidth * 0.75f),
               WithOpacity(skin.trajectory, opacity));
    DrawReverseCap(turnPoint, junctionRadius, core::Heading::Left,
                   skin.junction, opacity, skin.junctionGlow,
                   skin.reverseCapScale);
    DrawCircleV(turnPoint, junctionRadius,
                WithOpacity(skin.junction, opacity));
    DrawCircleV(turnPoint, junctionRadius * 0.35f,
                WithOpacity(skin.playerCore, opacity));
    EndScissorMode();

    DrawRectangleLinesEx({frame.x + 0.75f, frame.y + 0.75f,
                          frame.width - 1.5f, frame.height - 1.5f},
                         1.5f, skin.fieldBorder);
    // The preview must also reflect the HUD and hit-flash palette entries,
    // not only the route and marker, so each color has a visible example.
    if (skin.showHud) {
        const Rectangle hud{836, 255, 63, 104};
        DrawRectangleRounded(hud, 0.1f, 4, skin.hudBackground);
        Label(font, "ACC", 841, 263, 11, skin.hudText);
        Label(font, "98.5%", 839, 282, 11, skin.hudText);
        Label(font, "PERF", 841, 310, 10, skin.hudText);
        Label(font, "42", 845, 326, 13, skin.hudText);
    } else {
        Label(font, "HUD", 839, 280, 11, kMuted);
        Label(font, "OFF", 840, 296, 11, kMuted);
    }
    Label(font, "PLAY: [ / ] OR NUM - / +  (10 PX/S)", 610, 425, 11.0f, kMuted);
}

constexpr float kWheelInner = 64.0f;
constexpr float kWheelOuter = 84.0f;
constexpr float kPi = 3.14159265358979323846f;

void DrawColorPicker(Font font, const Skin& skin, const SkinUi& ui) {
    const Vector2 center = SkinColorWheelCenter();
    const Color current = SkinColor(skin, ui.selectedColor);
    SkinHsv hsv = SkinColorToHsv(current);
    if (hsv.s < 0.0001f) hsv.h = ui.pickerHue;
    // Native raylib primitives draw this wheel. No external GUI framework,
    // texture files, or R/G/B/A sliders are needed.
    for (int i = 0; i < 120; ++i) {
        const float hue = i / 120.0f;
        const Color tint = SkinColorFromHsv({hue, 1.0f, 1.0f});
        DrawRing(center, kWheelInner, kWheelOuter,
                 hue * 360.0f + 90.0f,
                 (hue + 1.0f / 120.0f) * 360.0f + 90.0f, 2, tint);
    }
    const Vector2 marker{
        center.x + 74.0f * std::cos(hsv.h * 2.0f * kPi),
        center.y + 74.0f * std::sin(hsv.h * 2.0f * kPi)
    };
    DrawCircleV(marker, 7.0f, kText);
    DrawCircleV(marker, 4.0f, SkinColorFromHsv({hsv.h, 1.0f, 1.0f}));

    const Rectangle square = SkinColorSquareRect();
    constexpr int kSteps = 24;
    const float dx = square.width / kSteps;
    const float dy = square.height / kSteps;
    for (int y = 0; y < kSteps; ++y) {
        for (int x = 0; x < kSteps; ++x) {
            const float saturation = (x + 0.5f) / kSteps;
            const float value = 1.0f - (y + 0.5f) / kSteps;
            DrawRectangleRec({square.x + x * dx, square.y + y * dy,
                              dx + 0.2f, dy + 0.2f},
                             SkinColorFromHsv({hsv.h, saturation, value}));
        }
    }
    DrawRectangleLinesEx(square, 1.0f, kText);
    const Vector2 selection{square.x + hsv.s * square.width,
                            square.y + (1.0f - hsv.v) * square.height};
    DrawCircleV(selection, 5.0f, kPanel);
    DrawCircleLines(static_cast<int>(selection.x),
                    static_cast<int>(selection.y), 5.0f, kText);
    Label(font, "HEX / ALPHA", 347, 389, 14, kMuted);
    DrawInput(font, SkinHexRect(),
              ui.editingHex ? ui.hexBuffer : FormatSkinHex(current),
              ui.editingHex, ui.editingHex && ui.hexSelectAll);
    Label(font, "6 DIGITS=RGB  8=RGBA", 334, 449, 11, kMuted);
}

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
Rectangle SkinDeleteConfirmRect() { return {283, 310, 169, 42}; }
Rectangle SkinDeleteCancelRect() { return {506, 310, 169, 42}; }

void DrawSkinMenu(Font font, const Skin& skin, const SkinUi& ui) {
    Label(font, "SKIN  /  GAMEPLAY APPEARANCE", 52, 88, 27, kAccent);
    Button(font, MenuBackButton(), "BACK / CANCEL");
    const char* tabs[] = {"PATH", "PLAYER", "COLORS", "MY SKINS"};
    for (int i = 0; i < 4; ++i)
        Button(font, SkinTabButton(i), tabs[i], ui.page == i);

    Panel({52, 181, 519, 276});
    Panel({590, 181, 329, 276});
    if (ui.page == 0 || ui.page == 1) {
        for (int row = 0; row < RowCount(ui.page); ++row) {
            Label(font, RowName(ui.page, row), 70.0f,
                   206.0f + row * RowPitch(ui.page),
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
                DrawSwitch(font, SwitchRect(ui.page, row),
                           ToggleValue(skin, ui.page, row));
            }
        }
        PreviewPath(font, skin);
    } else if (ui.page == 2) {
        for (int index = 0; index < kSkinColorCount; ++index) {
            const Rectangle row = ColorRow(index);
            if (index == ui.selectedColor)
                DrawRectangleRounded(row, 0.08f, 3, Color{43, 59, 72, 255});
            Label(font, SkinColorLabel(index), row.x + 7, row.y + 5, 12,
                  index == ui.selectedColor ? kGold : kText);
            DrawRectangle(static_cast<int>(row.x + 158),
                          static_cast<int>(row.y + 4), 38, 17,
                          SkinColor(skin, index));
        }
        DrawColorPicker(font, skin, ui);
        PreviewPath(font, skin); // Always visible on COLORS as well.
    } else {
        Label(font, "PERSONAL SKINS / PROFILES", 69, 195, 18, kGold);
        if (ui.profiles.empty())
            Label(font, "NO SAVED PROFILES YET", 80, 250, 17, kMuted);
        for (int row = 0; row < 5; ++row) {
            const int index = ui.profileOffset + row;
            if (index >= static_cast<int>(ui.profiles.size())) break;
            const Rectangle rect = SkinProfileRow(row);
            DrawRectangleRounded(rect, 0.1f, 4,
                index == ui.selectedProfile
                    ? Color{52, 83, 87, 255}
                    : Color{36, 43, 56, 255});
            Label(font, FitText(font, ui.profiles[index].name,
                               rect.width - 24, 16),
                  rect.x + 12, rect.y + 7, 16,
                  index == ui.selectedProfile ? kGold : kText);
        }
        Button(font, SkinProfileActionRect(0), "ACTIVATE");
        Button(font, SkinProfileActionRect(1), "STORE");
        Button(font, SkinProfileActionRect(2), "REFRESH");
        Button(font, SkinProfileActionRect(3), "DELETE");
        Button(font, SkinProfileActionRect(4), "UNDO");
        PreviewPath(font, skin);
    }
    Label(font, "SKIN NAME", 616, 111, 12, kMuted);
    DrawInput(font, NameField(), skin.name, ui.editingName,
              ui.editingName && ui.nameSelectAll);
    Label(font, ui.dirty ? "UNSAVED CHANGES" : "ACTIVE PROFILE",
          630, 171, 12, ui.dirty ? kGold : kMuted);
    const char* actions[] = {"LOAD", "SAVE", "SAVE AS", "RESET"};
    for (int i = 0; i < 4; ++i)
        Button(font, SkinActionButton(i), actions[i]);
    Label(font, core::utf8::Truncate(ui.notice, 112), 53, 515, 13,
          ui.noticeError ? Color{245, 150, 150, 255} : kGold);
    if (!ui.pendingDeleteProfile.empty()) {
        DrawRectangle(0, 0, 960, 540, Color{12, 14, 22, 202});
        Panel({171, 188, 618, 183});
        Label(font, "MOVE THIS SKIN PROFILE TO TRASH?", 206, 214, 22, kGold);
        Label(font, FitText(font, ui.pendingDeleteProfile.filename().u8string(),
                             520, 16), 226, 259, 16, kText);
        Button(font, SkinDeleteConfirmRect(), "DELETE / Y");
        Button(font, SkinDeleteCancelRect(), "CANCEL / N");
    }
}

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
