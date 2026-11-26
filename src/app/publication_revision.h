#pragma once
#include "app/package_validation.h"
#include <cstdint>
#include <map>
#include <string>

namespace locus::app {

// Content address, not a server-issued ID, proof of ownership or signature.
struct ContentDigest {
    std::uint64_t bytes=0;
    std::string sha256; // 64 lowercase hex characters
};
struct PublishedDifficultyContent {
    ContentDigest file;
    std::string gameplaySha256; // effective chart + exact audio bytes
};
struct PackageRevisionContent {
    ContentDigest manifest;
    ContentDigest audio;
    std::optional<ContentDigest> background;
    std::map<std::string,PublishedDifficultyContent> difficulties; // authoring IDs
};

// Caller exclusively owns a private, immutable, extracted directory for
// the duration. No network, database, media decode or publication.
// Outputs remain unchanged on failure. Hashing is bounded and streaming.
bool DescribePackageRevision(const std::filesystem::path& folder,
                             PackageRevisionContent& revision,std::string& error);

} // namespace locus::app
