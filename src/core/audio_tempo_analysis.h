#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <vector>

namespace locus::core {

// Inspired by spectral-flux onset detection, local autocorrelation
// tempograms and dynamic-programming tempo tracking. No external source
// code is copied. The result is deliberately a proposal, not authored data.
struct TempoProposal {
    std::int64_t timeMs=0;
    double bpm=0;
    double confidence=0; // heuristic periodicity/phase evidence, not probability
};
inline constexpr int kOnsetRate=100;

namespace audio_detail {
constexpr int kAnalysisRate=16000;
constexpr std::size_t kFftSize=512;
constexpr double kPi=3.14159265358979323846;
using Spectrum=std::array<std::complex<double>,kFftSize>;
inline void Fft(Spectrum& a) {
    for (std::size_t i=1,j=0;i<a.size();++i) {
        std::size_t bit=a.size()/2;
        for (;j&bit;bit/=2) j^=bit;
        j^=bit;
        if (i<j) std::swap(a[i],a[j]);
    }
    for (std::size_t len=2;len<=a.size();len*=2) {
        const auto step=std::polar(1.0,-2*kPi/len);
        for (std::size_t i=0;i<a.size();i+=len) {
            std::complex<double> w=1;
            for (std::size_t j=0;j<len/2;++j) {
                const auto u=a[i+j],v=a[i+j+len/2]*w;
                a[i+j]=u+v; a[i+j+len/2]=u-v;
                w*=step;
            }
        }
    }
}
} // namespace audio_detail

// Time-indexed spectral flux, centered FFT windows every 10 ms.
// Downsampling uses box averages and avoids an extra full-rate PCM copy.
template<class Sample>
std::vector<float> BuildOnsetEnvelope(std::uint32_t frames,unsigned rate,
                                       unsigned channels,Sample sample) {
    using namespace audio_detail;
    if (!frames || !rate || !channels) return {};
    const std::size_t count=static_cast<std::size_t>(
        std::uint64_t(frames)*kAnalysisRate/rate);
    if (count<kFftSize || count>96000000) return {};
    std::vector<float> mono(count);
    for (std::size_t i=0;i<count;++i) {
        const auto lo=std::uint64_t(i)*rate/kAnalysisRate;
        const auto hi=std::min<std::uint64_t>(frames,std::max(lo+1,
            std::uint64_t(i+1)*rate/kAnalysisRate));
        double value=0;
        for (auto frame=lo;frame<hi;++frame) {
            double sum=0, first=0;
            for (unsigned c=0;c<channels;++c) {
                const float v=sample(static_cast<std::uint32_t>(frame),c);
                if (!std::isfinite(v)) continue;
                const auto safe=std::clamp(v,-1.0f,1.0f);
                if (c==0) first=safe;
                sum+=safe;
            }
            const double mixed=sum/channels;
            value+=std::abs(mixed)<std::abs(first)*0.1?first:mixed;
        }
        mono[i]=static_cast<float>(value/std::max<std::uint64_t>(1,hi-lo));
    }
    constexpr int hop=kAnalysisRate/kOnsetRate;
    std::vector<float> envelope((count+hop-1)/hop);
    Spectrum spectrum{};
    std::array<double,kFftSize/2+1> previous{};
    std::array<double,kFftSize> window{};
    std::array<double,kFftSize/2+1> weights{};
    for (std::size_t k=1;k<weights.size();++k) weights[k]=1.0/std::sqrt(static_cast<double>(k));
    for (std::size_t i=0;i<window.size();++i)
        window[i]=0.5-0.5*std::cos(2*kPi*i/(window.size()-1));
    for (std::size_t frame=0;frame<envelope.size();++frame) {
        const auto center=static_cast<std::int64_t>(frame*hop);
        for (std::size_t j=0;j<kFftSize;++j) {
            const auto i=center+static_cast<std::int64_t>(j)-static_cast<std::int64_t>(kFftSize/2);
            spectrum[j]=i>=0 && static_cast<std::size_t>(i)<mono.size()
                ? mono[static_cast<std::size_t>(i)]*window[j] : 0.0;
        }
        Fft(spectrum);
        double flux=0;
        for (std::size_t k=1;k<previous.size();++k) {
            const double value=std::log1p(std::abs(spectrum[k]));
            // Reduce the domination of high-frequency bins and retain
            // bass/percussion evidence without depending on one band.
            flux+=weights[k]*std::max(0.0,value-previous[k]);
            previous[k]=value;
        }
        envelope[frame]=static_cast<float>(flux);
    }
    return envelope;
}

namespace audio_detail {
struct Onset { double frame=0,weight=0; };
struct TempoWindow { int center=0,lag=0; double evidence=0; };
struct BeatFit { double period=0,phase=0,confidence=0; };

inline std::vector<Onset> PickOnsets(const std::vector<float>& flux) {
    std::vector<Onset> peaks;
    if (flux.size()<3) return peaks;
    std::vector<double> sum(flux.size()+1);
    for (std::size_t i=0;i<flux.size();++i)
        sum[i+1]=sum[i]+std::max(0.0f,flux[i]);
    const double largest=*std::max_element(flux.begin(),flux.end());
    if (!(largest>1e-7)) return peaks;
    for (std::size_t i=1;i+1<flux.size();++i) {
        const auto lo=i>20?i-20:0;
        const auto hi=std::min(flux.size(),i+21);
        const double mean=(sum[hi]-sum[lo])/(hi-lo);
        const double v=flux[i];
        if (v<largest*0.012 || v<mean*1.45 ||
            v<flux[i-1] || v<=flux[i+1]) continue;
        const double denom=flux[i-1]-2*v+flux[i+1];
        const double sub=std::abs(denom)>1e-12
            ? std::clamp(0.5*(flux[i-1]-flux[i+1])/denom,-0.5,0.5):0.0;
        Onset peak{static_cast<double>(i)+sub,std::sqrt(v)};
        if (!peaks.empty() && peak.frame-peaks.back().frame<6) {
            if (peak.weight>peaks.back().weight) peaks.back()=peak;
        } else peaks.push_back(peak);
    }
    return peaks;
}

// Fit beat phase and fractional period to measured onset times, not the
// integer autocorrelation lag. One onset per inferred beat, weighted LS.
inline BeatFit FitBeats(const std::vector<Onset>& peaks,double from,
                        double to,double period) {
    if (!(to>from) || !(period>0)) return {};
    const auto first=std::lower_bound(peaks.begin(),peaks.end(),from,
        [](const Onset& p,double t){return p.frame<t;});
    const auto last=std::lower_bound(first,peaks.end(),to,
        [](const Onset& p,double t){return p.frame<t;});
    if (last-first<4) return {};
    // Phase votes with a tolerance to expressive timing and spectral latency.
    constexpr int phases=64;
    std::array<double,phases> votes{};
    const double origin=from;
    for (auto it=first;it!=last;++it) {
        const double phase=std::fmod((it->frame-origin)/period,1.0);
        const int b=static_cast<int>(phase*phases)%phases;
        for (int k=-5;k<=5;++k)
            votes[(b+k+phases)%phases]+=it->weight*(1.0-std::abs(k)/6.0);
    }
    const auto best=std::max_element(votes.begin(),votes.end())-votes.begin();
    double phase=origin+(best+0.5)*period/phases;
    struct Match { double beat,time,weight; };
    std::vector<Match> matches;
    const auto collect=[&] {
        matches.clear();
        for (auto it=first;it!=last;++it) {
            const double index=std::round((it->frame-phase)/period);
            const double residual=std::abs(it->frame-(phase+index*period));
            if (residual>std::min(9.0,period*0.16)) continue;
            const double weight=it->weight/(1+residual*residual*0.1);
            if (!matches.empty() && matches.back().beat==index) {
                if (weight>matches.back().weight)
                    matches.back()={index,it->frame,weight};
            } else matches.push_back({index,it->frame,weight});
        }
    };
    const double seed=period;
    for (int pass=0;pass<3;++pass) {
        collect();
        if (matches.size()<4) return {};
        double w=0,x=0,y=0,xx=0,xy=0;
        for (const auto& m:matches) {
            w+=m.weight; x+=m.weight*m.beat; y+=m.weight*m.time;
            xx+=m.weight*m.beat*m.beat; xy+=m.weight*m.beat*m.time;
        }
        const double denom=w*xx-x*x;
        if (!(denom>1e-9)) return {};
        const double fitted=(w*xy-x*y)/denom;
        if (std::abs(fitted-seed)>seed*0.06) return {};
        period=fitted; phase=(y-period*x)/w;
    }
    collect();
    if (matches.size()<4) return {};
    double error=0;
    for (const auto& m:matches)
        error+=std::abs(m.time-(phase+m.beat*period));
    error/=matches.size();
    const double coverage=std::clamp(matches.size()*period/(to-from),0.0,1.0);
    const double confidence=coverage*std::exp(-error/std::max(1.0,period*0.10));
    return {period,phase,confidence};
}
} // namespace audio_detail

// Local tempograms: 8-second window / 2-second hop, 40–240 BPM.
// Dynamic programming penalizes octave jumps and unstable tempo paths.
// Contiguous reliable windows become separate candidate timing segments.
inline std::vector<TempoProposal> SuggestTempoMap(const std::vector<float>& flux) {
    using namespace audio_detail;
    constexpr int minLag=25,maxLag=150,states=maxLag-minLag+1;
    constexpr int halfWindow=400,hop=200;
    if (flux.size()<400 || flux.size()>600000) return {};
    const int n=static_cast<int>(flux.size());
    const auto peaks=PickOnsets(flux);
    if (peaks.size()<4) return {};
    std::vector<double> envelope(n);
    for (const auto& p:peaks) {
        const int i=static_cast<int>(std::lround(p.frame));
        for (int d=-3;d<=3;++d)
            if (i+d>=0 && i+d<n)
                envelope[i+d]+=p.weight*std::exp(-0.5*d*d/2.25);
    }
    std::vector<TempoWindow> windows;
    std::vector<std::array<short,states>> back;
    std::vector<std::array<double,states>> evidence;
    std::array<double,states> previous{},current{};
    std::array<std::array<double,states>,states> transition{};
    for (int k=0;k<states;++k) for (int j=0;j<states;++j) {
        const double delta=std::abs(std::log2(static_cast<double>(k+minLag)/(j+minLag)));
        transition[k][j]=0.17*delta+(delta>0.06?0.10:0.0);
    }
    for (int center=0;center<n;center+=hop) {
        const int from=std::max(0,center-halfWindow);
        const int to=std::min(n,center+halfWindow);
        std::array<double,2*maxLag+1> corr{};
        for (int lag=minLag/2;lag<=2*maxLag;++lag) {
            double dot=0,a=0,b=0;
            for (int i=from;i+lag<to;++i) {
                const auto x=envelope[i],y=envelope[i+lag];
                dot+=x*y; a+=x*x; b+=y*y;
            }
            if (a*b>1e-12) corr[lag]=dot/std::sqrt(a*b);
        }
        std::array<double,states> row{};
        std::array<short,states> links{};
        for (int k=0;k<states;++k) {
            const int lag=k+minLag;
            const double bpm=6000.0/lag;
            const double prior=0.025*std::abs(std::log2(bpm/120.0));
            row[k]=std::max(0.0,corr[lag]);
            const double emission=corr[lag]+0.20*corr[lag*2]-
                0.35*corr[lag/2]-prior;
            double best=-std::numeric_limits<double>::infinity();
            short at=static_cast<short>(k);
            for (int j=0;j<states;++j) {
                const double score=previous[j]-transition[k][j];
                if (score>best) {best=score;at=static_cast<short>(j);}
            }
            current[k]=best+emission; links[k]=at;
        }
        // Subtract common offset to keep long tracks numerically stable.
        const double best=*std::max_element(current.begin(),current.end());
        for (int k=0;k<states;++k) previous[k]=current[k]-best;
        windows.push_back({center,0,0});
        back.push_back(links); evidence.push_back(row);
    }
    int state=static_cast<int>(std::max_element(previous.begin(),previous.end())-
                              previous.begin());
    for (std::size_t i=windows.size();i-->0;) {
        windows[i].lag=state+minLag;
        windows[i].evidence=evidence[i][state];
        state=back[i][state];
    }
    struct Segment { int from,to; double lag,evidence; int count; };
    std::vector<Segment> segments;
    for (const auto& w:windows) {
        if (w.evidence<0.27) continue;
        if (segments.empty() || w.center-segments.back().to>hop*2 ||
            std::abs(w.lag-segments.back().lag)>segments.back().lag*0.055) {
            segments.push_back({w.center,w.center,static_cast<double>(w.lag),w.evidence,1});
        } else {
            auto& s=segments.back();
            s.lag=(s.lag*s.count+w.lag)/(s.count+1);
            s.evidence=(s.evidence*s.count+w.evidence)/(s.count+1);
            s.to=w.center; ++s.count;
        }
    }
    std::vector<TempoProposal> result;
    BeatFit previousFit{};
    for (std::size_t i=0;i<segments.size() && result.size()<256;++i) {
        const auto& s=segments[i];
        if (s.count<2) continue;
        const double from=i==0?std::max(0,s.from-halfWindow):(segments[i-1].to+s.from)*0.5;
        const double to=i+1==segments.size()?n:(s.to+segments[i+1].from)*0.5;
        // Fit the reliable interior; boundary uncertainty is shown to mapper.
        const double margin=std::min(100.0,(to-from)*0.15);
        auto fit=FitBeats(peaks,from+margin,to-margin,s.lag);
        if (!(fit.period>0)) fit=FitBeats(peaks,from,to,s.lag);
        if (!(fit.period>0) || fit.confidence<0.22) continue;
        const double confidence=std::clamp(s.evidence*fit.confidence,0.0,1.0);
        if (confidence<0.15) continue;
        double marker=fit.phase+std::ceil((from-fit.phase)/fit.period-1e-8)*fit.period;
        // Locate the earliest measured beat close to the estimated anchor.
        // For later sections search backwards up to a beat so a BPM change
        // at a boundary is not systematically delayed by one quarter note.
        if (result.empty()) {
            // Beat zero must be an observed onset, not an extrapolated
            // phase inside the silent lead-in of a recording.
            bool found=false;
            for (auto it=std::lower_bound(peaks.begin(),peaks.end(),from,
                     [](const Onset& p,double t){return p.frame<t;});
                 it!=peaks.end() && it->frame<to;++it) {
                const double index=std::round((it->frame-fit.phase)/fit.period);
                if (std::abs(it->frame-fit.phase-index*fit.period)<=
                        std::min(9.0,fit.period*0.15)) {
                    marker=it->frame; found=true; break;
                }
            }
            if (!found) continue;
        } else {
            const double prior=marker-fit.period;
            if (std::abs(prior-from)<std::abs(marker-from)) marker=prior;
            if (previousFit.period>0) {
                const auto lo=std::lower_bound(peaks.begin(),peaks.end(),from-300,
                    [](const Onset& p,double t){return p.frame<t;});
                const auto hi=std::lower_bound(lo,peaks.end(),from+300,
                    [](const Onset& p,double t){return p.frame<t;});
                const auto cost=[](const Onset& p,const BeatFit& grid) {
                    const double index=std::round((p.frame-grid.phase)/grid.period);
                    const double residual=std::abs(p.frame-grid.phase-index*grid.period);
                    const double tolerance=std::max(2.0,grid.period*0.12);
                    return p.weight*std::min(1.0,residual*residual/(tolerance*tolerance));
                };
                double bestCost=std::numeric_limits<double>::infinity();
                for (auto it=lo;it!=hi;++it) {
                    const double at=it->frame;
                    const double gridIndex=std::round((at-fit.phase)/fit.period);
                    if (std::abs(at-fit.phase-gridIndex*fit.period)>fit.period*0.12 ||
                        at*10<=result.back().timeMs) continue;
                    double error=0,weight=0;
                    for (auto p=lo;p!=hi;++p) {
                        error+=cost(*p,p->frame<at?previousFit:fit);
                        weight+=p->weight;
                    }
                    if (!(weight>0)) continue;
                    error=error/weight+0.02*std::abs(at-from)/300.0;
                    if (error<bestCost) {bestCost=error;marker=at;}
                }
            }
        }
        double best=std::min(9.0,fit.period*0.15);
        double exact=marker;
        for (auto it=std::lower_bound(peaks.begin(),peaks.end(),marker-best,
                 [](const Onset& p,double t){return p.frame<t;});
             it!=peaks.end() && it->frame<=marker+best;++it) {
            const double d=std::abs(it->frame-marker);
            if (d<best) {best=d;exact=it->frame;}
        }
        if (exact<0 || exact>=n) continue;
        const auto ms=static_cast<std::int64_t>(std::llround(exact*10));
        const double bpm=std::round(6000.0/fit.period*1000)/1000;
        if (!result.empty() && ms<=result.back().timeMs) continue;
        if (!result.empty() && std::abs(bpm-result.back().bpm)<result.back().bpm*0.015) {
            // Keep a phase reset after a genuine break; otherwise no need
            // for duplicate BPM markers from a noisy local window.
            const double beat=(ms-result.back().timeMs)*result.back().bpm/60000.0;
            if (std::abs(beat-std::round(beat))<0.10) continue;
        }
        result.push_back({ms,bpm,confidence});
        previousFit=fit;
    }
    return result;
}
} // namespace locus::core
