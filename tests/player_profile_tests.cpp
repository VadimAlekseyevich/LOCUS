#include "app/player_profile.h"
#include "core/chart_json.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs=std::filesystem;
using namespace locus;
int main() {
    const auto root=fs::temp_directory_path()/("locus-player-test-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!fs::create_directory(root)) return 1;
    struct Cleanup {
        fs::path root;
        ~Cleanup() { std::error_code ec; fs::remove_all(root,ec); }
    } cleanup{root};
    app::LocalPlayerProfile first,reopened,other;
    std::string error;
    if (!app::LoadOrCreateLocalPlayer(root/"one",first,error) ||
        !core::ValidDifficultyId(first.id) ||
        !app::LoadOrCreateLocalPlayer(root/"one",reopened,error) ||
        first.id!=reopened.id ||
        !app::LoadOrCreateLocalPlayer(root/"two",other,error) ||
        other.id==first.id) {
        std::cerr<<"Local profile identity failed: "<<error<<'\n'; return 1;
    }
    std::ofstream(root/"one"/"player.json",std::ios::trunc)<<"{broken";
    if (app::LoadOrCreateLocalPlayer(root/"one",reopened,error) ||
        error.empty() || reopened.id!=first.id) {
        std::cerr<<"Corruption must not silently replace the player identity\n"; return 1;
    }
    std::ifstream check(root/"one"/"player.json");
    std::string bytes;
    std::getline(check,bytes);
    return bytes=="{broken"?0:1;
}
