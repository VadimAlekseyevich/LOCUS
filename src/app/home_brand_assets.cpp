#include "app/home_brand_assets.h"
#include "app/ui_theme.h"

#include <filesystem>
#include <string>

namespace locus::app {
namespace {

std::string AssetPath(const char* file) {
    // Assets are copied alongside the executable by CMake, so launching
    // LOCUS from a shortcut or another working directory still works.
    return (std::filesystem::u8path(GetApplicationDirectory()) /
            "assets" / "ui" / file).u8string();
}

void DrawTextureIn(Texture2D texture, Rectangle bounds) {
    if (!IsTextureValid(texture)) return;
    DrawTexturePro(texture,
        {0.0f, 0.0f, static_cast<float>(texture.width),
                     static_cast<float>(texture.height)},
        bounds, {0.0f, 0.0f}, 0.0f, WHITE);
}

} // namespace

void HomeBrandAssets::Load() {
    Unload();
    const std::string wordmarkPath = AssetPath("wordmark.png");
    if (FileExists(wordmarkPath.c_str())) {
        wordmark_ = LoadTexture(wordmarkPath.c_str());
        if (IsTextureValid(wordmark_))
            SetTextureFilter(wordmark_, TEXTURE_FILTER_BILINEAR);
    }
    const std::string iconPath = AssetPath("app-icon.png");
    if (FileExists(iconPath.c_str())) {
        Image image = LoadImage(iconPath.c_str());
        if (IsImageValid(image)) {
            // Runtime titlebar/taskbar icon. The packaged EXE resource can
            // be updated separately without changing these menu textures.
            SetWindowIcon(image);
            UnloadImage(image);
        }
    }
}

void HomeBrandAssets::Unload() {
    if (IsTextureValid(wordmark_)) UnloadTexture(wordmark_);
    wordmark_ = {};
}

void HomeBrandAssets::DrawAt(Font fallback, Rectangle bounds) const {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    if (IsTextureValid(wordmark_)) {
        const float aspect = static_cast<float>(wordmark_.width) /
                             static_cast<float>(wordmark_.height);
        const float width = std::min(bounds.width, bounds.height * aspect);
        const float height = width / aspect;
        DrawTextureIn(wordmark_, {bounds.x + (bounds.width - width) * 0.5f,
            bounds.y + (bounds.height - height) * 0.5f, width, height});
    } else {
        const float size = bounds.height * 0.7f;
        DrawTextEx(fallback, "locus.", {bounds.x, bounds.y}, size, 1.0f,
                   ApprovedUiTheme().softBlue);
    }
}

void HomeBrandAssets::Draw(Font fallback) const {
    DrawAt(fallback, {315.0f, 145.0f, 330.0f, 97.6f});
}

} // namespace locus::app
