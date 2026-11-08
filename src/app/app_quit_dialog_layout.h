#pragma once

#include "app/screen_flow.h"
#include "app/viewport.h"
#include "raylib.h"

namespace locus::app {
// The global quit dialog's drawing and pointer input share one geometry
// contract. Mapping physical pixels to virtual coordinates is pure, so a
// portrait/ultrawide window can be tested without creating a raylib window.
inline constexpr Rectangle kQuitConfirmButton{263.0f, 329.0f, 196.0f, 54.0f};
inline constexpr Rectangle kQuitCancelButton{502.0f, 329.0f, 196.0f, 54.0f};

enum class QuitDialogChoice { None, Confirm, Cancel };

inline QuitDialogChoice QuitDialogPointerChoice(
    Screen screen, Vector2 physical, int windowWidth,
    int windowHeight) noexcept {
    if (windowWidth <= 0 || windowHeight <= 0)
        return QuitDialogChoice::None;
    const ViewportTransform viewport = screen == Screen::Play
        ? ViewportTransform::Gameplay(windowWidth, windowHeight)
        : ViewportTransform::Menu(windowWidth, windowHeight);
    if (viewport.scale <= 0.0f) return QuitDialogChoice::None;
    const Vector2 virtualMouse = viewport.ToVirtual(physical);
    const auto inside = [&](Rectangle rect) {
        return virtualMouse.x >= rect.x &&
               virtualMouse.y >= rect.y &&
               virtualMouse.x < rect.x + rect.width &&
               virtualMouse.y < rect.y + rect.height;
    };
    if (inside(kQuitConfirmButton)) return QuitDialogChoice::Confirm;
    if (inside(kQuitCancelButton)) return QuitDialogChoice::Cancel;
    return QuitDialogChoice::None;
}
} // namespace locus::app
