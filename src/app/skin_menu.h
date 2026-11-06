#pragma once

#include "app/skin.h"
#include "raylib.h"

#include <string>

namespace locus::app {

enum class SkinCommand { None, Load, Save, SaveAs, Reset };

struct SkinUi {
    int page = 0;                 // 0 = path, 1 = player/HUD, 2 = palette
    int selectedColor = 1;
    bool dirty = false;
    bool editingName = false;
    std::string previousName;
    std::string notice = "SKINS ARE INDEPENDENT OF SONGS AND CHARTS";
    bool noticeError = false;
    Skin saved;                  // last applied profile, for cancel/back
};

void DrawSkinMenu(Font font, const Skin& skin, const SkinUi& ui);
SkinCommand SkinMenuClick(Vector2 mouse, Skin& skin, SkinUi& ui);
// True when input belongs to the active name editor (including ESC).
bool HandleSkinNameInput(Skin& skin, SkinUi& ui);
Rectangle SkinTabButton(int index);
Rectangle SkinActionButton(int index);

} // namespace locus::app
