#pragma once
#include "app/viewport.h"
#include <algorithm>

namespace locus::app {

// Layout engine supplies actual available regions. The adapter converts
// physical pixels once into the renderer's coordinate space. No widget
// computes its own screen-dependent geometry.
struct EditorLayoutSnapshot {
    bool native = false;
    ViewportTransform coordinates{};
    Rectangle timeline{};
    Rectangle preview{};
    Rectangle overview{};
    Rectangle compareToggle{};
    Rectangle status{};
    ViewportTransform Coordinates(int width, int height) const noexcept {
        return native ? coordinates : ViewportTransform::Menu(width, height);
    }
};

inline bool HasLayoutArea(Rectangle r) noexcept {
    return r.width > 0 && r.height > 0;
}

// A hidden/zero-sized native slot is intentionally empty, never a request
// to resurrect a legacy hitbox. Unmeasured headless fixtures may still
// provide individual regions without instantiating RmlUi.
inline Rectangle ResolveEditorRegion(const EditorLayoutSnapshot& layout,
                                      Rectangle measured, Rectangle legacy) noexcept {
    return layout.native || HasLayoutArea(measured) ? measured : legacy;
}

// Native controls are the default owner. Only named custom canvas slots
// (and their exclusive comparison overlay) receive raw raylib gestures.
// Adding/moving a native control requires no changes to this router.
inline bool EditorCanvasContains(const EditorLayoutSnapshot& layout,
                                 Vector2 point) noexcept {
    for (Rectangle area : {layout.timeline, layout.preview, layout.overview,
                           layout.compareToggle}) {
        if (HasLayoutArea(area) && point.x >= area.x && point.y >= area.y &&
            point.x < area.x+area.width && point.y < area.y+area.height)
            return true;
    }
    return false;
}

struct EditorPointerDispatch {
    bool nativeButtons = true;
    bool nativeWheel = true;
};

class EditorPointerRouter {
public:
    EditorPointerDispatch Route(const EditorLayoutSnapshot& layout, Vector2 point,
                                bool pressed, bool held, bool released,
                                bool canvasOverlayOpen) noexcept {
        const bool nativeHover = !canvasOverlayOpen &&
            !EditorCanvasContains(layout, point);
        if (pressed || (held && !captured_)) {
            captured_ = true;
            nativeCapture_ = nativeHover;
        }
        const EditorPointerDispatch dispatch{
            captured_ ? nativeCapture_ : nativeHover, nativeHover};
        if (released || (!held && !pressed)) captured_ = false;
        return dispatch;
    }

    void Reset() noexcept { captured_ = false; nativeCapture_ = true; }
private:
    bool captured_ = false;
    bool nativeCapture_ = true;
};

// Equal square panes; no fixed maximum side or dependency on window size.
inline Rectangle SquarePreviewPane(Rectangle area, int count, int index,
                                   float gap = 8.0f) noexcept {
    const int visible = std::clamp(count, 1, 3);
    if (index < 0 || index >= visible || !HasLayoutArea(area)) return {};
    gap = std::clamp(gap, 0.0f, area.width / (2.0f * visible));
    const float side = std::min(area.height,
        (area.width - (visible - 1) * gap) / visible);
    const float total = side * visible + (visible - 1) * gap;
    return {area.x + (area.width - total) * .5f + index * (side + gap),
            area.y + (area.height - side) * .5f, side, side};
}

} // namespace locus::app
