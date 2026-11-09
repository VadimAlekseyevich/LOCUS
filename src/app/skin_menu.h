#pragma once

#include "app/skin.h"
#include "raylib.h"

#include <filesystem>
#include <string>
#include <vector>

namespace locus::app {

enum class SkinCommand {
    None, Load, Save, SaveAs, Reset, ActivateProfile, StoreProfile, RefreshProfiles,
    DeleteProfile, RestoreProfile
};
struct SkinProfileItem {
    std::filesystem::path path;
    std::string name;
};

struct SkinUi {
    int page = 0;               // 0 = path, 1 = player/HUD, 2 = colors, 3 = profiles
    int selectedColor = 1;
    bool dirty = false;
    bool editingName = false;
    bool nameSelectAll = false;
    std::string previousName;

    int editingNumberRow = -1;  // numeric field focused on current page
    bool numberSelectAll = false;
    std::string numberBuffer;   // don't change a live value until validated
    int draggingRow = -1;       // path/player sliders only
    int draggingColor = 0;      // 1=hue wheel, 2=saturation/value square
    float pickerHue = 0.0f;     // survives hue changes on grayscale colors
    bool editingHex = false, hexSelectAll = false;
    std::string hexBuffer;      // #RRGGBB or #RRGGBBAA
    std::vector<SkinProfileItem> profiles;
    int selectedProfile = -1, profileOffset = 0;
    std::filesystem::path pendingDeleteProfile; // Captured file identity for confirmation.

    std::string notice = "COLOR WHEEL + HEX; SKINS IN PERSONAL PROFILES FOLDER";
    bool noticeError = false;
    Skin saved;                 // last applied profile, for cancel/back
};

void DrawSkinMenu(Font font, const Skin& skin, const SkinUi& ui);
SkinCommand SkinMenuClick(Vector2 mouse, Skin& skin, SkinUi& ui);

// Consumed keyboard input while the name or a number is being edited.
bool SkinMenuEditingText(const SkinUi& ui) noexcept;
void HandleSkinTextInput(Skin& skin, SkinUi& ui);
// Commit on blur as well as Enter; invalid input keeps focus and the old value.
bool FinishSkinTextInput(Skin& skin, SkinUi& ui, bool apply);
// Called every frame, even when no new click arrives.
void UpdateSkinSliderDrag(Vector2 mouse, bool leftHeld, Skin& skin, SkinUi& ui);

Rectangle SkinTabButton(int index);
Rectangle SkinActionButton(int index);
Rectangle SkinNumberInputRect(int page, int row);
Rectangle SkinSliderRect(int page, int row);
Rectangle SkinHexRect();
Rectangle SkinColorSquareRect();
Vector2 SkinColorWheelCenter();
Rectangle SkinProfileRow(int index);
Rectangle SkinProfileActionRect(int index);
Rectangle SkinDeleteConfirmRect();
Rectangle SkinDeleteCancelRect();

} // namespace locus::app
