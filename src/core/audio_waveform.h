#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace locus::core {

struct AudioPeak {
    float minimum=0,maximum=0;
};
inline AudioPeak MergeAudioPeaks(AudioPeak a,AudioPeak b) noexcept {
    return {std::min(a.minimum,b.minimum),std::max(a.maximum,b.maximum)};
}

// Peak envelope at 200 Hz with power-of-two reduction levels. A view
// queries exact base-bin coverage in O(log n), independently of track length.
class AudioWaveform {
public:
    static constexpr int kBinsPerSecond=200;
    template<class Sample>
    static AudioWaveform Build(std::uint32_t frames,unsigned rate,
                               unsigned channels,Sample sample) {
        AudioWaveform out;
        if (!frames || !rate || !channels) return out;
        out.duration_=static_cast<double>(frames)/rate;
        const auto count=static_cast<std::size_t>(
            (std::uint64_t(frames)*kBinsPerSecond+rate-1)/rate);
        if (count>10000000) return {};
        std::vector<AudioPeak> bins(count);
        for (std::uint32_t i=0;i<frames;++i) {
            auto& bin=bins[static_cast<std::size_t>(
                std::uint64_t(i)*kBinsPerSecond/rate)];
            for (unsigned c=0;c<channels;++c) {
                const float v=sample(i,c);
                if (!std::isfinite(v)) continue;
                bin.minimum=std::min(bin.minimum,std::clamp(v,-1.0f,1.0f));
                bin.maximum=std::max(bin.maximum,std::clamp(v,-1.0f,1.0f));
            }
        }
        out.levels_.push_back(std::move(bins));
        while (out.levels_.back().size()>1) {
            const auto& previous=out.levels_.back();
            std::vector<AudioPeak> next((previous.size()+1)/2);
            for (std::size_t i=0;i<next.size();++i) {
                next[i]=previous[i*2];
                if (i*2+1<previous.size())
                    next[i]=MergeAudioPeaks(next[i],previous[i*2+1]);
            }
            out.levels_.push_back(std::move(next));
        }
        return out;
    }
    bool Empty() const noexcept { return levels_.empty(); }
    double Duration() const noexcept { return duration_; }
    AudioPeak Range(double from,double to) const noexcept {
        if (Empty() || !std::isfinite(from) || !std::isfinite(to) ||
            to<=from || to<=0 || from>=duration_) return {};
        const auto count=levels_[0].size();
        auto lo=static_cast<std::size_t>(std::floor(std::max(0.0,from)*kBinsPerSecond));
        const auto hi=std::min(count,static_cast<std::size_t>(
            std::ceil(std::min(duration_,to)*kBinsPerSecond)));
        AudioPeak peak;
        while (lo<hi) {
            std::size_t level=0,span=1;
            while (level+1<levels_.size() && span<=(hi-lo)/2 &&
                   lo%(span*2)==0) { span*=2; ++level; }
            peak=MergeAudioPeaks(peak,levels_[level][lo/span]);
            lo+=span;
        }
        return peak;
    }
private:
    double duration_=0;
    std::vector<std::vector<AudioPeak>> levels_;
};

} // namespace locus::core
