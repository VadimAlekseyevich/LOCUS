#include "app/quit_media.h"

#include "app/chart_playback.h"
#include "app/demo_transport.h"
#include "app/game_session.h"
#include "app/local_music.h"

namespace locus::app {

QuitMediaAction QuitMediaCoordinator::OnModalTransition(
    bool before, bool after, QuitMediaSnapshot media) noexcept {
    if (!before && after) {
        // Beginning a new confirmation forgets any previous cancellation.
        pausedByModal_ = QuitMediaAction::None;
        if (!media.playing || media.paused) return QuitMediaAction::None;
        switch (media.screen) {
            case Screen::Play:
                pausedByModal_ = QuitMediaAction::PauseGameplay;
                return pausedByModal_;
            case Screen::Results:
                pausedByModal_ = QuitMediaAction::PauseResults;
                return pausedByModal_;
            case Screen::Listen:
                pausedByModal_ = QuitMediaAction::PauseListening;
                return pausedByModal_;
            case Screen::Editor:
                pausedByModal_ = QuitMediaAction::PauseEditorPreview;
                return pausedByModal_;
            default:
                return QuitMediaAction::None;
        }
    }

    if (before && !after) {
        // The current transport cannot be changed from under the dialog:
        // input is blocked by UpdateScreenFlow while confirmation is open.
        const auto paused = pausedByModal_;
        pausedByModal_ = QuitMediaAction::None;
        switch (paused) {
            case QuitMediaAction::PauseGameplay:
                return QuitMediaAction::ResumeGameplay;
            case QuitMediaAction::PauseResults:
                return QuitMediaAction::ResumeResults;
            case QuitMediaAction::PauseListening:
                return QuitMediaAction::ResumeListening;
            case QuitMediaAction::PauseEditorPreview:
                return QuitMediaAction::ResumeEditorPreview;
            default:
                return QuitMediaAction::None;
        }
    }
    return QuitMediaAction::None;
}

void UpdateQuitMedia(QuitMediaCoordinator& coordinator, bool previousModal,
                     bool currentModal, Screen screen, bool customChart,
                     double now, GameSession& session, DemoTransport& demo,
                     ChartPlayback& playback, LocalMusic& song) {
    if (previousModal == currentModal) return;

    QuitMediaSnapshot snapshot;
    snapshot.screen = screen;
    if (!previousModal && currentModal) {
        switch (screen) {
            case Screen::Play:
                snapshot.playing = customChart ? playback.Playing() : demo.Playing();
                snapshot.paused = customChart ? playback.Paused() : demo.Paused();
                break;
            case Screen::Results:
                snapshot.playing = customChart ? playback.Playing() : demo.Playing();
                snapshot.paused = customChart ? playback.Paused() : demo.Paused();
                break;
            case Screen::Listen:
                snapshot.playing = song.Playing();
                snapshot.paused = song.Paused();
                break;
            case Screen::Editor:
                snapshot.playing = playback.Playing();
                snapshot.paused = playback.Paused();
                break;
            default:
                break;
        }
    }

    switch (coordinator.OnModalTransition(
        previousModal, currentModal, snapshot)) {
        case QuitMediaAction::PauseGameplay:
        case QuitMediaAction::ResumeGameplay:
            session.TogglePause(customChart, now);
            break;
        case QuitMediaAction::PauseResults:
        case QuitMediaAction::ResumeResults:
            session.TogglePause(customChart, now);
            break;
        case QuitMediaAction::PauseListening: song.Pause(now); break;
        case QuitMediaAction::ResumeListening: song.Resume(now); break;
        case QuitMediaAction::PauseEditorPreview: playback.Pause(now); break;
        case QuitMediaAction::ResumeEditorPreview: playback.Resume(now); break;
        case QuitMediaAction::None: break;
    }
}

} // namespace locus::app
