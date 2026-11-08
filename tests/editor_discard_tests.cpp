#include "app/editor_discard_guard.h"

#include <iostream>

using namespace locus::app;

namespace {
bool Check(bool condition, const char* what) {
    if (!condition) std::cerr << "EDITOR_DISCARD_FAIL " << what << '\n';
    return condition;
}
}

int main() {
    bool ok = true;
    EditorDiscardGuard guard;

    FrameInput home;
    home.homePressed = true;
    ok &= Check(guard.Handle(home, true, false, false) ==
                    EditorDiscardOutcome::PromptOpened &&
                guard.Pending() == EditorPending::Home &&
                !home.homePressed,
                "unsaved Home navigation opens confirmation and blocks exit");

    FrameInput waiting;
    waiting.editorOpen = true; // Other navigation must not reach the editor.
    ok &= Check(guard.Handle(waiting, true, false, false) ==
                    EditorDiscardOutcome::None &&
                guard.Pending() == EditorPending::Home &&
                !waiting.editorOpen,
                "pending confirmation blocks unrelated file open");

    FrameInput cancel;
    ok &= Check(guard.Handle(cancel, true, false, true) ==
                    EditorDiscardOutcome::ChangesKept &&
                guard.Pending() == EditorPending::None,
                "N/Escape cancels navigation but keeps the draft");

    FrameInput requestHome;
    requestHome.requestedScreen = Screen::SongSelect;
    ok &= Check(guard.Handle(requestHome, true, false, false) ==
                    EditorDiscardOutcome::PromptOpened &&
                guard.Pending() == EditorPending::Home &&
                !requestHome.requestedScreen,
                "unsaved navigation to other screens is protected");
    FrameInput confirmedHome;
    confirmedHome.requestedScreen = Screen::Skin;
    confirmedHome.editorNew = true; // Must never leak an unrelated command.
    ok &= Check(guard.Handle(confirmedHome, true, true, false) ==
                    EditorDiscardOutcome::Approved &&
                guard.Pending() == EditorPending::None &&
                confirmedHome.homePressed && !confirmedHome.editorNew &&
                !confirmedHome.requestedScreen,
                "Y accepts only the exact pending Home action");

    FrameInput requestNew;
    requestNew.editorNew = true;
    requestNew.editorOpen = true;
    ok &= Check(guard.Handle(requestNew, true, false, false) ==
                    EditorDiscardOutcome::PromptOpened &&
                guard.Pending() == EditorPending::NewDraft &&
                !requestNew.editorNew && !requestNew.editorOpen,
                "New Draft takes priority over Open Chart in the same frame");
    FrameInput confirmedNew;
    ok &= Check(guard.Handle(confirmedNew, true, true, false) ==
                    EditorDiscardOutcome::Approved &&
                confirmedNew.editorNew && !confirmedNew.editorOpen,
                "Y runs exactly the confirmed New Draft action");

    FrameInput requestOpen;
    requestOpen.editorOpen = true;
    ok &= Check(guard.Handle(requestOpen, true, false, false) ==
                    EditorDiscardOutcome::PromptOpened &&
                guard.Pending() == EditorPending::OpenChart &&
                !requestOpen.editorOpen,
                "unsaved Open Chart requires confirmation");
    FrameInput confirmedOpen;
    ok &= Check(guard.Handle(confirmedOpen, true, true, false) ==
                    EditorDiscardOutcome::Approved &&
                confirmedOpen.editorOpen && !confirmedOpen.homePressed,
                "Y permits exactly the confirmed Open Chart action");

    FrameInput cleanHome;
    cleanHome.homePressed = true;
    ok &= Check(guard.Handle(cleanHome, false, false, false) ==
                    EditorDiscardOutcome::None && cleanHome.homePressed &&
                guard.Pending() == EditorPending::None,
                "clean document navigates immediately");

    FrameInput openAgain;
    openAgain.editorOpen = true;
    guard.Handle(openAgain, true, false, false);
    FrameInput osClose;
    osClose.closeRequested = true;
    ok &= Check(guard.Handle(osClose, true, false, false) ==
                    EditorDiscardOutcome::None &&
                guard.Pending() == EditorPending::None && osClose.closeRequested,
                "OS close supersedes editor prompt and leaves global quit enabled");

    FrameInput sameScreen;
    sameScreen.requestedScreen = Screen::Editor;
    ok &= Check(guard.Handle(sameScreen, true, false, false) ==
                    EditorDiscardOutcome::None &&
                sameScreen.requestedScreen == Screen::Editor,
                "requested Editor screen is not a discard action");

    if (!ok) return 1;
    std::cout << "EDITOR_DISCARD_PASS home=1 open=1 new=1 cancel=1 os-close=1\n";
    return 0;
}
