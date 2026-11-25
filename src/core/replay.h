#pragma once

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
inline constexpr std::size_t kReplayHeaderBytes = 24;
inline constexpr std::size_t kReplayEventBytes = 9;
inline constexpr std::size_t kMaxReplayBytes =
    kReplayHeaderBytes + kMaxReplayEvents * kReplayEventBytes;

struct ReplayTrace {
    int judgementVersion = 0;
    double offsetSeconds = 0.0;
    std::vector<ReplayEvent> events;
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
