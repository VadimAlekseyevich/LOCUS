#include "app/image_asset_texture.h"
#include "app/image_asset_file.h"

namespace locus::app {
bool ImageAssetTexture::Sync(const std::filesystem::path& imagePath,
                             std::string& error) {
    if (imagePath == requested_) {
        error.clear();
        return true;
    }
    Shutdown();
    requested_ = imagePath;
    if (imagePath.empty()) {
        error.clear();
        return true;
    }
    if (!ValidateImageAssetFile(imagePath, error)) return false;
    // raylib expects a UTF-8 filename on Windows.
    Image image = LoadImage(imagePath.u8string().c_str());
    if (!IsImageValid(image)) {
        error = "Cannot decode the background PNG/JPEG image.";
        return false;
    }
    texture_ = LoadTextureFromImage(image);
    UnloadImage(image);
    if (!IsTextureValid(texture_)) {
        texture_ = Texture2D{};
        error = "Cannot upload background image to the GPU.";
        return false;
    }
    SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
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
}
} // namespace locus::app
