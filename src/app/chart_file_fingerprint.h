#pragma once

#include "app/chart_file.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace locus::app {

// Lightweight NON-cryptographic content fingerprint for detecting accidental
// external chart edits that preserve file size and last-write timestamp.
// Reading at most 8 MiB per chart (the chart loader's maximum) is cheaper
// than parsing JSON and recalculating the difficulty of every difficulty.
inline bool ChartFileFingerprint(const std::filesystem::path& path,
                                 std::uint64_t& result) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size > kMaxChartFileBytes) return false;
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    std::uint64_t hash = 14695981039346656037ULL;
    std::array<char, 16384> buffer{};
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = file.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash *= 1099511628211ULL;
        }
    }
    if (!file.eof() || file.bad()) return false;
    result = hash;
    return true;
}

} // namespace locus::app
