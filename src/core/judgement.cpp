#include "core/judgement.h"

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
    return (turn == Turn::Left) ? "LEFT" : "RIGHT";
}

void TurnJudge::Reset() noexcept {
    nextEvent_ = 0;
    counts_ = GradeCounts{};
    last_.reset();
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
        const double expectedSeconds = offsetSeconds_ + next->beat * 60.0 / chart_.bpm;
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

    const double expectedSeconds = offsetSeconds_ + next->beat * 60.0 / chart_.bpm;
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

} // namespace locus::core
