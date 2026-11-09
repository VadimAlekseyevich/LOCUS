#include "app/image_asset_texture.h"
#include "app/image_asset_decode.h"

namespace locus::app {
bool ImageAssetTexture::Sync(const std::filesystem::path& imagePath,
                             std::string& error) {
    if (imagePath == requested_) {
        error = loadError_;
        return loadError_.empty();
    }
    Shutdown();
    requested_ = imagePath;
    if (imagePath.empty()) {
        error.clear();
        return true;
    }
    Image image{};
    if (!DecodeImageAssetFile(imagePath, image, error)) {
        loadError_ = error;
        return false;
    }
    texture_ = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!IsTextureValid(texture_)) {
        texture_ = Texture2D{};
        error = loadError_ = "Cannot upload background image to the GPU.";
        return false;
    }
    SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    loadError_.clear();
    error.clear();
    return true;
}

void ImageAssetTexture::DrawCover(int targetWidth, int targetHeight,
                                   unsigned char darkenOpacity) const {
    if (!Ready() || targetWidth <= 0 || targetHeight <= 0) return;
    const auto crop = ComputeImageCoverCrop(
        static_cast<float>(texture_.width), static_cast<float>(texture_.height),
        static_cast<float>(targetWidth), static_cast<float>(targetHeight));
    DrawTexturePro(texture_, {crop.x, crop.y, crop.width, crop.height},
                   {0.0f, 0.0f, static_cast<float>(targetWidth),
                    static_cast<float>(targetHeight)},
                   {0.0f, 0.0f}, 0.0f, WHITE);
    DrawRectangle(0, 0, targetWidth, targetHeight,
                  Color{0, 0, 0, darkenOpacity});
}

void ImageAssetTexture::Shutdown() {
    if (Ready() && IsWindowReady()) UnloadTexture(texture_);
    texture_ = Texture2D{};
    requested_.clear();
    loadError_.clear();
}
} // namespace locus::app
