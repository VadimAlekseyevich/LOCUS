#pragma once

#include "app/image_asset_file.h"
#include "raylib.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace locus::app {

// CPU-only decode, no window or GPU needed. Call before publishing assets;
// the same path is reused when the cached renderer uploads a texture.
inline bool DecodeImageAssetFile(const std::filesystem::path& path,
                                 Image& image, std::string& error) {
    image = Image{};
    if (!ValidateImageAssetFile(path, error)) return false;
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        error = "Cannot open image data.";
        return false;
    }
    const auto size = stream.tellg();
    if (size <= 0 ||
        size > static_cast<std::streamoff>(kMaxImageAssetBytes)) {
        error = "Image data exceeds supported size.";
        return false;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    stream.seekg(0, std::ios::beg);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()),
                     static_cast<std::streamsize>(size))) {
        error = "Cannot read image data.";
        return false;
    }
    std::string extension = path.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    image = LoadImageFromMemory(extension.c_str(), bytes.data(),
                                static_cast<int>(bytes.size()));
    if (!IsImageValid(image)) {
        error = "Cannot decode PNG/JPEG image. Check that it is a valid file.";
        return false;
    }
    if (image.width > 8192 || image.height > 8192) {
        UnloadImage(image);
        image = Image{};
        error = "Image dimensions exceed 8192 pixels.";
        return false;
    }
    error.clear();
    return true;
}

} // namespace locus::app
