#pragma once

#include "app/screen_flow.h"

namespace locus::app {

class GameSession;
class DemoTransport;
class ChartPlayback;
class LocalMusic;

// The exit modal must restore ONLY playback it paused itself. The policy
// contains no raylib state and is independently testable without audio.
enum class QuitMediaAction {
    None,
    PauseGameplay, ResumeGameplay,
    PauseListening, ResumeListening,
    PauseResults, ResumeResults,
    PauseEditorPreview, ResumeEditorPreview
};

struct QuitMediaSnapshot {
    Screen screen = Screen::Home;
    bool playing = false;
    bool paused = false;
};

class QuitMediaCoordinator {
public:
    // before/after describe the modal flag on adjacent frames. Confirmation
    // exits the application without invoking resume; dismissal does resume.
    QuitMediaAction OnModalTransition(bool before, bool after,
                                      QuitMediaSnapshot media) noexcept;
    void Reset() noexcept { pausedByModal_ = QuitMediaAction::None; }

private:
    QuitMediaAction pausedByModal_ = QuitMediaAction::None;
};

// Adapter for application composition: query the active transport once,
// decide the action, then apply it. Other screens never touch audio.
void UpdateQuitMedia(QuitMediaCoordinator& coordinator, bool previousModal,
                     bool currentModal, Screen screen, bool customChart,
                     double now, GameSession& session, DemoTransport& demo,
                     ChartPlayback& playback, LocalMusic& song);

} // namespace locus::app
