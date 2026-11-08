#include "app/app_quit_dialog_layout.h"

#include <iostream>

using namespace locus::app;

namespace {
bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "QUIT_POINTER_FAIL " << label << '\n';
    return condition;
}
}

int main() {
    bool ok = true;
    ok &= Check(QuitDialogPointerChoice(Screen::Home,
                    {361.0f, 356.0f}, 960, 540) ==
                    QuitDialogChoice::Confirm,
                "normal 960x540 confirmation hit");
    ok &= Check(QuitDialogPointerChoice(Screen::Home,
                    {600.0f, 356.0f}, 960, 540) ==
                    QuitDialogChoice::Cancel,
                "normal cancel hit");
    ok &= Check(QuitDialogPointerChoice(Screen::Home,
                    {482.0f, 356.0f}, 960, 540) ==
                    QuitDialogChoice::None,
                "gap cannot confirm or cancel");
    ok &= Check(QuitDialogPointerChoice(Screen::Home,
                    {722.0f, 712.0f}, 1920, 1080) ==
                    QuitDialogChoice::Confirm,
                "menu confirmation is scaled correctly at 1920x1080");
    ok &= Check(QuitDialogPointerChoice(Screen::Play,
                    {722.0f, 712.0f}, 1920, 1080) ==
                    QuitDialogChoice::Confirm,
                "gameplay confirmation uses gameplay viewport");
    // Portrait menu is centered vertically; gameplay is centered horizontally
    // and uses a different transformation on the same physical window.
    ok &= Check(QuitDialogPointerChoice(Screen::Home,
                    {270.0f, 604.0f}, 720, 1080) ==
                    QuitDialogChoice::Confirm,
                "portrait menu keeps quit confirmation clickable");
    ok &= Check(QuitDialogPointerChoice(Screen::Play,
                    {201.0f, 474.0f}, 720, 1080) ==
                    QuitDialogChoice::Confirm,
                "portrait gameplay uses the correct off-center viewport");
    ok &= Check(QuitDialogPointerChoice(Screen::Home,
                    {10.0f, 10.0f}, 720, 1080) ==
                    QuitDialogChoice::None,
                "letterboxed area never activates dialog buttons");
    ok &= Check(QuitDialogPointerChoice(Screen::Home,
                    {361.0f, 356.0f}, 0, 540) ==
                    QuitDialogChoice::None,
                "invalid window dimensions cannot activate a button");
    if (!ok) return 1;
    std::cout << "QUIT_POINTER_PASS viewport=1 portrait=1 modal=1\n";
    return 0;
}
