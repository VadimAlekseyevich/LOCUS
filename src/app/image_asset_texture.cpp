#include "app/image_asset_texture.h"
#include "app/image_asset_file.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

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
    // Open using std::filesystem's native path (UTF-16 on Windows), then
    // decode from memory. This also supports Cyrillic/Unicode asset paths
    // regardless of raylib's narrow-filename implementation.
    std::ifstream stream(imagePath, std::ios::binary | std::ios::ate);
    if (!stream) {
        error = "Cannot open image data.";
        return false;
    }
    const auto count = stream.tellg();
    if (count <= 0 || count > static_cast<std::streamoff>(kMaxImageAssetBytes)) {
        error = "Image data exceeds supported size.";
        return false;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(count));
    stream.seekg(0, std::ios::beg);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), count)) {
        error = "Cannot read image data.";
        return false;
    }
    std::string extension = imagePath.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    Image image = LoadImageFromMemory(extension.c_str(), bytes.data(),
                                      static_cast<int>(bytes.size()));
    if (!IsImageValid(image)) {
        error = "Cannot decode the background PNG/JPEG image.";
        return false;
    }
    if (image.width > 8192 || image.height > 8192) {
        UnloadImage(image);
        error = "Background image exceeds 8192 pixels per dimension.";
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
