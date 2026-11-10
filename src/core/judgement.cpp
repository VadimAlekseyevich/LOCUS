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
    firstScoredEvent_ = 0;
    counts_ = GradeCounts{};
    last_.reset();
    RecountScorable();
    ResetArrowQueues();
}

void TurnJudge::ResetFromBeat(int firstScoredBeat) noexcept {
    Reset();
    firstScoredEvent_ = static_cast<std::size_t>(
        std::lower_bound(chart_.turns.begin(), chart_.turns.end(),
                         firstScoredBeat,
                         [](const TurnEvent& event, int beat) {
                             return event.beat < beat;
                         }) - chart_.turns.begin());
    RecountScorable();
    ResetArrowQueues();
}

void TurnJudge::ResetFromAudioSeconds(double audioSeconds) noexcept {
    Reset();
    if (!std::isfinite(audioSeconds)) return;
    firstScoredEvent_ = static_cast<std::size_t>(
        std::lower_bound(chart_.turns.begin(), chart_.turns.end(),
                         audioSeconds, [&](const TurnEvent& event, double seconds) {
                             const double time = event.timeMs == kNoTimeMs
                                 ? offsetSeconds_ + event.beat * 60.0 /
                                       (chart_.bpm * chart_.ticksPerBeat)
                                 : static_cast<double>(event.timeMs) / 1000.0;
                             return time < seconds - 1e-9;
                         }) - chart_.turns.begin());
    RecountScorable();
    ResetArrowQueues();
}

namespace {
constexpr Heading kArrows[4] = {
    Heading::Up, Heading::Right, Heading::Down, Heading::Left
};

int ArrowIndex(Heading arrow) noexcept {
    switch (arrow) {
        case Heading::Up: return 0;
        case Heading::Right: return 1;
        case Heading::Down: return 2;
        case Heading::Left: return 3;
        default: return -1;
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

std::size_t TurnJudge::FindNextArrow(std::size_t from,
                                           unsigned bit) const noexcept {
    while (from < chart_.turns.size() &&
           (HeadingArrowMask(chart_.turns[from].direction) & bit) == 0u)
        ++from;
    return from;
}

void TurnJudge::ResetArrowQueues() noexcept {
    for (int i = 0; i < 4; ++i)
        nextByArrow_[i] = FindNextArrow(firstScoredEvent_, 1u << i);
}

std::size_t TurnJudge::NextEventIndex() const noexcept {
    return *std::min_element(nextByArrow_.begin(), nextByArrow_.end());
}

const TurnEvent* TurnJudge::NextEvent() const noexcept {
    const auto index = NextEventIndex();
    return index < chart_.turns.size() ? &chart_.turns[index] : nullptr;
}

void TurnJudge::Resolve(Grade grade, double errorSeconds, bool wrongDirection,
                        Heading arrow) noexcept {
    const int lane = ArrowIndex(arrow);
    if (lane < 0) return;
    const std::size_t at = nextByArrow_[lane];
    if (at >= chart_.turns.size()) return;
    last_ = Judgement{grade, chart_.turns[at].beat, errorSeconds,
                      wrongDirection, arrow};
    nextByArrow_[lane] = FindNextArrow(at + 1, 1u << lane);
    switch (grade) {
        case Grade::Perfect: ++counts_.perfect; break;
        case Grade::Good: ++counts_.good; break;
        case Grade::Bad: ++counts_.bad; break;
        case Grade::Miss: ++counts_.miss; break;
    }
}

void TurnJudge::Advance(double songSeconds) noexcept {
    if (!std::isfinite(songSeconds)) return;
    // Expire the oldest unresolved authored turn first. Each of its
    // remaining arrow components receives its own Miss.
    while (const TurnEvent* event = NextEvent()) {
        const double expectedSeconds = event->timeMs == kNoTimeMs
            ? offsetSeconds_ + event->beat * 60.0 /
                (chart_.bpm * chart_.ticksPerBeat)
            : static_cast<double>(event->timeMs) / 1000.0;
        if (songSeconds <= expectedSeconds + kBadWindowSeconds) break;
        const auto index = NextEventIndex();
        for (int i = 0; i < 4; ++i)
            if (nextByArrow_[i] == index)
                Resolve(Grade::Miss, 0.0, false, kArrows[i]);
    }
}

void TurnJudge::FinishRemainingMisses() noexcept {
    while (NextEvent()) {
        const auto index = NextEventIndex();
        for (int i = 0; i < 4; ++i)
            if (nextByArrow_[i] == index)
                Resolve(Grade::Miss, 0.0, false, kArrows[i]);
    }
}

std::optional<Judgement> TurnJudge::PressHeading(Heading outgoing,
                                                 double songSeconds) {
    const int lane = ArrowIndex(outgoing);
    if (lane < 0 || !std::isfinite(songSeconds)) return std::nullopt;
    Advance(songSeconds);

    // First judge the next expected note on this particular key, which may
    // belong to a LATER authored turn while another key has a pending half.
    const std::size_t at = nextByArrow_[lane];
    if (at < chart_.turns.size()) {
        const TurnEvent& target = chart_.turns[at];
        const double targetSeconds = target.timeMs == kNoTimeMs
            ? offsetSeconds_ + target.beat * 60.0 /
                (chart_.bpm * chart_.ticksPerBeat)
            : static_cast<double>(target.timeMs) / 1000.0;
        const double error = songSeconds - targetSeconds;
        if (std::abs(error) <= kBadWindowSeconds) {
            const double deviation = std::abs(error);
            const Grade grade = deviation <= kPerfectWindowSeconds ? Grade::Perfect
                : deviation <= kGoodWindowSeconds ? Grade::Good : Grade::Bad;
            Resolve(grade, error, false, outgoing);
            return last_;
        }
    }

    // Preserve the old wrong-direction rule for cardinal targets, but never
    // penalize a still-pending diagonal half for an unrelated key press.
    const TurnEvent* oldest = NextEvent();
    if (!oldest || IsDiagonal(oldest->direction) ||
        oldest->direction == outgoing) return std::nullopt;
    const double expectedSeconds = oldest->timeMs == kNoTimeMs
        ? offsetSeconds_ + oldest->beat * 60.0 /
            (chart_.bpm * chart_.ticksPerBeat)
        : static_cast<double>(oldest->timeMs) / 1000.0;
    const double error = songSeconds - expectedSeconds;
    if (std::abs(error) > kBadWindowSeconds) return std::nullopt;
    Resolve(Grade::Miss, error, true, oldest->direction);
    return last_;
}

} // namespace locus::core
