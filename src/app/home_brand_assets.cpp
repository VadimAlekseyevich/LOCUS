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
            "ui" / file).u8string();
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
            icon_ = LoadTextureFromImage(image);
            UnloadImage(image);
            if (IsTextureValid(icon_))
                SetTextureFilter(icon_, TEXTURE_FILTER_BILINEAR);
        }
    }
}

void HomeBrandAssets::Unload() {
    if (IsTextureValid(wordmark_)) UnloadTexture(wordmark_);
    if (IsTextureValid(icon_)) UnloadTexture(icon_);
    wordmark_ = {};
    icon_ = {};
}

void HomeBrandAssets::Draw(Font fallback) const {
    if (IsTextureValid(icon_))
        DrawTextureIn(icon_, {45.0f, 34.0f, 44.0f, 44.0f});
    else
        DrawTextEx(fallback, "LO", {48.0f, 43.0f}, 23.0f, 1.0f,
                   ApprovedUiTheme().softBlue);

    // The raster is produced from the exact approved SVG outlines.
    if (IsTextureValid(wordmark_))
        DrawTextureIn(wordmark_, {315.0f, 145.0f, 330.0f, 97.6f});
    else
        DrawTextEx(fallback, "locus.", {316.0f, 165.0f}, 72.0f, 1.0f,
                   ApprovedUiTheme().softBlue);
}

} // namespace locus::app
