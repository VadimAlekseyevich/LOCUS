#pragma once

#include "app/app_input.h"
#include "app/screen_flow.h"
#include "app/play_outro_transition.h"

namespace locus::core { class TurnJudge; }
namespace locus::app {
class GameSession;
class DemoTransport;
class ChartPlayback;

// Read-only summary of the current scored transport, used to build a
// GameplayView. The controller never draws, changes chart files, or touches
// a native window.
struct PlayTransportView {
    double songSeconds = 0.0;
    double offsetSeconds = 0.0;
    bool paused = false;
    bool finished = false;
    double practiceSpeed = 1.0;
    const char* status = "";
};

double PracticeRateForIndex(int index) noexcept;

// Non-owning, strictly Play-screen coordination. All references are owned by
// the composition root and remain stable when a different chart is loaded.
struct PlayActionsContext {
    AppState& app;
    GameSession& game;
    core::TurnJudge& judge;
    DemoTransport& demo;
    ChartPlayback& local;
    bool& customChart;
    bool& sessionCompleted;
    const core::Chart& chart;
    PlayOutroTransition& outro;
};

class PlayScreenActions {
public:
    explicit PlayScreenActions(PlayActionsContext context) noexcept
        : context_(context) {}

    // Used on frames which stay on Play, not at a screen boundary.
    void HandleControls(const FrameInput& input, double now);
    // Scoring also runs on the first frame of a newly entered Play screen.
    void Update(const FrameInput& input, double now);
    PlayTransportView Transport(double now) const;

private:
    PlayActionsContext context_;
};

} // namespace locus::app
