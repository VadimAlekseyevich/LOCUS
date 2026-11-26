#include "core/replay.h"
#include "core/judgement.h"
#include "core/play_mods.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace locus::core {
namespace {
constexpr char kMagic[] = "LOCREP03";
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "Replay requires binary64 timestamps");
void Put(std::string& out, std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        out.push_back(static_cast<char>((value >> (8u * i)) & 0xffu));
}
std::uint64_t Get(std::string_view bytes, std::size_t at, unsigned count) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i)
        value |= static_cast<std::uint64_t>(
            static_cast<unsigned char>(bytes[at + i])) << (8u * i);
    return value;
}
void PutDouble(std::string& out, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    Put(out, bits, 8);
}
double GetDouble(std::string_view bytes, std::size_t at) {
    const auto bits = Get(bytes, at, 8);
    double value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
bool ValidEvent(const ReplayEvent& event) noexcept {
    return static_cast<unsigned>(event.action) <=
               static_cast<unsigned>(ReplayAction::Finish) &&
           std::isfinite(event.songSeconds) &&
           (event.action != ReplayAction::Finish || event.songSeconds == 0.0);
}
} // namespace

const ReplayEvent* ReplayCursor::NextDue(const ReplayTrace& trace,
                                          double songSeconds, double finishSeconds,
                                          bool drain) noexcept {
    if (Done(trace) || !std::isfinite(songSeconds)) return nullptr;
    const auto& event = trace.events[next_];
    const double scheduled = event.action == ReplayAction::Finish
        ? finishSeconds : event.songSeconds;
    if (!drain && songSeconds < scheduled) return nullptr;
    ++next_;
    return &event;
}

bool ValidReplay(const ReplayTrace& trace) noexcept {
    if (trace.judgementVersion < 1 || trace.judgementVersion > 1000 ||
        !std::isfinite(trace.offsetSeconds) || !ValidModRate(trace.ratePercent) || !ValidModFlags(trace.modFlags) ||
        trace.events.size() > kMaxReplayEvents) return false;
    for (const auto& event : trace.events)
        if (!ValidEvent(event)) return false;
    return true;
}

bool EncodeReplay(const ReplayTrace& trace, std::string& bytes) {
    if (!ValidReplay(trace)) return false;
    std::string encoded;
    encoded.reserve(kReplayHeaderBytes + trace.events.size() * kReplayEventBytes);
    encoded.append(kMagic, 8);
    Put(encoded, static_cast<std::uint32_t>(trace.judgementVersion), 4);
    PutDouble(encoded, trace.offsetSeconds);
    Put(encoded, trace.events.size(), 4);
    Put(encoded, static_cast<unsigned>(trace.ratePercent), 4);
    Put(encoded, trace.modFlags, 4);
    for (const auto& event : trace.events) {
        Put(encoded, static_cast<unsigned>(event.action), 1);
        PutDouble(encoded, event.songSeconds);
    }
    bytes.swap(encoded);
    return true;
}

bool DecodeReplay(std::string_view bytes, ReplayTrace& trace) {
    if (bytes.size() < kReplayHeaderBytes || bytes.substr(0, 8) != kMagic)
        return false;
    const auto version = Get(bytes, 8, 4);
    const auto count = Get(bytes, 20, 4);
    if (version == 0 || version > 1000 || count > kMaxReplayEvents ||
        bytes.size() != kReplayHeaderBytes + count * kReplayEventBytes)
        return false;
    ReplayTrace decoded;
    decoded.judgementVersion = static_cast<int>(version);
    decoded.offsetSeconds = GetDouble(bytes, 12);
    const auto rate = Get(bytes,24,4);
    if (rate < 50 || rate > 150) return false;
    decoded.ratePercent = static_cast<int>(rate);
    decoded.modFlags = static_cast<ModFlags>(Get(bytes,28,4));
    if (!ValidModFlags(decoded.modFlags)) return false;
    if (!std::isfinite(decoded.offsetSeconds)) return false;
    decoded.events.reserve(static_cast<std::size_t>(count));
    for (std::size_t at = kReplayHeaderBytes; at < bytes.size(); at += kReplayEventBytes) {
        ReplayEvent event{static_cast<ReplayAction>(
            static_cast<unsigned char>(bytes[at])), GetDouble(bytes, at + 1)};
        if (!ValidEvent(event)) return false;
        decoded.events.push_back(event);
    }
    trace = std::move(decoded);
    return true;
}

std::optional<Judgement> ApplyReplayEvent(TurnJudge& judge, const ReplayEvent& event) {
    switch (event.action) {
        case ReplayAction::Advance: judge.Advance(event.songSeconds); break;
        case ReplayAction::Finish: judge.FinishRemainingMisses(); break;
        default:
            return judge.PressHeading(static_cast<Heading>(event.action), event.songSeconds);
    }
    return std::nullopt;
}

bool RejudgeReplay(const ReplayTrace& trace, TurnJudge& judge) {
    if (!ValidReplay(trace) || trace.judgementVersion != kJudgementRulesVersion)
        return false;
    judge.SetMods(trace.modFlags);
    judge.SetOffsetSeconds(trace.offsetSeconds);
    for (const auto& event : trace.events) ApplyReplayEvent(judge,event);
    return true;
}

} // namespace locus::core
