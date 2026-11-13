#pragma once

#include "raylib.h"

namespace locus::app {

// Only the Home menu uses these textures. Gameplay never loads or draws them.
class HomeBrandAssets {
public:
    HomeBrandAssets() = default;
    HomeBrandAssets(const HomeBrandAssets&) = delete;
    HomeBrandAssets& operator=(const HomeBrandAssets&) = delete;

    void Load();
    void Unload();
    void Draw(Font fallback) const;
    void DrawAt(Font fallback, Rectangle physicalBounds) const;

private:
    Texture2D wordmark_{};
};

} // namespace locus::app
