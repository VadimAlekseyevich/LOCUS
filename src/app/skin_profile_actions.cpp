#include "app/skin_profile_actions.h"
#include "app/song_set_storage.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>

namespace locus::app {
namespace {
namespace fs = std::filesystem;
bool ProfileExtension(const fs::path& path) {
    const std::wstring filename = path.filename().wstring();
    const std::wstring suffix = L".locus-skin.json";
    return filename.size() > suffix.size() &&
           filename.compare(filename.size() - suffix.size(),
                            suffix.size(), suffix) == 0;
}
std::string SafeProfileStem(const std::string& name) {
    std::string stem;
    for (unsigned char c : name) {
        if (stem.size() >= 40) break;
        if (c >= 'a' && c <= 'z') stem += static_cast<char>(c);
        else if (c >= 'A' && c <= 'Z') stem += static_cast<char>(c);
        else if (c >= '0' && c <= '9') stem += static_cast<char>(c);
        else if (c == '-' || c == '_') stem += static_cast<char>(c);
        else if (!stem.empty() && stem.back() != '-') stem += '-';
    }
    while (!stem.empty() && stem.back() == '-') stem.pop_back();
    return stem.empty() ? "My-Skin" : stem;
}
}

bool SkinProfileActions::RefreshProfiles() {
    const fs::path root = activeFile_.parent_path() / L"Profiles";
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) {
        ui_.notice = "CANNOT CREATE PERSONAL SKIN FOLDER: " + ec.message();
        ui_.noticeError = true;
        return false;
    }
    fs::path selectedPath;
    if (ui_.selectedProfile >= 0 &&
        ui_.selectedProfile < static_cast<int>(ui_.profiles.size()))
        selectedPath = ui_.profiles[ui_.selectedProfile].path;
    std::vector<SkinProfileItem> entries;
    for (fs::directory_iterator it(root, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (it->is_symlink(ec) || ec || !it->is_regular_file(ec) ||
            ec || !ProfileExtension(it->path())) continue;
        if (entries.size() >= 256) break;
        Skin candidate;
        std::string error;
        const std::string filename = it->path().stem().u8string();
        // Still list corrupt profiles, so selecting one shows a precise error.
        const bool valid = LoadSkin(it->path(), candidate, error, false);
        const std::string label = valid
            ? candidate.name : filename + " (INVALID)";
        if (activeProfile_.empty() && valid &&
            SerializeSkin(candidate) == SerializeSkin(active_))
            activeProfile_ = it->path();
        entries.push_back({it->path(), label});
    }
    if (ec) {
        ui_.notice = "CANNOT SCAN PERSONAL SKINS: " + ec.message();
        ui_.noticeError = true;
        return false;
    }
    std::sort(entries.begin(), entries.end(),
              [](const SkinProfileItem& a, const SkinProfileItem& b) {
                  return a.path.filename() < b.path.filename();
              });
    ui_.profiles = std::move(entries);
    ui_.selectedProfile = -1;
    for (std::size_t i=0; i<ui_.profiles.size(); ++i) {
        if (ui_.profiles[i].path == selectedPath ||
            (selectedPath.empty() && ui_.profiles[i].path == activeProfile_)) {
            ui_.selectedProfile = static_cast<int>(i);
            break;
        }
    }
    if (ui_.selectedProfile == -1 && !ui_.profiles.empty())
        ui_.selectedProfile = 0;
    ui_.profileOffset = std::clamp(ui_.profileOffset, 0,
        std::max(0, static_cast<int>(ui_.profiles.size()) - 5));
    return true;
}

bool SkinProfileActions::SaveActive() {
    std::string error;
    if (!SaveSkin(activeFile_, draft_, error)) {
        ui_.notice = "SAVE FAILED: " + error;
        ui_.noticeError = true;
        return false;
    }
    active_ = draft_;
    ui_.saved = active_;
    ui_.dirty = false;
    if (!activeProfile_.empty() &&
        !SaveSkin(activeProfile_, active_, error)) {
        ui_.notice = "ACTIVE SKIN SAVED; PERSONAL COPY FAILED: " + error;
        ui_.noticeError = true;
        return false;
    }
    for (auto& item : ui_.profiles)
        if (!activeProfile_.empty() && item.path == activeProfile_)
            item.name = active_.name; // Name edits update the in-game list immediately.
    ui_.notice = "SKIN SAVED AND APPLIED TO ALL CHARTS";
    ui_.noticeError = false;
    return true;
}

void SkinProfileActions::Apply(SkinCommand command) {
    switch (command) {
        case SkinCommand::Load: {
            const auto picked = pickers_.chooseLoad();
            if (picked.path.empty()) {
                if (!picked.canceled) {
                    ui_.notice = picked.error;
                    ui_.noticeError = true;
                }
                return;
            }
            Skin selected;
            std::string error;
            if (!LoadSkin(picked.path, selected, error, false)) {
                ui_.notice = "LOAD FAILED: " + error;
                ui_.noticeError = true;
                return;
            }
            draft_ = selected;
            activeProfile_.clear(); // External import must not overwrite old profile on SAVE.
            ui_.dirty = true;
            ui_.notice = "SKIN LOADED - STORE COPY TO PERSONAL LIBRARY";
            ui_.noticeError = false;
            break;
        }
        case SkinCommand::Save:
            SaveActive();
            break;
        case SkinCommand::SaveAs: {
            const auto picked =
                pickers_.chooseExport(L"My-Skin.locus-skin.json");
            if (picked.path.empty()) {
                if (!picked.canceled) {
                    ui_.notice = picked.error;
                    ui_.noticeError = true;
                }
                return;
            }
            std::string error;
            if (!SaveSkin(picked.path, draft_, error)) {
                ui_.notice = "EXPORT FAILED: " + error;
                ui_.noticeError = true;
                return;
            }
            SaveActive();
            break;
        }
        case SkinCommand::Reset:
            draft_ = Skin{};
            activeProfile_.clear(); // Reset never destroys a stored personal copy.
            ui_.dirty = true;
            ui_.notice = "DEFAULT SKIN - SAVE TO ACTIVATE";
            ui_.noticeError = false;
            break;
        case SkinCommand::RefreshProfiles:
            if (RefreshProfiles()) {
                ui_.notice = "PERSONAL SKIN FOLDER REFRESHED";
                ui_.noticeError = false;
            }
            break;
        case SkinCommand::ActivateProfile: {
            if (ui_.dirty) {
                ui_.notice = "SAVE OR CANCEL UNSAVED EDITS BEFORE CHANGING PROFILE";
                ui_.noticeError = true;
                return;
            }
            if (ui_.selectedProfile < 0 ||
                ui_.selectedProfile >= static_cast<int>(ui_.profiles.size())) {
                ui_.notice = "SELECT A PERSONAL SKIN FIRST";
                ui_.noticeError = true;
                return;
            }
            const fs::path selectedFile = ui_.profiles[ui_.selectedProfile].path;
            Skin selected;
            std::string error;
            if (!LoadSkin(selectedFile, selected, error, false)) {
                ui_.notice = "PROFILE LOAD FAILED: " + error;
                ui_.noticeError = true;
                return;
            }
            // Do not change active or preview until the persistent active
            // file was successfully replaced.
            if (!SaveSkin(activeFile_, selected, error)) {
                ui_.notice = "PROFILE ACTIVATE FAILED: " + error;
                ui_.noticeError = true;
                return;
            }
            active_ = selected;
            draft_ = selected;
            ui_.saved = selected;
            ui_.dirty = false;
            activeProfile_ = selectedFile;
            ui_.notice = "PERSONAL SKIN ACTIVATED";
            ui_.noticeError = false;
            break;
        }
        case SkinCommand::DeleteProfile: {
            // Confirmed path is captured before any list reordering or rescan.
            const fs::path target = ui_.pendingDeleteProfile;
            ui_.pendingDeleteProfile.clear();
            const fs::path root = activeFile_.parent_path() / L"Profiles";
            const auto listed = std::find_if(ui_.profiles.begin(), ui_.profiles.end(),
                [&](const SkinProfileItem& item) { return item.path == target; });
            std::error_code ec;
            if (listed == ui_.profiles.end() || target.parent_path() != root ||
                fs::is_symlink(target, ec) || ec ||
                !fs::is_regular_file(target, ec) || ec) {
                ui_.notice = "PROFILE NO LONGER EXISTS; REFRESH PERSONAL SKINS";
                ui_.noticeError = true;
                break;
            }
            std::string error;
            FolderWriteLock lock;
            if (!lock.Acquire(root, error) || !TrashLibraryItem(root, target, error)) {
                ui_.notice = "DELETE FAILED: " + error;
                ui_.noticeError = true;
                break;
            }
            // Current appearance survives, but Save must not recreate the
            // just-deleted personal profile file.
            if (activeProfile_ == target) activeProfile_.clear();
            RefreshProfiles();
            ui_.notice = "SKIN MOVED TO TRASH / UNDO TO RESTORE";
            ui_.noticeError = false;
            break;
        }
        case SkinCommand::RestoreProfile: {
            const fs::path root = activeFile_.parent_path() / L"Profiles";
            std::string error;
            FolderWriteLock lock;
            if (!lock.Acquire(root, error) ||
                !RestoreLatestTrashItem(root, error)) {
                ui_.notice = "RESTORE FAILED: " + error;
                ui_.noticeError = true;
                break;
            }
            RefreshProfiles();
            ui_.notice = "LAST DELETED SKIN RESTORED";
            ui_.noticeError = false;
            break;
        }
        case SkinCommand::StoreProfile: {
            std::string error;
            if (!draft_.Valid(error)) {
                ui_.notice = "INVALID SKIN: " + error;
                ui_.noticeError = true;
                return;
            }
            const fs::path directory = activeFile_.parent_path() / L"Profiles";
            std::error_code ec;
            fs::create_directories(directory, ec);
            if (ec) {
                ui_.notice = "CANNOT CREATE PERSONAL SKIN FOLDER";
                ui_.noticeError = true;
                return;
            }
            const std::string stem = SafeProfileStem(draft_.name);
            fs::path destination;
            for (int i = 1; i <= 9999; ++i) {
                const std::string name = stem +
                    (i == 1 ? "" : "-" + std::to_string(i)) +
                    ".locus-skin.json";
                destination = directory / fs::u8path(name);
                const bool exists = fs::exists(destination, ec);
                if (ec) break;
                if (!exists) break;
                destination.clear();
            }
            if (ec || destination.empty()) {
                ui_.notice = "PERSONAL SKIN LIBRARY IS FULL OR UNAVAILABLE";
                ui_.noticeError = true;
                return;
            }
            if (!SaveSkin(destination, draft_, error)) {
                ui_.notice = "STORE PROFILE FAILED: " + error;
                ui_.noticeError = true;
                return;
            }
            activeProfile_ = destination;
            if (!SaveActive()) return;
            RefreshProfiles();
            for (std::size_t i=0; i<ui_.profiles.size(); ++i)
                if (ui_.profiles[i].path == destination) {
                    ui_.selectedProfile = static_cast<int>(i);
                    ui_.profileOffset = std::clamp(static_cast<int>(i)-2, 0,
                        std::max(0, static_cast<int>(ui_.profiles.size())-5));
                    break;
                }
            ui_.notice = "PERSONAL SKIN SAVED TO SKINS/PROFILES";
            ui_.noticeError = false;
            break;
        }
        case SkinCommand::None: break;
    }
}

} // namespace locus::app
