#include "app/run_result_model.h"
#include "core/chart_revision.h"
#include <iostream>
#include <limits>

using namespace locus;
int main() {
    core::LoadedChart chart;
    chart.route.turns={{2,core::Heading::Right,1000}};
    core::TurnJudge judge(chart.route);
    judge.BeginReplayCapture();
    judge.PressHeading(core::Heading::Right,1.0);
    app::CompletedRunContext context;
    context.completedThisFrame=true;
    context.key={"set","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    context.songTitle="Song"; context.chartTitle="Hard";
    context.gameplayRevision=core::ChartGameplayRevision(chart);
    context.grades=judge.Counts(); context.scorableCount=judge.ScorableCount();
    context.statistics=judge.Statistics(); context.replay=judge.CompletedReplay();
    std::string error;
    auto result=app::BuildCompletedRun(context,error);
    if (!result || result->accuracyPercent!=100.0 || !result->id.empty() ||
        result->completedAtMs!=0 || !context.replay) {
        std::cerr<<"Could not build pure completed-run snapshot: "<<error<<'\n'; return 1;
    }
    result->id="1000-0"; result->completedAtMs=1000;
    if (!app::ValidRunResult(*result) ||
        !app::VerifyRunReplay(*result,*context.replay,chart.route,context.gameplayRevision,0.0)) {
        std::cerr<<"Portable score model did not reproduce completed run\n"; return 1;
    }
    context.spectator=true;
    if (app::BuildCompletedRun(context,error) || !error.empty()) return 1;
    context.spectator=false;
    context.grades.perfect=std::numeric_limits<int>::max();
    context.grades.good=std::numeric_limits<int>::max();
    if (app::BuildCompletedRun(context,error)) return 1;
    return 0;
}
