#pragma once

#include "app/screen_flow.h"

#include <string>

namespace locus::core { struct Chart; class TurnJudge; }
namespace locus::app {
class GameSession;
class ChartPlayback;
class LocalMusic;
struct EditorUiState;

// Non-owning application composition: every reference outlives the main
// event loop. Chart/judge/game-session references stay stable across loads.
struct SessionTransitionContext {
    core::Chart& activeChart;
    const core::Chart& demoChart;
    core::TurnJudge& judge;
    GameSession& gameSession;
    ChartPlayback& chartPlayback;
    LocalMusic& song;
    EditorUiState& editorUi;
    bool& customChart;
    bool& sessionCompleted;
    std::string& chartTitle;
};

// The only place that responds to a screen boundary by starting/stopping
// audio transports and scoring. Does NOT choose screens, process input, or
// mutate the editor document: callers have already committed navigation.
void ApplySessionTransition(const SessionTransitionContext& context,
                            Screen previous, Screen next,
                            bool loadedNewChart, double now);

} // namespace locus::app
