#pragma once
#include "app/song_library.h"
#include <filesystem>
#include <string>
#include <vector>

namespace locus::app { struct SongSetManifest; }
namespace locus::app::detail {
bool MaterializeIndexedChart(const LibraryChart&,const SongSetManifest&,
                             core::LoadedChart&,std::string& error);
// Helpers shared across independent catalog scanner and storage operations.
bool Inside(const std::filesystem::path&, const std::filesystem::path&);
std::string HashText(const std::filesystem::path&);
std::string HashSetId(const std::string&);
std::string FilenamePart(std::string);
std::string SongDisplayTitle(const std::filesystem::path&);
void SortChartDifficulties(std::vector<LibraryChart>&);
// Under the Song Set lock, reject a stale index or a chart added/changed
// externally. Used by shared BPM edits; export validates live disk data.
bool SameSongSetFiles(const LibrarySong&, std::string& error);
} // namespace locus::app::detail
