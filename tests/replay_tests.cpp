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
    live.BeginReplayCapture(125);
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
                decoded.ratePercent == 125 && decoded.events.size() == trace->events.size() &&
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
    ReplayCursor cursor;
    TurnJudge spectator(chart);
    spectator.SetOffsetSeconds(decoded.offsetSeconds);
    ok &= Check(!cursor.NextDue(decoded,1.0,3.0),
                "out-of-order earlier input cannot leapfrog a later dispatch");
    int delivered=0;
    while (const auto* event=cursor.NextDue(decoded,1.08,3.0)) {
        ApplyReplayEvent(spectator,*event);
        ++delivered;
    }
    ok &= Check(delivered==2 && spectator.Counts().Resolved()==2,
                "earlier input follows its predecessor in the same viewer update");
    while (const auto* event=cursor.NextDue(decoded,10.0,3.0))
        ApplyReplayEvent(spectator,*event);
    ok &= Check(cursor.Done(decoded) && spectator.AllScorableJudged() &&
                spectator.MaxCombo()==live.MaxCombo() &&
                spectator.Counts().miss==live.Counts().miss,
                "incremental spectator matches offline replay result");
    cursor.Reset();
    ok &= Check(!cursor.Done(decoded) && cursor.NextDue(decoded,0.0,3.0,true),
                "retry resets cursor; bounded silent tail can drain late operations");

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
    PlayMods mods;
    mods.Toggle(PlayMod::Easy);
    mods.Toggle(PlayMod::HardRock);
    mods.Toggle(PlayMod::Hidden);
    mods.Toggle(PlayMod::Flashlight);
    ok &= Check(ValidModFlags(mods.flags) && HasMod(mods.flags,PlayMod::HardRock) &&
                HasMod(mods.flags,PlayMod::Flashlight) && !HasMod(mods.flags,PlayMod::Easy),
                "exclusive groups preserve independent modifiers");
    Chart one;
    one.turns = {{2, Heading::Right, 1000}};
    TurnJudge modJudge(one), normalJudge(one), hardJudge(one);
    modJudge.SetMods(ModBit(PlayMod::Easy)|ModBit(PlayMod::Hidden));
    hardJudge.SetMods(ModBit(PlayMod::HardRock));
    modJudge.BeginReplayCapture(75);
    const auto easyHit = modJudge.PressHeading(Heading::Right,1.06);
    const auto normalHit = normalJudge.PressHeading(Heading::Right,1.06);
    const auto hardHit = hardJudge.PressHeading(Heading::Right,1.06);
    ok &= Check(easyHit && normalHit && hardHit && easyHit->grade == Grade::Perfect &&
                normalHit->grade == Grade::Good && hardHit->grade == Grade::Good,
                "EZ and HR apply distinct timing windows");
    const auto* modTrace = modJudge.CompletedReplay();
    TurnJudge verified(one);
    ok &= Check(modTrace && EncodeReplay(*modTrace,bytes) && DecodeReplay(bytes,decoded) &&
                decoded.modFlags == (ModBit(PlayMod::Easy)|ModBit(PlayMod::Hidden)) &&
                decoded.ratePercent == 75 && RejudgeReplay(decoded,verified) &&
                verified.Mods() == decoded.modFlags && verified.Counts().perfect == 1,
                "modded replay restores scoring and presentation configuration");
    const auto randomFlags=ModBit(PlayMod::Random);
    const auto randomA=TransformModChart(chart,randomFlags,123);
    const auto randomB=TransformModChart(chart,randomFlags,123);
    bool deterministic=true;
    for (std::size_t i=0;i<chart.turns.size();++i)
        deterministic &= randomA.turns[i].direction==randomB.turns[i].direction &&
            randomA.turns[i].timeMs==chart.turns[i].timeMs &&
            IsDiagonal(randomA.turns[i].direction)==IsDiagonal(chart.turns[i].direction);
    ok &= Check(deterministic,"random preserves rhythm and cardinality with fixed seed");
    TurnJudge sudden(one), perfect(one);
    sudden.SetMods(ModBit(PlayMod::SuddenDeath));
    sudden.Advance(2.0);
    perfect.SetMods(ModBit(PlayMod::Perfect));
    perfect.PressHeading(Heading::Right,1.06);
    ok &= Check(sudden.Failed() && perfect.Failed(),"SD and PF fail on their respective grades");
    decoded.modFlags = 3;
    ok &= Check(!EncodeReplay(decoded,bytes), "conflicting mods rejected");
    live.ResetFromAudioSeconds(2.0);
    live.BeginReplayCapture();
    live.FinishRemainingMisses();
    ok &= Check(!live.CompletedReplay(), "partial practice origin cannot capture full replay");
    return ok ? 0 : 1;
}
