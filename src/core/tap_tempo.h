#pragma once
#include "core/route.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <cstdint>

namespace locus::core {

// Taps are in original song time, including at reduced preview rates.
// Estimate a fractional period and phase from a bounded beat sequence.
class TapTempo {
public:
    void Reset() noexcept { count_=0; }
    int Count() const noexcept { return count_; }
    bool Tap(double seconds) noexcept {
        if (!std::isfinite(seconds) || seconds<0 || seconds>86400) return false;
        if (count_) {
            const double interval=seconds-taps_[count_-1];
            if (interval<0 || interval>2.5) Reset();
            else if (interval<0.12) return false;
        }
        if (count_==static_cast<int>(taps_.size())) {
            std::move(taps_.begin()+1,taps_.end(),taps_.begin());
            --count_;
        }
        taps_[count_++]=seconds;
        return true;
    }
    std::optional<TempoMarker> Estimate() const noexcept {
        if (count_<4) return std::nullopt;
        std::array<double,15> gaps{};
        for (int i=1;i<count_;++i) gaps[i-1]=taps_[i]-taps_[i-1];
        std::sort(gaps.begin(),gaps.begin()+count_-1);
        const double period=gaps[(count_-1)/2];
        if (!(period>0)) return std::nullopt;
        std::array<double,16> beat{};
        int stable=1;
        for (int i=1;i<count_;++i) {
            const double gap=taps_[i]-taps_[i-1];
            const double steps=std::clamp(std::round(gap/period),1.0,4.0);
            beat[i]=beat[i-1]+steps;
            if (std::abs(gap-steps*period)<=period*0.20) ++stable;
        }
        if (stable*4<count_*3) return std::nullopt;
        double x=0,y=0,xx=0,xy=0;
        for (int i=0;i<count_;++i) {
            const double t=taps_[i]-taps_[0];
            x+=beat[i]; y+=t; xx+=beat[i]*beat[i]; xy+=beat[i]*t;
        }
        const double denom=count_*xx-x*x;
        if (!(denom>0)) return std::nullopt;
        const double p=(count_*xy-x*y)/denom;
        if (!(p>0) || std::abs(p-period)>period*0.12) return std::nullopt;
        const double origin=taps_[0]+(y-p*x)/count_;
        double error=0;
        for (int i=0;i<count_;++i) error+=std::abs(taps_[i]-origin-beat[i]*p);
        if (error/count_>p*0.12 || origin<0) return std::nullopt;
        return TempoMarker{static_cast<std::int64_t>(std::llround(origin*1000)),
                           std::round(60000.0/p)/1000.0};
    }
private:
    std::array<double,16> taps_{};
    int count_=0;
};
} // namespace locus::core
