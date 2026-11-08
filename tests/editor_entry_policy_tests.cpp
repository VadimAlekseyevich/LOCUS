#include "app/editor_entry_policy.h"
#include <iostream>
using namespace locus::app;
int main() {
    bool ok = true;
    const auto check = [&](bool yes, const char* name) {
        if (!yes) std::cerr << "EDITOR_ENTRY_FAIL " << name << '\n';
        ok &= yes;
    };
    check(ChooseEditorEntry(Screen::Home, Screen::Editor, false, true, true) ==
          EditorEntryAction::NewDraft, "Home creates clean draft");
    check(ChooseEditorEntry(Screen::SongSelect, Screen::Editor, false, true, true) ==
          EditorEntryAction::KeepLoaded, "explicit library Edit preserves loaded chart");
    check(ChooseEditorEntry(Screen::Play, Screen::Editor, false, true, true) ==
          EditorEntryAction::OpenPlayedDifficulty &&
          ChooseEditorEntry(Screen::Results, Screen::Editor, false, true, true) ==
          EditorEntryAction::OpenPlayedDifficulty,
          "Play and Results Editor reopen played difficulty");
    check(ChooseEditorEntry(Screen::Results, Screen::Editor, true, true, true) ==
          EditorEntryAction::KeepLoaded,
          "returning from Editor TEST preserves unsaved changes");
    check(ChooseEditorEntry(Screen::Play, Screen::Editor, false, false, false) ==
          EditorEntryAction::NewDraft,
          "demo playback enters blank editor");
    check(ChooseEditorEntry(Screen::Play, Screen::SongSelect, false, true, true) ==
          EditorEntryAction::KeepLoaded,
          "ordinary navigation leaves document unchanged");
    if (ok) std::cout << "EDITOR_ENTRY_PASS home=1 library=1 test=1\n";
    return ok ? 0 : 1;
}
