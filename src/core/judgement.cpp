#include "core/judgement.h"

#include <algorithm>
#include <cmath>

namespace locus::core {

const char* GradeName(Grade grade) noexcept {
    switch (grade) {
        case Grade::Perfect: return "PERFECT";
        case Grade::Good: return "GOOD";
        case Grade::Bad: return "BAD";
        case Grade::Miss: return "MISS";
    }
    return "UNKNOWN";
}

const char* TurnName(Turn turn) noexcept {
    switch (turn) {
        case Turn::Left: return "LEFT 90";
        case Turn::Right: return "RIGHT 90";
        case Turn::Left45: return "LEFT 45";
        case Turn::Right45: return "RIGHT 45";
        case Turn::Left135: return "LEFT 135";
        case Turn::Right135: return "RIGHT 135";
    }
    return "UNKNOWN";
}

void TurnJudge::Reset() noexcept {
    nextEvent_ = 0;
    counts_ = GradeCounts{};
    last_.reset();
}

void TurnJudge::ResetFromBeat(int firstScoredBeat) noexcept {
    Reset();
    nextEvent_ = static_cast<std::size_t>(
        std::lower_bound(chart_.turns.begin(), chart_.turns.end(),
                         firstScoredBeat,
                         [](const TurnEvent& event, int beat) {
                             return event.beat < beat;
                         }) - chart_.turns.begin());
}

void TurnJudge::ResetFromAudioSeconds(double audioSeconds) noexcept {
    Reset();
    if (!std::isfinite(audioSeconds)) return;
    nextEvent_ = static_cast<std::size_t>(
        std::lower_bound(chart_.turns.begin(), chart_.turns.end(),
                         audioSeconds, [&](const TurnEvent& event, double seconds) {
                             const double time = event.timeMs == kNoTimeMs
                                 ? offsetSeconds_ + event.beat * 60.0 /
                                       (chart_.bpm * chart_.ticksPerBeat)
                                 : static_cast<double>(event.timeMs) / 1000.0;
                             return time < seconds - 1e-9;
                         }) - chart_.turns.begin());
}

const TurnEvent* TurnJudge::NextEvent() const noexcept {
    return nextEvent_ < chart_.turns.size() ? &chart_.turns[nextEvent_] : nullptr;
}

void TurnJudge::Resolve(Grade grade, double errorSeconds, bool wrongDirection) noexcept {
    const TurnEvent* event = NextEvent();
    if (!event) return;

    last_ = Judgement{grade, event->beat, errorSeconds, wrongDirection};
    ++nextEvent_;
    switch (grade) {
        case Grade::Perfect: ++counts_.perfect; break;
        case Grade::Good: ++counts_.good; break;
        case Grade::Bad: ++counts_.bad; break;
        case Grade::Miss: ++counts_.miss; break;
    }
}

void TurnJudge::Advance(double songSeconds) noexcept {
    if (!std::isfinite(songSeconds)) return;
    while (const TurnEvent* next = NextEvent()) {
        const double expectedSeconds = next->timeMs == kNoTimeMs
            ? offsetSeconds_ + next->beat * 60.0 /
                (chart_.bpm * chart_.ticksPerBeat)
            : static_cast<double>(next->timeMs) / 1000.0;
        if (songSeconds <= expectedSeconds + kBadWindowSeconds) break;
        Resolve(Grade::Miss, 0.0, false);
    }
}

void TurnJudge::FinishRemainingMisses() noexcept {
    while (NextEvent()) Resolve(Grade::Miss, 0.0, false);
}

std::optional<Judgement> TurnJudge::Press(Turn direction, double songSeconds) noexcept {
    if (!std::isfinite(songSeconds)) return std::nullopt;
    Advance(songSeconds);
    const TurnEvent* next = NextEvent();
    if (!next) return std::nullopt;

    const double expectedSeconds = next->timeMs == kNoTimeMs
            ? offsetSeconds_ + next->beat * 60.0 /
                (chart_.bpm * chart_.ticksPerBeat)
            : static_cast<double>(next->timeMs) / 1000.0;
    const double error = songSeconds - expectedSeconds;
    if (std::abs(error) > kBadWindowSeconds) {
        return std::nullopt; // Stray presses never consume chart turns.
    }

    Grade grade = Grade::Miss;
    const bool wrong = direction != next->direction;
    if (!wrong) {
        const double deviation = std::abs(error);
        if (deviation <= kPerfectWindowSeconds) grade = Grade::Perfect;
        else if (deviation <= kGoodWindowSeconds) grade = Grade::Good;
        else grade = Grade::Bad;
    }
    Resolve(grade, error, wrong);
    return last_;
}

std::optional<Judgement> TurnJudge::PressHeading(Heading outgoing,
                                                 double songSeconds) {
    if (!ValidHeading(outgoing) || !std::isfinite(songSeconds)) return std::nullopt;
    Advance(songSeconds);
    const TurnEvent* next = NextEvent();
    if (!next) return std::nullopt;

    const double expectedSeconds = next->timeMs == kNoTimeMs
            ? offsetSeconds_ + next->beat * 60.0 /
                (chart_.bpm * chart_.ticksPerBeat)
            : static_cast<double>(next->timeMs) / 1000.0;
    const double error = songSeconds - expectedSeconds;
    if (std::abs(error) > kBadWindowSeconds) return std::nullopt;

    const Heading incoming = IncomingHeadingAt(chart_, next->beat);
    const Heading expected = HeadingAfterTurn(incoming, next->direction);
    const bool wrong = outgoing != expected;
    Grade grade = Grade::Miss;
    if (!wrong) {
        const double deviation = std::abs(error);
        if (deviation <= kPerfectWindowSeconds) grade = Grade::Perfect;
        else if (deviation <= kGoodWindowSeconds) grade = Grade::Good;
        else grade = Grade::Bad;
    }
    Resolve(grade, error, wrong);
    return last_;
}

} // namespace locus::core
