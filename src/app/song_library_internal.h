#pragma once
#include "app/song_library.h"
#include <filesystem>
#include <string>
#include <vector>

namespace locus::app::detail {
// Helpers shared across independent catalog scanner and storage operations.
bool Inside(const std::filesystem::path&, const std::filesystem::path&);
std::string HashText(const std::filesystem::path&);
std::string HashSetId(const std::string&);
std::string FilenamePart(std::string);
std::string SongDisplayTitle(const std::filesystem::path&);
void SortChartDifficulties(std::vector<LibraryChart>&);
} // namespace locus::app::detail
