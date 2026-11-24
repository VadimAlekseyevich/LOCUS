#include "app/home_brand_assets.h"
#include "app/ui_theme.h"
#include "app/executable_directory.h"
#include "app/image_asset_decode.h"
#include <iostream>

#include <filesystem>
#include <string>

namespace locus::app {
namespace {

std::filesystem::path AssetPath(const char* file) {
    return ExecutableDirectory() / "assets" / "ui" / file;
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
    std::string error;
    Image image{};
    if (DecodeImageAssetFile(AssetPath("wordmark.png"), image, error)) {
        wordmark_ = LoadTextureFromImage(image);
        UnloadImage(image);
        if (IsTextureValid(wordmark_))
            SetTextureFilter(wordmark_, TEXTURE_FILTER_BILINEAR);
    } else {
        std::cerr << "UI_WORDMARK_UNAVAILABLE " << error << '\n';
    }
    if (DecodeImageAssetFile(AssetPath("app-icon.png"), image, error)) {
        SetWindowIcon(image);
        UnloadImage(image);
    } else {
        std::cerr << "UI_ICON_UNAVAILABLE " << error << '\n';
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
