#pragma once
#include <algorithm>
#include <array>
#include "core/route.h"
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
enum class PlayMod : std::uint32_t { Easy=1, HardRock=2, Hidden=4, Flashlight=8, Mirror=16, Random=32, Autoplay=64, SuddenDeath=128, Perfect=256, Cinema=512 };
using ModFlags = std::uint32_t;
constexpr ModFlags ModBit(PlayMod mod) noexcept { return static_cast<ModFlags>(mod); }
constexpr bool HasMod(ModFlags flags, PlayMod mod) noexcept { return (flags & ModBit(mod)) != 0; }
struct ModDefinition { PlayMod mod; const char* id; const char* code; const char* name; ModFlags conflicts; };
inline constexpr std::array<ModDefinition,10> kPlayModDefinitions{{
    {PlayMod::Easy,"ez","EZ","Easy",2u}, {PlayMod::HardRock,"hr","HR","Hard Rock",1u},
    {PlayMod::Hidden,"hd","HD","Hidden",8u}, {PlayMod::Flashlight,"fl","FL","Flashlight",4u},
    {PlayMod::Mirror,"mr","MR","Mirror",32u}, {PlayMod::Random,"rd","RD","Random",16u},
    {PlayMod::Autoplay,"at","AT","Autoplay",128u|256u|512u},
    {PlayMod::SuddenDeath,"sd","SD","Sudden Death",64u|256u|512u},
    {PlayMod::Perfect,"pf","PF","Perfect",64u|128u|512u},
    {PlayMod::Cinema,"cn","CN","Cinema",64u|128u|256u}
}};
constexpr bool ValidModFlags(ModFlags flags) noexcept {
    if (flags & ~1023u) return false;
    for (const auto& mod : kPlayModDefinitions)
        if (HasMod(flags,mod.mod) && (flags & mod.conflicts)) return false;
    return true;
}
constexpr bool Assisted(ModFlags flags) noexcept {
    return HasMod(flags,PlayMod::Autoplay) || HasMod(flags,PlayMod::Cinema);
}
// Fixed integer PRNG: replay output does not depend on standard-library RNGs.
inline Chart TransformModChart(const Chart& source, ModFlags flags, std::uint32_t seed) {
    Chart chart=source;
    const auto transform=[&](Heading heading) {
        if (HasMod(flags,PlayMod::Mirror)) return MirrorHeadingHorizontal(heading);
        if (HasMod(flags,PlayMod::Random)) {
            seed=seed*1664525u+1013904223u;
            return RotateHeading(heading,static_cast<int>((seed>>30)*2));
        }
        return heading;
    };
    chart.initialHeading=transform(chart.initialHeading);
    for (auto& turn : chart.turns) turn.direction=transform(turn.direction);
    return chart;
}
constexpr double WindowScale(ModFlags flags) noexcept {
    return HasMod(flags,PlayMod::Easy) ? 1.5 : HasMod(flags,PlayMod::HardRock) ? 0.7 : 1.0;
}
inline std::string FormatMods(int rate, ModFlags flags) {
    std::string out = rate == 100 ? "" : FormatRateMod(rate);
    const auto add = [&](PlayMod mod,const char* code) {
        if (HasMod(flags,mod)) { if (!out.empty()) out += " "; out += code; }
    };
    for (const auto& mod : kPlayModDefinitions) add(mod.mod,mod.code);
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
        for (const auto& item : kPlayModDefinitions)
            if (item.mod == mod) { flags &= ~item.conflicts; flags |= bit; break; }
    }
    void Adjust(int delta) noexcept {
        if (rateMod == RateMod::HalfTime) halfTimePercent = std::clamp(halfTimePercent + delta,50,99);
        else if (rateMod == RateMod::DoubleTime) doubleTimePercent = std::clamp(doubleTimePercent + delta,101,150);
    }
};
} // namespace locus::core
