#pragma once

#include "raylib.h"
#include "app/screen_flow.h"
#include "app/viewport.h"

namespace locus::app {
// Shared virtual geometry for save/discard/cancel on editor drafts. Hit tests
// convert physical pointer coordinates with the same viewport as rendering.
inline constexpr Rectangle kEditorSaveDraftButton{191.0f, 331.0f, 178.0f, 53.0f};
inline constexpr Rectangle kEditorDiscardDraftButton{391.0f, 331.0f, 178.0f, 53.0f};
inline constexpr Rectangle kEditorCancelDraftButton{591.0f, 331.0f, 178.0f, 53.0f};
inline constexpr Rectangle kEditorQuitSaveButton{382.0f, 386.0f, 196.0f, 42.0f};
// Global quit can be opened from PLAY, which uses a different camera from
// menu/editor screens. Match the rendered Save button's own viewport.
inline bool EditorQuitSavePointerHit(Screen screen, Vector2 physical,
                                     int screenWidth, int screenHeight) noexcept {
    if (screenWidth <= 0 || screenHeight <= 0) return false;
    const auto viewport = screen == Screen::Play
        ? ViewportTransform::Gameplay(screenWidth, screenHeight)
        : ViewportTransform::Menu(screenWidth, screenHeight);
    if (viewport.scale <= 0.0f) return false;
    const auto point = viewport.ToVirtual(physical);
    const auto rect = kEditorQuitSaveButton;
    return point.x >= rect.x && point.x < rect.x + rect.width &&
           point.y >= rect.y && point.y < rect.y + rect.height;
}
} // namespace locus::app
