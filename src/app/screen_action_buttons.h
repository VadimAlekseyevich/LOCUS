#pragma once
#include "app/viewport.h"
#include "raylib.h"

namespace locus::app {
// All hitboxes share the menu viewport between the renderer and input.
inline constexpr Rectangle kListenPauseButton{89, 376, 186, 40};
inline constexpr Rectangle kListenReplayButton{286, 376, 187, 40};
inline constexpr Rectangle kListenChooseButton{89, 424, 215, 41};
inline constexpr Rectangle kListenHomeButton{315, 424, 158, 41};
inline constexpr Rectangle kResultsReplayButton{98, 404, 217, 46};
inline constexpr Rectangle kResultsLibraryButton{327, 404, 218, 46};
inline constexpr Rectangle kResultsEditorButton{557, 404, 218, 46};
inline constexpr Rectangle kEditorModalConfirmButton{239, 333, 217, 47};
inline constexpr Rectangle kEditorModalCancelButton{504, 333, 217, 47};

inline bool MenuActionHit(Rectangle button, Vector2 pointer,
                          int width, int height) noexcept {
    if (width <= 0 || height <= 0) return false;
    const auto viewport = ViewportTransform::Menu(width, height);
    if (viewport.scale <= 0.0f) return false;
    const Vector2 mouse = viewport.ToVirtual(pointer);
    return mouse.x >= button.x && mouse.y >= button.y &&
           mouse.x < button.x + button.width &&
           mouse.y < button.y + button.height;
}
} // namespace locus::app
