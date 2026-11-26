#include "core/performance.h"
#include "app/run_result_model.h"
#include <cmath>
#include <iostream>

using namespace locus;
namespace {
bool Check(bool ok,const char* message) {
    if (!ok) std::cerr<<"PERFORMANCE_FAIL: "<<message<<'\n';
    return ok;
}
}
int main() {
    using namespace core;
    Chart chart;
    for (int i=0;i<240;++i)
        chart.turns.push_back({i+1,i%2?Heading::Up:Heading::Right,1000+100*i});
    const auto nm=CalculatePerformanceDifficulty(chart,0,100,0,0);
    const auto dt=CalculatePerformanceDifficulty(chart,0,125,0,0);
    const auto ht=CalculatePerformanceDifficulty(chart,0,75,0,0);
    if (!nm || !dt || !ht || nm->keyPresses!=240) return 1;
    const GradeCounts ss{240,0,0,0}, good{200,40,0,0}, miss{200,0,0,40};
    const auto perfect=CalculatePerformance(*nm,ss,100,0);
    const auto faster=CalculatePerformance(*dt,ss,125,0);
    const auto slower=CalculatePerformance(*ht,ss,75,0);
    const auto imperfect=CalculatePerformance(*nm,good,100,0);
    const auto missed=CalculatePerformance(*nm,miss,100,0);
    if (!perfect || !faster || !slower || !imperfect || !missed) return 1;
    bool ok=Check(*faster>*perfect && *perfect>*slower &&
        *perfect>*imperfect && *imperfect>*missed,"physical strain, accuracy and misses");
    ok &= Check(CalculatePerformance(*nm,ss,100,ModBit(PlayMod::Hidden))==perfect &&
        CalculatePerformance(*nm,ss,100,ModBit(PlayMod::Flashlight))==perfect &&
        CalculatePerformance(*nm,ss,100,ModBit(PlayMod::Easy)).value_or(0)<*perfect &&
        !CalculatePerformance(*nm,ss,100,ModBit(PlayMod::Autoplay)) &&
        !CalculatePerformance(*nm,miss,100,ModBit(PlayMod::SuddenDeath)) &&
        !CalculatePerformance(*nm,{239,0,0,0},100,0),
        "modifier policy and incomplete score rejection");
    app::RunResult run;
    run.id="11111111111111111111111111111111";
    run.playerId="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    run.songSetId="set"; run.difficultyId="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    run.gameplayRevision="cccccccccccccccccccccccccccccccc";
    run.songTitle="Song"; run.chartTitle="Hard";
    run.completedAtMs=1; run.grades=ss; run.notesJudged=240;
    run.accuracyPercent=100.0; run.performanceDifficulty=nm;
    auto second=run;
    second.id="22222222222222222222222222222222";
    second.grades=good; second.accuracyPercent=good.AccuracyPercent();
    auto third=run;
    third.id="33333333333333333333333333333333";
    third.difficultyId="dddddddddddddddddddddddddddddddd";
    auto foreign=third;
    foreign.id="44444444444444444444444444444444";
    foreign.playerId="eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
    const auto summary=app::SummarizePlayerPerformance(run.playerId,{run,second,third,foreign});
    ok &= Check(summary.playCount==3 && summary.ratedDifficulties==2 &&
        std::abs(summary.totalPp-*perfect*1.95)<1e-8 &&
        std::abs(summary.weightedAccuracy-(200.0+good.AccuracyPercent())/3.0)<1e-8,
        "one best result per difficulty; foreign owners excluded");
    run.performanceDifficulty.reset();
    ok &= Check(!app::RunPerformance(run),"older score without difficulty snapshot remains unrated");
    return ok?0:1;
}
