#include "app/player_profile.h"
#include "app/atomic_text_file.h"
#include "app/local_identity.h"
#include "core/chart_json.h"
#include "core/strict_json.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <utility>

namespace locus::app {
namespace fs=std::filesystem;
namespace {
bool ReadProfile(const fs::path& file,LocalPlayerProfile& profile,std::string& error) {
    std::error_code ec;
    if (fs::symlink_status(file,ec).type()!=fs::file_type::regular || ec) {
        error="Local player profile is not a regular file."; return false;
    }
    const auto size=fs::file_size(file,ec);
    if (ec || size==0 || size>4096) {
        error="Local player profile has invalid size."; return false;
    }
    std::ifstream input(file,std::ios::binary);
    std::string bytes(static_cast<std::size_t>(size),'\0');
    if (!input.read(bytes.data(),static_cast<std::streamsize>(bytes.size())) ||
        input.peek()!=std::char_traits<char>::eof() || input.bad()) {
        error="Cannot read local player profile."; return false;
    }
    if (!core::detail::ValidateJsonStructure(bytes,error)) return false;
    const auto json=nlohmann::json::parse(bytes,nullptr,false);
    if (!json.is_object() || json.size()!=2 || !json.contains("version") ||
        !json["version"].is_number_integer() || json["version"]!=1 ||
        !json.contains("playerId") || !json["playerId"].is_string() ||
        !core::ValidDifficultyId(json["playerId"].get<std::string>())) {
        error="Invalid local player profile."; return false;
    }
    profile={json["playerId"].get<std::string>()};
    error.clear(); return true;
}
}
bool LoadOrCreateLocalPlayer(const fs::path& userData,LocalPlayerProfile& profile,
                             std::string& error) {
    error.clear();
    try {
        if (userData.empty()) { error="Local data path is missing."; return false; }
        std::error_code ec;
        fs::create_directories(userData,ec);
        if (ec) { error="Cannot create local data directory: "+ec.message(); return false; }
        const auto file=userData/"player.json";
        const auto status=fs::symlink_status(file,ec);
        if (status.type()!=fs::file_type::not_found ||
            (ec && ec!=std::errc::no_such_file_or_directory))
            return ReadProfile(file,profile,error);
        const nlohmann::json json={{"version",1},{"playerId",NewLocalId()}};
        const auto result=AtomicCreateTextFile(file,json.dump(2)+"\n",error);
        if (result==AtomicTextWriteStatus::Failed) return false;
        // Read the committed winner even when another process created it.
        return ReadProfile(file,profile,error);
    } catch(const std::exception& e) {
        error=std::string("Cannot initialize local player: ")+e.what(); return false;
    }
}
} // namespace locus::app
