#pragma once
#include <filesystem>
#include <string>

namespace locus::app {
// Local owner, deliberately distinct from a future authenticated user ID.
// Loaded once at startup; never inferred from a display name.
struct LocalPlayerProfile {
    std::string id;
};
// Create the installation's default local profile atomically if absent.
// Concurrent starts converge on one ID. Corrupt/unreadable data is an
// error, not a request to silently allocate a different owner.
bool LoadOrCreateLocalPlayer(const std::filesystem::path& userData,
                             LocalPlayerProfile& profile,std::string& error);
} // namespace locus::app
