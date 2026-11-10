#include "app/skin_menu.h"
#include "app/skin_menu_layout.h"
#include "app/menu_ui.h"
#include "app/ui_button.h"
#include "app/route_renderer.h"
#include "app/visual_style.h"
#include "app/viewport.h"
#include "core/utf8.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace locus::app {
using namespace skin_detail;
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
    DrawUiButton(font, r, text, SkinButtonStyle(), active);
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

    // The SKIN preview clips like the real gameplay square,
    // including thick outline strokes and partially visible junctions.
    const auto clip = ViewportTransform::Menu(
        GetScreenWidth(), GetScreenHeight()).Scissor(frame);
    BeginScissorMode(clip.x, clip.y, clip.width, clip.height);
    const float pixelsPerBeat =
        StepForFixedSpeed(skin.baseSpeedPxPerSecond, 120.0, 1) *
        frame.width / kVirtualHeight;
    const float horizon = static_cast<float>(
        ScoredPreviewTicks(skin.lookaheadMs, 120.0, 1));
    // Steady rightward travel: the avatar appears ahead of the
    // camera center by the same speed-dependent lag as in PLAY.
    const float lagPx = std::min(
        72.0f, skin.baseSpeedPxPerSecond * skin.cameraLagMs / 1000.0f) *
        frame.width / kVirtualHeight;
    const auto point = [&](float beats) {
        const float progress = beats / horizon;
        return Vector2{687.0f + lagPx + beats * pixelsPerBeat,
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


} // namespace

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
        Button(font, SkinProfilesFolderRect(), "OPEN FOLDER");
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

} // namespace locus::app
