#pragma once

#include "core/tempo_map.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace locus::core {
enum class MetronomeClick { None, Beat, Downbeat };

// A marker starts a local bar. Work in original song seconds, not ticks:
// fractional gaps between adjacent tempo anchors have zero song duration.
class MetronomeCursor {
public:
    void Reset() noexcept { previous_=std::numeric_limits<double>::quiet_NaN(); }
    MetronomeClick Update(const Chart& chart,double offset,double seconds,
                          double rate) noexcept {
        if (!std::isfinite(seconds) || !(rate>0) || !std::isfinite(rate)) {
            Reset(); return MetronomeClick::None;
        }
        const auto& map=chart.tempoMap;
        double anchor=offset,bpm=chart.bpm;
        if (!map.empty()) {
            const auto next=std::upper_bound(map.begin(),map.end(),seconds*1000.0,
                [](double ms,const TempoMarker& marker) {return ms<marker.timeMs;});
            const auto& active=next==map.begin()?map.front():*(next-1);
            anchor=active.timeMs/1000.0; bpm=active.bpm;
        }
        if (!(bpm>0) || !std::isfinite(bpm)) { Reset(); return MetronomeClick::None; }
        const double quarter=60.0/bpm;
        const double localBeat=std::floor((seconds-anchor)/quarter+1e-8);
        const double at=anchor+localBeat*quarter;
        const double previous=previous_;
        previous_=seconds;
        // Avoid a delayed burst after seek, pause or a long stalled frame.
        // Allow a fresh start exactly on a beat, but not an arbitrary seek.
        const bool fresh=!std::isfinite(previous) || seconds<previous;
        const double age=(seconds-at)/rate;
        if (fresh ? age>0.025 : at<=previous+1e-8) return MetronomeClick::None;
        if (age>0.075 || age < -1e-6) return MetronomeClick::None;
        return std::abs(std::fmod(localBeat,4.0))<0.5
            ? MetronomeClick::Downbeat : MetronomeClick::Beat;
    }
private:
    double previous_=std::numeric_limits<double>::quiet_NaN();
};
} // namespace locus::core
