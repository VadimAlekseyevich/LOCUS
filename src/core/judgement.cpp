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

const char* DirectionName(Heading direction) noexcept {
    switch (direction) {
        case Heading::Up: return "UP";
        case Heading::Right: return "RIGHT";
        case Heading::Down: return "DOWN";
        case Heading::Left: return "LEFT";
        case Heading::UpRight: return "UP RIGHT";
        case Heading::DownRight: return "DOWN RIGHT";
        case Heading::DownLeft: return "DOWN LEFT";
        case Heading::UpLeft: return "UP LEFT";
    }
    return "UNKNOWN";
}

void TurnJudge::Reset() noexcept {
    nextEvent_ = 0;
    firstScoredEvent_ = 0;
    resolvedArrows_ = 0u;
    counts_ = GradeCounts{};
    last_.reset();
    RecountScorable();
}

void TurnJudge::ResetFromBeat(int firstScoredBeat) noexcept {
    Reset();
    nextEvent_ = static_cast<std::size_t>(
        std::lower_bound(chart_.turns.begin(), chart_.turns.end(),
                         firstScoredBeat,
                         [](const TurnEvent& event, int beat) {
                             return event.beat < beat;
                         }) - chart_.turns.begin());
    firstScoredEvent_ = nextEvent_;
    RecountScorable();
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
    firstScoredEvent_ = nextEvent_;
    RecountScorable();
}

const TurnEvent* TurnJudge::NextEvent() const noexcept {
    return nextEvent_ < chart_.turns.size() ? &chart_.turns[nextEvent_] : nullptr;
}

namespace {
unsigned FirstArrow(unsigned mask) noexcept {
    return mask & (~mask + 1u);
}

Heading CardinalArrow(unsigned mask) noexcept {
    switch (mask) {
        case 1u: return Heading::Up;
        case 2u: return Heading::Right;
        case 4u: return Heading::Down;
        default: return Heading::Left;
    }
}

int ArrowCount(unsigned mask) noexcept {
    int count = 0;
    while (mask) {
        mask &= mask - 1u;
        ++count;
    }
    return count;
}
} // namespace

void TurnJudge::RecountScorable() noexcept {
    scorableCount_ = 0;
    for (std::size_t i = firstScoredEvent_; i < chart_.turns.size(); ++i)
        scorableCount_ += static_cast<std::size_t>(
            ArrowCount(HeadingArrowMask(chart_.turns[i].direction)));
}

void TurnJudge::Resolve(Grade grade, double errorSeconds, bool wrongDirection,
                        Heading arrow) noexcept {
    const TurnEvent* event = NextEvent();
    if (!event) return;
    const unsigned pending = HeadingArrowMask(event->direction) & ~resolvedArrows_;
    const unsigned bit = HeadingArrowMask(arrow);
    if ((pending & bit) == 0u || IsDiagonal(arrow)) return;

    last_ = Judgement{grade, event->beat, errorSeconds, wrongDirection, arrow};
    resolvedArrows_ |= bit;
    switch (grade) {
        case Grade::Perfect: ++counts_.perfect; break;
        case Grade::Good: ++counts_.good; break;
        case Grade::Bad: ++counts_.bad; break;
        case Grade::Miss: ++counts_.miss; break;
    }
    if ((resolvedArrows_ & HeadingArrowMask(event->direction)) ==
        HeadingArrowMask(event->direction)) {
        ++nextEvent_;
        resolvedArrows_ = 0u;
    }
}

void TurnJudge::Advance(double songSeconds) noexcept {
    if (!std::isfinite(songSeconds)) return;
    while (const TurnEvent* event = NextEvent()) {
        const double expectedSeconds = event->timeMs == kNoTimeMs
            ? offsetSeconds_ + event->beat * 60.0 /
                (chart_.bpm * chart_.ticksPerBeat)
            : static_cast<double>(event->timeMs) / 1000.0;
        if (songSeconds <= expectedSeconds + kBadWindowSeconds) break;
        const unsigned remaining =
            HeadingArrowMask(event->direction) & ~resolvedArrows_;
        if (!remaining) break;
        Resolve(Grade::Miss, 0.0, false, CardinalArrow(FirstArrow(remaining)));
    }
}

void TurnJudge::FinishRemainingMisses() noexcept {
    while (const TurnEvent* event = NextEvent()) {
        const unsigned pending = HeadingArrowMask(event->direction) & ~resolvedArrows_;
        if (!pending) break;
        Resolve(Grade::Miss, 0.0, false, CardinalArrow(FirstArrow(pending)));
    }
}

std::optional<Judgement> TurnJudge::PressHeading(Heading outgoing,
                                                 double songSeconds) {
    if (!ValidHeading(outgoing) || IsDiagonal(outgoing) ||
        !std::isfinite(songSeconds)) return std::nullopt;
    Advance(songSeconds);
    const TurnEvent* next = NextEvent();
    if (!next) return std::nullopt;

    const double expectedSeconds = next->timeMs == kNoTimeMs
        ? offsetSeconds_ + next->beat * 60.0 /
            (chart_.bpm * chart_.ticksPerBeat)
        : static_cast<double>(next->timeMs) / 1000.0;
    const double error = songSeconds - expectedSeconds;
    if (std::abs(error) > kBadWindowSeconds) return std::nullopt;

    const unsigned required = HeadingArrowMask(next->direction);
    const unsigned input = HeadingArrowMask(outgoing);
    const unsigned remaining = required & ~resolvedArrows_;
    if (remaining & input) {
        const double deviation = std::abs(error);
        const Grade grade = deviation <= kPerfectWindowSeconds ? Grade::Perfect
            : deviation <= kGoodWindowSeconds ? Grade::Good : Grade::Bad;
        Resolve(grade, error, false, outgoing);
        return last_;
    }
    // A duplicate component never scores twice. Other key presses during
    // a diagonal do not destroy either legitimate component's hit window.
    if (IsDiagonal(next->direction) || (required & input)) return std::nullopt;
    Resolve(Grade::Miss, error, true, CardinalArrow(FirstArrow(remaining)));
    return last_;
}

} // namespace locus::core
