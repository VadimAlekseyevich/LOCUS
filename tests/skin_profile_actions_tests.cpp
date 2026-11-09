#include "app/skin_profile_actions.h"

#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
namespace fs = std::filesystem;
fs::path importFile, exportFile;
int loadCalls = 0, exportCalls = 0;
locus::app::MusicPickResult CancelLoad() {
    ++loadCalls; return {{}, {}, true};
}
locus::app::MusicPickResult GoodLoad() {
    ++loadCalls; return {importFile.wstring(), {}, false};
}
locus::app::MusicPickResult BadLoad() {
    ++loadCalls; return {L"no-such-skin-file.locus-skin.json", {}, false};
}
locus::app::MusicPickResult Export(const std::wstring& suggestion) {
    ++exportCalls;
    return {suggestion == L"My-Skin.locus-skin.json"
                ? exportFile.wstring() : L"", {}, false};
}
locus::app::MusicPickResult CancelExport(const std::wstring&) {
    ++exportCalls; return {{}, {}, true};
}
bool Check(bool value, const char* label) {
    if (!value) std::cerr << "SKIN_PROFILE_FAIL " << label << '\n';
    return value;
}
}

int main() {
    using namespace locus::app;
    bool ok = true;
    const fs::path root = fs::temp_directory_path() /
        ("locus-skin-actions-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const fs::path activeFile = root / "Skins" / "active.locus-skin.json";
    importFile = root / "my-skin.locus-skin.json";
    exportFile = root / "exported" / "My-Skin.locus-skin.json";
    Skin active, draft;
    active.name = "CURRENT";
    draft = active;
    SkinUi ui;
    ui.saved = active;
    std::string error;
    Skin imported;
    imported.name = "IMPORTED";
    ok &= Check(SaveSkin(importFile, imported, error),
                "test import fixture persisted successfully");

    SkinProfileActions canceled{active, draft, ui, activeFile,
                                {CancelLoad, CancelExport}};
    const auto oldNotice = ui.notice;
    canceled.Apply(SkinCommand::Load);
    canceled.Apply(SkinCommand::SaveAs);
    ok &= Check(loadCalls == 1 && exportCalls == 1 &&
                active.name == "CURRENT" && draft.name == "CURRENT" &&
                ui.notice == oldNotice && !ui.dirty &&
                !fs::exists(activeFile),
                "canceled file dialogs do not modify active or draft skins");

    SkinProfileActions load{active, draft, ui, activeFile,
                            {GoodLoad, Export}};
    load.Apply(SkinCommand::Load);
    ok &= Check(active.name == "CURRENT" && draft.name == "IMPORTED" &&
                ui.dirty && !ui.noticeError && !fs::exists(activeFile),
                "Load updates only the draft until user saves");
    load.Apply(SkinCommand::SaveAs);
    Skin reopened;
    ok &= Check(fs::exists(exportFile) && fs::exists(activeFile) &&
                active.name == "IMPORTED" && !ui.dirty &&
                ui.saved.name == "IMPORTED" &&
                LoadSkin(activeFile, reopened, error) &&
                reopened.name == "IMPORTED",
                "Save As exports a copy and applies a global active profile");

    // Startup fallback remains supported, but an explicit picker is strict.
    Skin fallback;
    fallback.name = "PREVIOUS";
    ok &= Check(LoadSkin(root / "not-created-yet.json", fallback, error) &&
                fallback.name == "Default" &&
                !LoadSkin(root / "not-created-yet.json", fallback, error, false) &&
                fallback.name == "Default" && !error.empty(),
                "startup skin defaults and strict file-picker behavior are distinct");

    SkinProfileActions bad{active, draft, ui, activeFile,
                           {BadLoad, CancelExport}};
    bad.Apply(SkinCommand::Load);
    ok &= Check(ui.noticeError && ui.notice.find("LOAD FAILED: ") == 0 &&
                draft.name == "IMPORTED" && active.name == "IMPORTED",
                "failed skin parse does not replace a working profile");

    bad.Apply(SkinCommand::Reset);
    ok &= Check(ui.dirty && draft.name == "Default" &&
                active.name == "IMPORTED" &&
                ui.notice == "DEFAULT SKIN - SAVE TO ACTIVATE",
                "Reset prepares defaults without applying");
    bad.Apply(SkinCommand::Save);
    ok &= Check(!ui.dirty && active.name == "Default" &&
                ui.saved.name == "Default" && !ui.noticeError,
                "Save publishes the draft and clears dirty state");

    draft.name.clear(); // Invalid profile must never become active.
    ui.dirty = true;
    bad.Apply(SkinCommand::Save);
    ok &= Check(ui.noticeError && ui.dirty &&
                active.name == "Default" &&
                ui.notice.find("SAVE FAILED: ") == 0,
                "invalid skin remains draft and cannot overwrite active file");

    // A personal library lives alongside (not inside) the one active skin
    // configuration. It is not a list of arbitrary external file paths.
    draft.name = "Personal blue";
    ui.dirty = true;
    bad.Apply(SkinCommand::StoreProfile);
    const fs::path profileDir = activeFile.parent_path() / "Profiles";
    ok &= Check(!ui.noticeError && !ui.dirty &&
                fs::exists(profileDir / "Personal-blue.locus-skin.json") &&
                !ui.profiles.empty() && ui.selectedProfile >= 0 &&
                active.name == "Personal blue",
                "store copy creates an independently reusable personal skin");
    const fs::path firstProfile = profileDir / "Personal-blue.locus-skin.json";

    draft.name = "Personal red";
    draft.trajectory = Color{240, 34, 70, 201};
    ui.dirty = true;
    bad.Apply(SkinCommand::StoreProfile);
    ok &= Check(active.name == "Personal red" &&
                ui.profiles.size() == 2 &&
                fs::exists(profileDir / "Personal-red.locus-skin.json"),
                "another personal profile does not overwrite previous skin");

    int blueIndex = -1;
    for (std::size_t i = 0; i < ui.profiles.size(); ++i)
        if (ui.profiles[i].path == firstProfile)
            blueIndex = static_cast<int>(i);
    ui.selectedProfile = blueIndex;
    draft.name = "UNSAVED";
    ui.dirty = true;
    bad.Apply(SkinCommand::ActivateProfile);
    ok &= Check(blueIndex >= 0 && ui.noticeError &&
                active.name == "Personal red" &&
                draft.name == "UNSAVED",
                "switching profiles cannot discard unsaved edits");
    draft = active;
    ui.dirty = false;
    bad.Apply(SkinCommand::ActivateProfile);
    ok &= Check(!ui.noticeError && !ui.dirty &&
                active.name == "Personal blue" &&
                draft.name == "Personal blue" &&
                ui.saved.name == "Personal blue",
                "activating a stored profile updates active, draft and saved state");
    draft.pathWidth = 9.75f;
    ui.dirty = true;
    bad.Apply(SkinCommand::Save);
    Skin updatedPersonal;
    ok &= Check(LoadSkin(firstProfile, updatedPersonal, error, false) &&
                updatedPersonal.pathWidth == 9.75f &&
                active.pathWidth == 9.75f,
                "Save updates active skin and the currently activated personal copy");

    // Corrupted user-controlled profile files remain visible, but are never
    // applied to the game. The current active skin stays untouched.
    const fs::path broken = profileDir / "Broken.locus-skin.json";
    {
        std::ofstream stream(broken, std::ios::binary | std::ios::trunc);
        stream << "{ invalid json";
    }
    ok &= Check(bad.RefreshProfiles() && ui.profiles.size() == 3,
                "corrupt profiles may be inspected without aborting folder scan");
    for (std::size_t i = 0; i < ui.profiles.size(); ++i)
        if (ui.profiles[i].path == broken)
            ui.selectedProfile = static_cast<int>(i);
    bad.Apply(SkinCommand::ActivateProfile);
    ok &= Check(ui.noticeError && active.name == "Personal blue",
                "corrupt selected profile cannot replace active skin");

    // A profile, including the active one, can be deleted and restored
    // without affecting the currently applied appearance or unsaved draft.
    ui.pendingDeleteProfile = broken;
    bad.Apply(SkinCommand::DeleteProfile);
    ok &= Check(!ui.noticeError && !fs::exists(broken) &&
                ui.profiles.size() == 2 && active.name == "Personal blue" &&
                fs::exists(profileDir.parent_path() / "Trash"),
                "invalid personal profile moves to independent skin Trash");
    bad.Apply(SkinCommand::RestoreProfile);
    ok &= Check(!ui.noticeError && fs::exists(broken) &&
                ui.profiles.size() == 3,
                "undo restores the exact previously deleted profile");
    ui.pendingDeleteProfile = firstProfile;
    bad.Apply(SkinCommand::DeleteProfile);
    ok &= Check(!ui.noticeError && !fs::exists(firstProfile) &&
                active.name == "Personal blue" && !ui.dirty,
                "deleting active profile preserves its applied appearance");
    bad.Apply(SkinCommand::Save);
    ok &= Check(!fs::exists(firstProfile) &&
                active.name == "Personal blue",
                "later Save cannot silently recreate a deleted profile");
    bad.Apply(SkinCommand::RestoreProfile);
    ok &= Check(!ui.noticeError && fs::exists(firstProfile),
                "undo restores a deleted active profile file");
    std::error_code ec;
    fs::remove_all(root, ec);
    if (!ok) return 1;
    std::cout << "SKIN_PROFILE_PASS cancel=1 load=1 saveas=1 reset=1 invalid=1\n";
    return 0;
}
