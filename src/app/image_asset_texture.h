#pragma once

#include "app/image_cover_geometry.h"
#include "raylib.h"

#include <filesystem>
#include <string>

namespace locus::app {

// Reusable GPU image resource. No Song Set or gameplay dependencies. Requires
// explicit Shutdown while raylib's window/graphics context is still alive.
class ImageAssetTexture {
public:
    ImageAssetTexture() = default;
    ImageAssetTexture(const ImageAssetTexture&) = delete;
    ImageAssetTexture& operator=(const ImageAssetTexture&) = delete;

    bool Sync(const std::filesystem::path& imagePath, std::string& error);
    void DrawCover(int targetWidth, int targetHeight,
                   unsigned char darkenOpacity) const;
    void Shutdown();
    bool Ready() const noexcept { return texture_.id != 0; }
private:
    Texture2D texture_{};
    std::filesystem::path requested_;
};
} // namespace locus::app
