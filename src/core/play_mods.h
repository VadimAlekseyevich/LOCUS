#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
namespace locus::core {
enum class RateMod { None, HalfTime, DoubleTime };
constexpr bool ValidModRate(int percent) noexcept { return percent >= 50 && percent <= 150; }
constexpr RateMod RateModFor(int percent) noexcept {
    return percent < 100 ? RateMod::HalfTime : percent > 100 ? RateMod::DoubleTime : RateMod::None;
}
constexpr const char* RateModCode(RateMod mod) noexcept {
    return mod == RateMod::HalfTime ? "HT" : mod == RateMod::DoubleTime ? "DT" : "NM";
}
inline std::string FormatRateMod(int percent) {
    return std::string(RateModCode(RateModFor(percent))) + " " + std::to_string(percent) + "%";
}
// Stable persisted modifier bits. Never reuse a bit for a different rule.
enum class PlayMod : std::uint32_t { Easy=1, HardRock=2, Hidden=4, Flashlight=8 };
using ModFlags = std::uint32_t;
constexpr ModFlags ModBit(PlayMod mod) noexcept { return static_cast<ModFlags>(mod); }
constexpr bool HasMod(ModFlags flags, PlayMod mod) noexcept { return (flags & ModBit(mod)) != 0; }
constexpr bool ValidModFlags(ModFlags flags) noexcept {
    return (flags & ~15u)==0 && (flags & 3u)!=3u && (flags & 12u)!=12u;
}
constexpr double WindowScale(ModFlags flags) noexcept {
    return HasMod(flags,PlayMod::Easy) ? 1.5 : HasMod(flags,PlayMod::HardRock) ? 0.7 : 1.0;
}
inline std::string FormatMods(int rate, ModFlags flags) {
    std::string out = rate == 100 ? "" : FormatRateMod(rate);
    const auto add = [&](PlayMod mod,const char* code) {
        if (HasMod(flags,mod)) { if (!out.empty()) out += " "; out += code; }
    };
    add(PlayMod::Easy,"EZ"); add(PlayMod::HardRock,"HR");
    add(PlayMod::Hidden,"HD"); add(PlayMod::Flashlight,"FL");
    return out.empty() ? "NM" : out;
}
// Menu configuration; a scored session takes an immutable snapshot.
struct PlayMods {
    RateMod rateMod = RateMod::None;
    ModFlags flags = 0;
    int halfTimePercent = 75;
    int doubleTimePercent = 125;
    int RatePercent() const noexcept {
        return rateMod == RateMod::HalfTime ? std::clamp(halfTimePercent,50,99) :
               rateMod == RateMod::DoubleTime ? std::clamp(doubleTimePercent,101,150) : 100;
    }
    void Toggle(RateMod mod) noexcept { rateMod = rateMod == mod ? RateMod::None : mod; }
    void Toggle(PlayMod mod) noexcept {
        const auto bit = ModBit(mod);
        if (flags & bit) { flags &= ~bit; return; }
        flags &= ~(bit & 3u ? 3u : 12u);
        flags |= bit;
    }
    void Adjust(int delta) noexcept {
        if (rateMod == RateMod::HalfTime) halfTimePercent = std::clamp(halfTimePercent + delta,50,99);
        else if (rateMod == RateMod::DoubleTime) doubleTimePercent = std::clamp(doubleTimePercent + delta,101,150);
    }
};
} // namespace locus::core
