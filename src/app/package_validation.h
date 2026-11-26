#pragma once
#include "app/song_set_manifest.h"
#include "core/chart_json.h"
#include <map>

namespace locus::app {
struct PackageDifficulty {
    std::filesystem::path file;
    core::LoadedChart chart; // original chart; apply authoritative timing for scoring
};
struct ValidatedPackage {
    SongSetManifest manifest;
    std::filesystem::path audio;
    std::map<std::string,PackageDifficulty> difficulties;
};

// Read a private, freshly extracted directory. No desktop dependencies;
// validates package structure and chart schemas, NOT decoded audio duration,
// full media integrity, ownership, or online eligibility. No publication.
bool ValidateStagedPackage(const std::filesystem::path& folder,
                           ValidatedPackage& package,std::string& error);
} // namespace locus::app
