#pragma once

#include "core/route.h"

#include <cstddef>
#include <optional>

namespace locus::core {

// Provisional, symmetric timing windows for the first playable demo.
// These are gameplay defaults, not measured device latency or calibration.
constexpr double kPerfectWindowSeconds = 0.050;
constexpr double kGoodWindowSeconds = 0.110;
constexpr double kBadWindowSeconds = 0.180;

enum class Grade { Perfect, Good, Bad, Miss };

struct Judgement {
    Grade grade = Grade::Miss;
    int beat = 0;
    double timingErrorSeconds = 0.0; // Negative means early.
    bool wrongDirection = false;
    // A diagonal turn yields one judgement for each cardinal component.
    // This is the physical arrow whose result was recorded.
    Heading arrow = Heading::Up;
};

struct GradeCounts {
    int perfect = 0;
    int good = 0;
    int bad = 0;
    int miss = 0;

    int Resolved() const noexcept { return perfect + good + bad + miss; }

    // The player's requested weighted accuracy in percent. Misses count
    // in the denominator, despite contributing zero points. An unplayed
    // chart starts at 100% until the first note is judged.
    double AccuracyPercent() const noexcept {
        const int resolved = Resolved();
        return resolved == 0 ? 100.0
            : static_cast<double>(100LL * perfect + 75LL * good + 50LL * bad)
                / static_cast<double>(resolved);
    }
};

const char* GradeName(Grade grade) noexcept;
const char* DirectionName(Heading heading) noexcept;

// One judgement per *physical arrow*. An authored diagonal turn has two
// independent notes at the same timestamp, each with its own timing grade;
// their presses need not overlap or occur in any chord window. Wrong cardinal
// directions remain Miss; unrelated keys during a diagonal are ignored and
// its unpressed component eventually becomes Miss. Chart geometry never
// depends on whether either note was hit.
class TurnJudge {
public:
    explicit TurnJudge(const Chart& chart) noexcept : chart_(chart) {}

    // Offset is audio time at which chart beat zero occurs. Call between sessions.
    void SetOffsetSeconds(double offset) noexcept {
        offsetSeconds_ = offset;
        Reset();
    }

    void Reset() noexcept;
    // Skip earlier turns without recording misses in a midway practice test.
    void ResetFromBeat(int firstScoredBeat) noexcept;
    // Editor TEST excludes only notes before this exact audio timestamp.
    void ResetFromAudioSeconds(double audioSeconds) noexcept;

    // Mark every unplayed event whose late window has elapsed as Miss.
    void Advance(double songSeconds) noexcept;
    void FinishRemainingMisses() noexcept;

    // Fresh key-down only, timestamped in the same song-time domain as
    // marker animation. Returns nullopt for ignored/out-of-window presses.
    // One fresh physical arrow key-down; accepts Up/Right/Down/Left only.
    // For a diagonal each expected arrow is resolved once, in any order.
    std::optional<Judgement> PressHeading(Heading outgoing, double songSeconds);

    const GradeCounts& Counts() const noexcept { return counts_; }
    const std::optional<Judgement>& Last() const noexcept { return last_; }
    const TurnEvent* NextEvent() const noexcept;

    // Total includes only notes at/after the editor TEST start position.
    // Once every playable note has a judgement (hit or Miss), Results may
    // open immediately without waiting for the audio file to finish.
    std::size_t ScorableCount() const noexcept;
    bool AllScorableJudged() const noexcept {
        return ScorableCount() != 0 && NextEvent() == nullptr;
    }

private:
    void Resolve(Grade grade, double errorSeconds, bool wrongDirection,
                 Heading arrow) noexcept;

    const Chart& chart_;
    double offsetSeconds_ = 0.0;
    std::size_t nextEvent_ = 0;
    std::size_t firstScoredEvent_ = 0;
    // Resolved physical-arrow mask of the current authored turn. The next
    // turn is reached only after all required components are graded.
    unsigned resolvedArrows_ = 0u;
    GradeCounts counts_;
    std::optional<Judgement> last_;
};

} // namespace locus::core
