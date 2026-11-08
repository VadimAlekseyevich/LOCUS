#pragma once
#include "app/app_input.h"
#include "app/screen_flow.h"
#include "core/judgement.h"

namespace locus::app {
// Navigation and chart-completion policy has no dependency on rendering,
// audio devices or filesystem persistence. Used by the actual runtime
// and the headless scenario checks.
void UpdateState(AppState& state, const FrameInput& input, bool smokeWindow);
bool ChartReadyForResults(const core::TurnJudge& judge,
                          bool audioFinished) noexcept;
void FinishSession(AppState& state, core::TurnJudge& judge);
} // namespace locus::app
