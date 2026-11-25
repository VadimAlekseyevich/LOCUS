#include "core/judgement.h"
#include "core/replay.h"

#include <cmath>
#include <iostream>
#include <limits>

using namespace locus::core;
namespace {
bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "REPLAY_TEST_FAIL: " << message << '\n';
    return condition;
}
}
int main() {
    bool ok = true;
    Chart chart;
    chart.turns = {{2, Heading::UpRight, 1000}, {3, Heading::Up, 1100},
                   {4, Heading::Left, 2000}, {5, Heading::Down, 2500}};
    TurnJudge live(chart);
    live.SetOffsetSeconds(0.25);
    live.BeginReplayCapture();
    for (int i = 0; i < 10000; ++i) live.Advance(0.5);
    live.PressHeading(Heading::Right, 1.08);
    live.PressHeading(Heading::Up, 0.96); // dispatch order, not sorted time
    live.PressHeading(Heading::Up, 1.1);
    live.Advance(2.2);
    live.PressHeading(Heading::Right, 2.5); // wrong direction
    live.Advance(4.0);
    const auto* trace = live.CompletedReplay();
    ok &= Check(trace && trace->events.size() == 5,
                "capture dispatch order, no idle/post-completion events");
    if (!trace) return 1;
    std::string bytes;
    ReplayTrace decoded;
    ok &= Check(EncodeReplay(*trace, bytes) && DecodeReplay(bytes, decoded) &&
                decoded.events.size() == trace->events.size() &&
                decoded.events[1].songSeconds == 0.96 &&
                decoded.offsetSeconds == 0.25,
                "binary codec round trips exact double timestamps");
    TurnJudge playback(chart);
    ok &= Check(RejudgeReplay(decoded, playback) && playback.AllScorableJudged() &&
                playback.Counts().perfect == live.Counts().perfect &&
                playback.Counts().good == live.Counts().good &&
                playback.Counts().bad == live.Counts().bad &&
                playback.Counts().miss == live.Counts().miss &&
                playback.MaxCombo() == live.MaxCombo() &&
                playback.Statistics().meanErrorMs == live.Statistics().meanErrorMs &&
                playback.Statistics().unstableRate == live.Statistics().unstableRate,
                "replay reuses actual judge and preserves counts/combo/telemetry");
    const auto prior = decoded.events.size();
    ok &= Check(!DecodeReplay(std::string_view(bytes).substr(0,bytes.size()-1),decoded) &&
                decoded.events.size() == prior, "truncated trace leaves output intact");
    std::string corrupt = bytes;
    corrupt[kReplayHeaderBytes] = static_cast<char>(255);
    ok &= Check(!DecodeReplay(corrupt,decoded), "invalid action rejected");
    ReplayTrace incompatible = decoded;
    ++incompatible.judgementVersion;
    ok &= Check(!RejudgeReplay(incompatible,playback) && playback.AllScorableJudged(),
                "incompatible rules rejected without resetting playback");
    incompatible = decoded;
    incompatible.events[0].songSeconds = std::numeric_limits<double>::infinity();
    ok &= Check(!EncodeReplay(incompatible, bytes), "nonfinite timestamp rejected");
    live.Reset();
    ok &= Check(!live.CompletedReplay(), "reset discards recording");
    live.BeginReplayCapture();
    live.FinishRemainingMisses();
    trace = live.CompletedReplay();
    ok &= Check(trace && trace->events.size() == 1 &&
                RejudgeReplay(*trace,playback) && playback.Counts().miss == 5,
                "explicit finalization also replays");
    live.ResetFromAudioSeconds(2.0);
    live.BeginReplayCapture();
    live.FinishRemainingMisses();
    ok &= Check(!live.CompletedReplay(), "partial practice origin cannot capture full replay");
    return ok ? 0 : 1;
}
