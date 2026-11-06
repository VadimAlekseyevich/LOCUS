#include "app/skin_menu.h"

#include "app/menu_ui.h"
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

bool Hit(Vector2 mouse, Rectangle area) {
    return CheckCollisionPointRec(mouse, area);
}

Rectangle Minus(int row) { return {429.0f, 200.0f + row * 36.0f, 34.0f, 29.0f}; }
Rectangle Plus(int row) { return {526.0f, 200.0f + row * 36.0f, 34.0f, 29.0f}; }
Rectangle ColorRow(int row) { return {70.0f, 199.0f + row * 28.0f, 485.0f, 26.0f}; }
Rectangle ChannelMinus(int channel) {
    return {723.0f, 252.0f + channel * 42.0f, 36.0f, 30.0f};
}
Rectangle ChannelPlus(int channel) {
    return {854.0f, 252.0f + channel * 42.0f, 36.0f, 30.0f};
}
Rectangle NameField() { return {615.0f, 128.0f, 302.0f, 36.0f}; }

const char* RowName(int page, int row) {
    const char* path[] = {"LOOKAHEAD", "PATH WIDTH", "OUTLINE SCALE",
                          "FADE CURVE", "PATH OPACITY", "TURN SIZE", "TURN GLOW"};
    const char* player[] = {"PLAYER SIZE", "CAMERA ZOOM", "PLAYER GLOW",
                            "HIT FLASH SIZE", "SHOW HUD"};
    return page == 0 ? path[row] : player[row];
}

int RowCount(int page) { return page == 0 ? 7 : 5; }

std::string RowValue(const Skin& skin, int page, int row) {
    if (page == 0) {
        switch (row) {
            case 0: return TextFormat("%.1f BEATS", skin.lookaheadBeats);
            case 1: return TextFormat("%.1f PX", skin.pathWidth);
            case 2: return TextFormat("%.1f X", skin.outlineScale);
            case 3: return TextFormat("%.2f", skin.fadePower);
            case 4: return TextFormat("%d%%", static_cast<int>(std::lround(skin.pathOpacity * 100)));
            case 5: return TextFormat("%.1f PX", skin.junctionRadius);
            default: return skin.junctionGlow ? "ON" : "OFF";
        }
    }
    switch (row) {
        case 0: return TextFormat("%.1f PX", skin.playerRadius);
        case 1: return TextFormat("%.0f PX", skin.cameraStep);
        case 2: return skin.playerGlow ? "ON" : "OFF";
        case 3: return TextFormat("%.1f X", skin.hitFlashScale);
        default: return skin.showHud ? "ON" : "OFF";
    }
}

void ChangeRow(Skin& skin, int page, int row, int direction) {
    const auto update = [&](float& value, float step, float low, float high) {
        value = std::clamp(value + step * direction, low, high);
        // Avoid cumulative decimal drift in UI controls.
        value = std::round(value * 100.0f) / 100.0f;
    };
    if (page == 0) {
        switch (row) {
            case 0: update(skin.lookaheadBeats, 0.5f, 0.5f, 16.0f); break;
            case 1: update(skin.pathWidth, 0.5f, 1.0f, 16.0f); break;
            case 2: update(skin.outlineScale, 0.1f, 1.0f, 4.0f); break;
            case 3: update(skin.fadePower, 0.25f, 0.25f, 4.0f); break;
            case 4: update(skin.pathOpacity, 0.05f, 0.1f, 1.0f); break;
            case 5: update(skin.junctionRadius, 0.5f, 2.0f, 24.0f); break;
            case 6: skin.junctionGlow = !skin.junctionGlow; break;
        }
    } else {
        switch (row) {
            case 0: update(skin.playerRadius, 0.5f, 4.0f, 30.0f); break;
            case 1: update(skin.cameraStep, 5.0f, 25.0f, 100.0f); break;
            case 2: skin.playerGlow = !skin.playerGlow; break;
            case 3: update(skin.hitFlashScale, 0.1f, 0.5f, 3.0f); break;
            case 4: skin.showHud = !skin.showHud; break;
        }
    }
}

void PreviewPath(const Skin& skin) {
    const Rectangle area{605.0f, 193.0f, 298.0f, 248.0f};
    DrawRectangleRounded(area, 0.07f, 6, skin.background);
    DrawRectangleRoundedLinesEx(area, 0.07f, 6, 1.2f, kMuted);
    Label(GetFontDefault(), "LIVE PREVIEW", 622, 206, 14.0f, kMuted);

    const auto point = [&](float beats) {
        // Fit the entire configured horizon into the preview pane.
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
    Label(GetFontDefault(), "SAVE TO USE IN PLAY / TEST", 618, 408, 12, kMuted);
}

void Panel(Rectangle area) {
    DrawRectangleRounded(area, 0.05f, 6, kPanel);
    DrawRectangleRoundedLinesEx(area, 0.05f, 6, 1.0f,
                                Color{51, 62, 78, 255});
}
} // namespace

