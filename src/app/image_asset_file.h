#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace locus::app {

// Shared boundary for disk-backed images. Song backgrounds use it now and
// future skin textures can use it without knowing about Song Sets.
inline bool SafeImageFilename(std::string_view name) noexcept {
    if (name.empty() || name == "." || name == ".." || name.size() > 255)
        return false;
    for (unsigned char c : name)
        if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':')
            return false;
    const auto dot = name.find_last_of('.');
    if (dot == std::string_view::npos) return false;
    std::string extension(name.substr(dot));
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}

inline constexpr std::uintmax_t kMaxImageAssetBytes = 24ULL * 1024ULL * 1024ULL;

inline bool ValidateImageAssetFile(const std::filesystem::path& path,
                                   std::string& error) {
    if (!SafeImageFilename(path.filename().u8string())) {
        error = "Image must be a PNG/JPEG with a safe filename.";
        return false;
    }
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size < 24 || size > kMaxImageAssetBytes) {
        error = "Image is missing, empty or larger than 24 MiB.";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    std::array<unsigned char, 24> bytes{};
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        error = "Cannot read image header.";
        return false;
    }
    const bool png = bytes[0] == 137 && bytes[1] == 80 &&
        bytes[2] == 78 && bytes[3] == 71 &&
        bytes[4] == 13 && bytes[5] == 10 &&
        bytes[6] == 26 && bytes[7] == 10 &&
        bytes[12] == 'I' && bytes[13] == 'H' &&
        bytes[14] == 'D' && bytes[15] == 'R';
    const bool jpeg = bytes[0] == 0xff && bytes[1] == 0xd8 &&
                      bytes[2] == 0xff;
    std::string ext = path.extension().u8string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){return static_cast<char>(std::tolower(c));});
    if ((ext == ".png" && png) ||
        ((ext == ".jpg" || ext == ".jpeg") && jpeg)) {
        if (png) {
            const auto get32 = [&](int offset) {
                return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
                       (static_cast<std::uint32_t>(bytes[offset+1]) << 16) |
                       (static_cast<std::uint32_t>(bytes[offset+2]) << 8) |
                        static_cast<std::uint32_t>(bytes[offset+3]);
            };
            if (!get32(16) || !get32(20) ||
                get32(16) > 8192 || get32(20) > 8192) {
                error = "Image dimensions must be between 1 and 8192.";
                return false;
            }
        }
        error.clear();
        return true;
    }
    error = "Image file signature does not match its extension.";
    return false;
}

} // namespace locus::app
