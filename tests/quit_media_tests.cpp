#include "app/quit_media.h"

#include "app/screen_flow.h"

#include <iostream>

using locus::app::QuitMediaAction;
using locus::app::QuitMediaCoordinator;
using locus::app::QuitMediaSnapshot;
using locus::app::Screen;

namespace {
bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "QUIT_MEDIA_FAIL " << label << '\n';
    return condition;
}
}

int main() {
    bool ok = true;
    QuitMediaCoordinator modal;
    const auto apply = [&](bool previous, bool current, Screen screen,
                           bool playing, bool paused) {
        return modal.OnModalTransition(
            previous, current, QuitMediaSnapshot{screen, playing, paused});
    };

    // Gameplay: exactly one pause and one resume even if the modal renders
    // for many frames. Resume is allowed only after user cancels the dialog.
    ok &= Check(apply(false, true, Screen::Play, true, false) ==
                    QuitMediaAction::PauseGameplay,
                "active gameplay pauses when confirmation opens");
    for (int i = 0; i < 4; ++i)
        ok &= Check(apply(true, true, Screen::Play, true, true) ==
                        QuitMediaAction::None,
                    "modal frames never toggle gameplay pause again");
    ok &= Check(apply(true, false, Screen::Play, true, true) ==
                    QuitMediaAction::ResumeGameplay,
                "canceled confirmation resumes gameplay exactly once");
    ok &= Check(apply(false, false, Screen::Play, true, false) ==
                    QuitMediaAction::None,
                "normal frames do not resume gameplay again");

    // An existing manual pause must survive cancellation unchanged.
    ok &= Check(apply(false, true, Screen::Play, true, true) ==
                    QuitMediaAction::None &&
                apply(true, false, Screen::Play, true, true) ==
                    QuitMediaAction::None,
                "manual gameplay pause is never undone by exit modal");

    // A finished/stopped session is not a playing session. This prevents the
    // previous implementation's spurious TogglePause on stopped gameplay.
    ok &= Check(apply(false, true, Screen::Play, false, false) ==
                    QuitMediaAction::None &&
                apply(true, false, Screen::Play, false, false) ==
                    QuitMediaAction::None,
                "stopped gameplay is not paused or resumed");

    ok &= Check(apply(false, true, Screen::Listen, true, false) ==
                    QuitMediaAction::PauseListening &&
                apply(true, false, Screen::Listen, true, true) ==
                    QuitMediaAction::ResumeListening,
                "listening screen owns its pause/resume pair");
    ok &= Check(apply(false, true, Screen::Listen, true, true) ==
                    QuitMediaAction::None &&
                apply(true, false, Screen::Listen, true, true) ==
                    QuitMediaAction::None,
                "already-paused listening is left paused");

    ok &= Check(apply(false, true, Screen::Editor, true, false) ==
                    QuitMediaAction::PauseEditorPreview &&
                apply(true, false, Screen::Editor, true, true) ==
                    QuitMediaAction::ResumeEditorPreview,
                "editor preview resumes only if modal paused it");
    ok &= Check(apply(false, true, Screen::Editor, false, false) ==
                    QuitMediaAction::None &&
                apply(true, false, Screen::Editor, false, false) ==
                    QuitMediaAction::None,
                "editor with no preview cannot start audio on cancel");

    for (Screen screen : {Screen::Home, Screen::SongSelect, Screen::Skin,
                          Screen::Settings, Screen::Results}) {
        ok &= Check(apply(false, true, screen, true, false) ==
                        QuitMediaAction::None &&
                    apply(true, false, screen, true, false) ==
                        QuitMediaAction::None,
                    "non-media screens never affect playback");
    }

    // Confirmation terminates the application directly; never request
    // resume on a frame where the exit modal is still open.
    ok &= Check(apply(false, true, Screen::Play, true, false) ==
                    QuitMediaAction::PauseGameplay &&
                apply(true, true, Screen::Play, true, true) ==
                    QuitMediaAction::None,
                "confirmed exit does not resume the audio");
    modal.Reset();
    ok &= Check(apply(true, false, Screen::Play, true, true) ==
                    QuitMediaAction::None,
                "reset state cannot resume a stale transport");
    if (!ok) return 1;
    std::cout << "QUIT_MEDIA_PASS play=1 listen=1 editor=1 manual-pause=1 stopped=1\n";
    return 0;
}
