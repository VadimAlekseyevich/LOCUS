#pragma once
#include <algorithm>
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
// Menu configuration; a scored session takes an immutable snapshot.
struct PlayMods {
    RateMod rateMod = RateMod::None;
    int halfTimePercent = 75;
    int doubleTimePercent = 125;
    int RatePercent() const noexcept {
        return rateMod == RateMod::HalfTime ? std::clamp(halfTimePercent,50,99) :
               rateMod == RateMod::DoubleTime ? std::clamp(doubleTimePercent,101,150) : 100;
    }
    void Toggle(RateMod mod) noexcept { rateMod = rateMod == mod ? RateMod::None : mod; }
    void Adjust(int delta) noexcept {
        if (rateMod == RateMod::HalfTime) halfTimePercent = std::clamp(halfTimePercent + delta,50,99);
        else if (rateMod == RateMod::DoubleTime) doubleTimePercent = std::clamp(doubleTimePercent + delta,101,150);
    }
};
} // namespace locus::core