Rectangle SkinTabButton(int index) {
    return {52.0f + index * 158.0f, 134.0f, 147.0f, 34.0f};
}
Rectangle SkinActionButton(int index) {
    return {52.0f + index * 135.0f, 473.0f, 125.0f, 34.0f};
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
                  15.0f, kMuted);
            Button(font, Minus(row), "-");
            Label(font, RowValue(skin, ui.page, row), 472.0f,
                  206.0f + row * 36.0f, 13.0f);
            Button(font, Plus(row), "+");
        }
        PreviewPath(skin);
    } else {
        for (int index = 0; index < kSkinColorCount; ++index) {
            const Rectangle row = ColorRow(index);
            if (index == ui.selectedColor) {
                DrawRectangleRounded(row, 0.08f, 3, Color{43, 59, 72, 255});
            }
            Label(font, SkinColorLabel(index), row.x + 9, row.y + 5, 13,
                  index == ui.selectedColor ? kGold : kText);
            DrawRectangle(static_cast<int>(row.x + 393),
                          static_cast<int>(row.y + 4), 62, 17,
                          SkinColor(skin, index));
        }
        Label(font, SkinColorLabel(ui.selectedColor), 612, 199, 17, kAccent);
        const Color color = SkinColor(skin, ui.selectedColor);
        const int channels[] = {color.r, color.g, color.b, color.a};
        const char* labels[] = {"RED", "GREEN", "BLUE", "ALPHA"};
        for (int i = 0; i < 4; ++i) {
            Label(font, labels[i], 612, 258.0f + i * 42.0f, 13, kMuted);
            Button(font, ChannelMinus(i), "-");
            Label(font, std::to_string(channels[i]), 783.0f,
                  258.0f + i * 42.0f, 17);
            Button(font, ChannelPlus(i), "+");
        }
        Label(font, "SHIFT + CLICK = 1 STEP", 611, 426, 12, kMuted);
    }
    Label(font, "NAME", 616, 112, 12, kMuted);
    Button(font, NameField(), ui.editingName ? "TYPE NAME + ENTER" : "RENAME");
    Label(font, skin.name.substr(0, 29) + (ui.dirty ? " *" : ""),
          637, 170, 12, kGold);
    const char* actions[] = {"LOAD", "SAVE", "SAVE AS", "RESET"};
    for (int i = 0; i < 4; ++i)
        Button(font, SkinActionButton(i), actions[i]);
    Label(font, ui.notice.substr(0, 101), 53, 515, 13,
          ui.noticeError ? Color{245, 150, 150, 255} : kGold);
}

SkinCommand SkinMenuClick(Vector2 mouse, Skin& skin, SkinUi& ui) {
    for (int i = 0; i < 3; ++i)
        if (Hit(mouse, SkinTabButton(i))) {
            ui.page = i;
            return SkinCommand::None;
        }
    if (Hit(mouse, NameField())) {
        ui.editingName = true;
        ui.previousName = skin.name;
        ui.notice = "TYPE A NAME / ENTER TO APPLY / ESC TO CANCEL";
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
        for (int i = 0; i < 4; ++i) {
            const int dir = Hit(mouse, ChannelMinus(i)) ? -1
                          : Hit(mouse, ChannelPlus(i)) ? 1 : 0;
            if (dir == 0) continue;
            Color& color = SkinColor(skin, ui.selectedColor);
            unsigned char* channels[] = {&color.r, &color.g, &color.b, &color.a};
            const int increment = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)
                ? 1 : 8;
            *channels[i] = static_cast<unsigned char>(
                std::clamp(static_cast<int>(*channels[i]) + dir * increment, 0, 255));
            ui.dirty = true;
            return SkinCommand::None;
        }
    } else {
        for (int i = 0; i < RowCount(ui.page); ++i) {
            const int dir = Hit(mouse, Minus(i)) ? -1 : Hit(mouse, Plus(i)) ? 1 : 0;
            if (dir == 0) continue;
            ChangeRow(skin, ui.page, i, dir);
            ui.dirty = true;
            return SkinCommand::None;
        }
    }
    return SkinCommand::None;
}

bool HandleSkinNameInput(Skin& skin, SkinUi& ui) {
    if (!ui.editingName) return false;
    if (IsKeyPressed(KEY_ESCAPE)) {
        skin.name = ui.previousName;
        ui.editingName = false;
        ui.notice = "NAME EDIT CANCELED";
        return true;
    }
    if (IsKeyPressed(KEY_ENTER)) {
        if (skin.name.empty()) skin.name = ui.previousName;
        ui.editingName = false;
        ui.dirty = skin.name != ui.previousName || ui.dirty;
        ui.notice = "NAME UPDATED - SAVE TO APPLY";
        return true;
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !skin.name.empty()) {
        // Remove the last UTF-8 code point, not just one byte.
        do { skin.name.pop_back(); }
        while (!skin.name.empty() &&
               (static_cast<unsigned char>(skin.name.back()) & 0xc0) == 0x80);
    }
    int codepoint = 0;
    while ((codepoint = GetCharPressed()) != 0) {
        if (codepoint < 32 || codepoint > 0x10ffff) continue;
        std::string utf8;
        if (codepoint < 0x80) utf8 += static_cast<char>(codepoint);
        else if (codepoint < 0x800) {
            utf8 += static_cast<char>(0xc0 | (codepoint >> 6));
            utf8 += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else if (codepoint < 0x10000) {
            utf8 += static_cast<char>(0xe0 | (codepoint >> 12));
            utf8 += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            utf8 += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else {
            utf8 += static_cast<char>(0xf0 | (codepoint >> 18));
            utf8 += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
            utf8 += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            utf8 += static_cast<char>(0x80 | (codepoint & 0x3f));
        }
        if (skin.name.size() + utf8.size() <= 64) skin.name += utf8;
    }
    return true;
}

} // namespace locus::app
