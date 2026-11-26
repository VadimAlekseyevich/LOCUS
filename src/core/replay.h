#pragma once

#include "core/play_mods.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace locus::core {

class TurnJudge;

// A replay captures the actual order of scoring operations, in calibrated
// song seconds. It is not a video, raw keyboard log, or transport recording.
// Codes 0..3 are cardinal key-downs in Heading order.
enum class ReplayAction : std::uint8_t {
    Up, Right, Down, Left, Advance, Finish
};
struct ReplayEvent {
    ReplayAction action = ReplayAction::Advance;
    double songSeconds = 0.0;
};

inline constexpr std::size_t kMaxReplayEvents = 2000000;
inline constexpr std::size_t kReplayHeaderBytes = 36;
inline constexpr std::size_t kReplayEventBytes = 9;
inline constexpr std::size_t kMaxReplayBytes =
    kReplayHeaderBytes + kMaxReplayEvents * kReplayEventBytes;

struct ReplayTrace {
    int judgementVersion = 0;
    double offsetSeconds = 0.0;
    std::vector<ReplayEvent> events;
    int ratePercent = 100; // Immutable HT/DT configuration.
    ModFlags modFlags = 0;
    std::uint32_t modSeed = 0;
};

// Frame-independent sequential delivery. Earlier timestamps arriving after
// later ones are dispatched immediately after their predecessor, never sorted.
// A Finish operation has no original dispatch timestamp, so the viewer supplies
// a presentation deadline (normally the final note's late window).
class ReplayCursor {
public:
    void Reset() noexcept { next_ = 0; }
    const ReplayEvent* NextDue(const ReplayTrace& trace, double songSeconds,
                               double finishSeconds, bool drain = false) noexcept;
    bool Done(const ReplayTrace& trace) const noexcept { return next_ >= trace.events.size(); }
private:
    std::size_t next_ = 0;
};

bool ValidReplay(const ReplayTrace& trace) noexcept;
// Portable little-endian IEEE-754 binary codec; never round timestamps.
bool EncodeReplay(const ReplayTrace& trace, std::string& bytes);
bool DecodeReplay(std::string_view bytes, ReplayTrace& trace);
// Recalculate with the same judge implementation, not a second scoring
// algorithm. The caller must supply the matching authored chart/revision.
// Invalid/incompatible traces do not mutate the judge.
bool RejudgeReplay(const ReplayTrace& trace, TurnJudge& judge);

} // namespace locus::core
