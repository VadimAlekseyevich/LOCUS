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

// One judgement per authored event. Wrong directions inside a window are
// Miss; stray presses outside all windows are ignored. Keyboard input never
// changes the const Chart or the deterministic route derived from it.
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
    // Absolute direction input. Diagonals require the corresponding two
    // arrow keys, but still score as one authored turn event.
    std::optional<Judgement> PressHeading(Heading outgoing, double songSeconds);

    const GradeCounts& Counts() const noexcept { return counts_; }
    const std::optional<Judgement>& Last() const noexcept { return last_; }
    const TurnEvent* NextEvent() const noexcept;

    // Total includes only notes at/after the editor TEST start position.
    // Once every playable note has a judgement (hit or Miss), Results may
    // open immediately without waiting for the audio file to finish.
    std::size_t ScorableCount() const noexcept {
        return chart_.turns.size() - firstScoredEvent_;
    }
    bool AllScorableJudged() const noexcept {
        return ScorableCount() != 0 && NextEvent() == nullptr;
    }

private:
    void Resolve(Grade grade, double errorSeconds, bool wrongDirection) noexcept;

    const Chart& chart_;
    double offsetSeconds_ = 0.0;
    std::size_t nextEvent_ = 0;
    std::size_t firstScoredEvent_ = 0;
    GradeCounts counts_;
    std::optional<Judgement> last_;
};

} // namespace locus::core
