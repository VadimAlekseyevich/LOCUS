#pragma once

#include "app/skin_controls.h"
#include "app/skin_menu.h"
#include "raylib.h"

namespace locus::app::skin_detail {

// Shared geometry for BOTH the renderer and pointer hitboxes: no duplicated
// coordinates or independent widget definitions between event and draw code.
inline bool Hit(Vector2 mouse, Rectangle rect) {
    return CheckCollisionPointRec(mouse, rect);
}

inline Rectangle ColorRow(int row) {
    // Ten colours must fit above the skin action row at y=473.
    return {70.0f, 194.0f + row * 25.0f, 205.0f, 24.0f};
}
inline Rectangle NameField() { return {615.0f, 128.0f, 302.0f, 36.0f}; }
constexpr float RowPitch(int page) noexcept {
    return page == 0 ? 31.0f : 36.0f;
}
inline Rectangle SwitchRect(int page, int row) {
    return {468.0f, 200.0f + row * RowPitch(page), 84.0f, 28.0f};
}

inline const char* RowName(int page, int row) {
    const SkinSetting* setting = GetSkinSetting(page, row);
    return setting ? setting->label : "";
}

inline int RowCount(int page) { return SkinSettingCount(page); }

inline bool ToggleValue(const Skin& skin, int page, int row) {
    return GetSkinToggle(skin, page, row);
}

inline void FlipToggle(Skin& skin, int page, int row) {
    FlipSkinToggle(skin, page, row);
}

inline Rectangle SliderHit(Rectangle rect) {
    return {rect.x - 7.0f, rect.y - 12.0f, rect.width + 14.0f, 30.0f};
}

constexpr float kWheelInner = 64.0f;
constexpr float kWheelOuter = 84.0f;
constexpr float kPi = 3.14159265358979323846f;

} // namespace locus::app::skin_detail
