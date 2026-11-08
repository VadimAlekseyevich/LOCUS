#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace locus::app {

// Portable .locuspack ZIP archives: only store (method 0), flat UTF-8 names,
// bounded file counts/sizes and CRC verification. ZIP import is deliberately
// stricter than general-purpose unzippers to prevent path traversal and bombs.
bool ExportSongSetArchive(
    const std::filesystem::path& setFolder,
    const std::vector<std::filesystem::path>& chartFiles,
    const std::filesystem::path& archiveFile,
    std::string& error);

// The caller creates an empty, private staging folder outside Songs and
// owns its cleanup. All content is validated before publication into Songs.
bool ExtractSongSetArchive(const std::filesystem::path& archiveFile,
                           const std::filesystem::path& stagingFolder,
                           std::string& error);

} // namespace locus::app
