#include "core/chart_difficulty.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <functional>
#include <vector>

namespace locus::core {
namespace {

constexpr double kInvSqrt2 = 0.707106781186547524400844362104849039;
constexpr double kEpsilon = 1e-9;

struct Point { double x = 0.0, y = 0.0; };
Point Unit(Heading heading) noexcept {
    switch (heading) {
        case Heading::Up: return {0.0, -1.0};
        case Heading::Right: return {1.0, 0.0};
        case Heading::Down: return {0.0, 1.0};
        case Heading::Left: return {-1.0, 0.0};
        case Heading::UpRight: return {kInvSqrt2, -kInvSqrt2};
        case Heading::DownRight: return {kInvSqrt2, kInvSqrt2};
        case Heading::DownLeft: return {-kInvSqrt2, kInvSqrt2};
        case Heading::UpLeft: return {-kInvSqrt2, -kInvSqrt2};
    }
    return {};
}
struct Note {
    double timeMs = 0.0; // Real elapsed milliseconds at the selected rate.
    Point pos;
    Heading heading = Heading::Right;
    unsigned mask = 0;
    int keys = 1;
    double reading = 0.0;
};
double Nonnegative(double value) {
    return std::isfinite(value) ? std::max(0.0, value) : 0.0;
}
int HeadingIndex(Heading h) {
    switch (h) {
        case Heading::Up: return 0;
        case Heading::UpRight: return 1;
        case Heading::Right: return 2;
        case Heading::DownRight: return 3;
        case Heading::Down: return 4;
        case Heading::DownLeft: return 5;
        case Heading::Left: return 6;
        case Heading::UpLeft: return 7;
    }
    return 0;
}
int Bits(unsigned m) {
    int bits = 0;
    for (; m; m &= m - 1) ++bits;
    return bits;
}
Point Sub(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
double Dot(Point a, Point b) { return a.x*b.x + a.y*b.y; }
double Cross(Point a, Point b) { return a.x*b.y - a.y*b.x; }
double Dist(Point a, Point b) { return std::sqrt(Dot(Sub(a,b), Sub(a,b))); }
double PointSegmentDistance(Point p, Point a, Point b) {
    const Point line = Sub(b, a);
    const double lengthSquared = Dot(line, line);
    if (lengthSquared < kEpsilon) return Dist(p, a);
    const double t = std::clamp(Dot(Sub(p, a), line) / lengthSquared, 0.0, 1.0);
    return Dist(p, {a.x + line.x*t, a.y + line.y*t});
}
bool Intersects(Point a, Point b, Point c, Point d) {
    const Point ab = Sub(b,a), cd = Sub(d,c), ac = Sub(c,a);
    const double determinant = Cross(ab,cd);
    if (std::abs(determinant) < kEpsilon) return false;
    const double t = Cross(ac,cd) / determinant;
    const double u = Cross(ac,ab) / determinant;
    // Endpoints count as overlaps, but immediate neighbor segments are skipped.
    return t >= -kEpsilon && t <= 1.0 + kEpsilon &&
           u >= -kEpsilon && u <= 1.0 + kEpsilon;
}
double SegmentDistance(Point a, Point b, Point c, Point d) {
    if (Intersects(a,b,c,d)) return 0.0;
    return std::min({
        PointSegmentDistance(a,c,d), PointSegmentDistance(b,c,d),
        PointSegmentDistance(c,a,b), PointSegmentDistance(d,a,b)});
}
double Percentile(std::vector<double> values, double fraction) {
    if (values.empty()) return 0.0;
    const std::size_t index = static_cast<std::size_t>(std::floor(
        std::clamp(fraction, 0.0, 1.0) * (values.size() - 1)));
    std::nth_element(values.begin(), values.begin() + index, values.end());
    return values[index];
}

// These are chart-only heuristics, not physiological or player-specific
// parameters. Times are elapsed real seconds at the selected rate.
constexpr double kSpeedTau = 0.40;
constexpr double kFingerTau = 0.48;
constexpr double kCoordinationTau = 0.42;
constexpr double kRhythmTau = 0.45;
constexpr double kStaminaTau = 6.0;
constexpr std::int64_t kSectionMs = 400;
constexpr double kPeakWeight = 0.90;
enum Skill : std::size_t { Speed, Finger, Coordination, Rhythm, Stamina, SkillCount };
using Skills = std::array<double, SkillCount>;
constexpr double kSkillWeights[]={0.35,0.30,0.20,0.10,0.05};
double PhysicalNorm(const Skills& skills) {
    double sum=0.0;
    for (std::size_t i=0;i<SkillCount;++i) sum+=kSkillWeights[i]*skills[i]*skills[i];
    return std::sqrt(sum);
}

double Decay(double dt, double tau) {
    return std::exp(-dt / tau);
}

// A fixed normalization (rather than division by the number of occupied
// sections) ensures padding a chart with easy notes cannot dilute a spike.
// Only sections containing actual input contribute: silence is recovery.
class SectionPeaks {
public:
    SectionPeaks(DifficultyTimeline* timeline,double duration,double firstMs)
        : timeline_(timeline), durationMs_(duration*1000.0), firstMs_(firstMs) {}
    void Add(double relativeMs, const Skills& sample) {
        const auto section = static_cast<std::int64_t>(std::floor(relativeMs / kSectionMs));
        if (section != current_) {
            if (current_ >= 0) Flush();
            current_ = section;
            peak_.fill(0.0);
        }
        for (std::size_t i = 0; i < SkillCount; ++i)
            peak_[i] = std::max(peak_[i], sample[i]);
    }
    Skills Finish() {
        if (current_ >= 0) Flush();
        Skills result{};
        std::vector<double> values;
        values.reserve(peaks_.size());
        for (std::size_t i = 0; i < SkillCount; ++i) {
            values.clear();
            for (const auto& p : peaks_)
                if (p[i] > 0.0) values.push_back(p[i]);
            std::sort(values.begin(), values.end(), std::greater<double>{});
            double weighted = 0.0, weight = 1.0;
            for (double v : values) {
                weighted += weight * v * v;
                weight *= kPeakWeight;
                if (weight < 1e-12) break;
            }
            result[i] = std::sqrt((1.0 - kPeakWeight) * weighted);
        }
        return result;
    }
private:
    void Flush() {
        peaks_.push_back(peak_);
        if (!timeline_ || !std::isfinite(durationMs_) || durationMs_<=0) return;
        const double start=static_cast<double>(firstMs_+current_*kSectionMs);
        const double end=start+kSectionMs;
        if (end<=0 || start>=durationMs_) return;
        const double count=DifficultyTimeline::kBins;
        const auto lo=static_cast<std::size_t>(std::clamp(
            std::floor(start/durationMs_*count),0.0,count-1));
        const auto hi=static_cast<std::size_t>(std::clamp(
            std::ceil(end/durationMs_*count)-1,0.0,count-1));
        const double strain=PhysicalNorm(peak_);
        for (auto i=lo;i<=hi;++i)
            timeline_->peaks[i]=std::max(timeline_->peaks[i],strain);
        timeline_->maximum=std::max(timeline_->maximum,strain);
    }
    DifficultyTimeline* timeline_;
    double durationMs_;
    double firstMs_;
    std::int64_t current_ = -1;
    Skills peak_{};
    std::vector<Skills> peaks_;
};

// Surprise is change in a sequence, rather than merely change of adjacent
// gaps. A stable alternating rhythm earns a discount after two repetitions.
// The bounded history requires actual evidence, not an accidental match.
double RhythmSurprise(double dt, const std::array<double, 8>& gaps,
                      std::size_t count) {
    if (count == 0 || dt > 2.0 || gaps[0] > 2.0) return 0.0;
    const auto distance = [](double a, double b) {
        return std::min(3.0, std::abs(std::log2(a / b)));
    };
    const double adjacent = distance(dt, gaps[0]);
    double novelty = adjacent;
    for (std::size_t period = 2; period <= 4; ++period) {
        if (count < 2 * period - 1) continue;
        double error = distance(dt, gaps[period-1]);
        for (std::size_t j = 0; j < period-1; ++j)
            error += distance(gaps[j], gaps[j+period]);
        error /= static_cast<double>(period);
        novelty = std::min(novelty, error);
    }
    return 0.20 * adjacent + 0.80 * novelty;
}

class InputStrain {
public:
    Skills Add(const Note& note, const Note* prev) {
        const double dt = prev ? (note.timeMs - prev->timeMs) / 1000.0 : 0.0;
        const auto reused = prev ? Bits(note.mask & prev->mask) : 0;
        const auto changed = prev ? Bits(note.mask ^ prev->mask) : 0;
        // Global physical press rate and independent per-finger histories.
        speed_ = speed_ * Decay(dt, kSpeedTau) + note.keys / kSpeedTau;
        double hardest = 0.0, squares = 0.0, fingerRhythm = 0.0;
        for (std::size_t f = 0; f < fingers_.size(); ++f) {
            auto& finger = fingers_[f];
            finger.strain *= Decay(dt, kFingerTau);
            if (note.mask & (1u << f)) {
                double increment = 1.0;
                if (finger.seen) {
                    const double gap = (note.timeMs - finger.lastMs) / 1000.0;
                    // Short same-finger intervals cost extra, even if
                    // intervening events use entirely different directions.
                    increment += 0.65 * std::pow(0.125 / (gap + 0.025), 1.2);
                    if (finger.lastGap > 0.0 && gap <= 1.0 && finger.lastGap <= 1.0)
                        fingerRhythm += std::min(2.0,
                            std::abs(std::log2(gap / finger.lastGap)));
                    finger.lastGap = gap;
                }
                finger.strain += increment / kFingerTau;
                finger.lastMs = note.timeMs;
                finger.seen = true;
            }
            hardest = std::max(hardest, finger.strain);
            squares += finger.strain * finger.strain;
        }
        // A busy anchor dominates, while two busy fingers still cost more
        // than one. Rotations and reflections only permute the four states.
        const double fingerLoad = 0.75 * hardest + 0.25 * std::sqrt(squares);

        // Chord synchronization and release/retrigger of an overlapping
        // chord. Ordinary jacks are already accounted for by finger strain.
        // No heading angles, path coordinates or assumed hand assignment.
        double coordinationImpulse = note.keys == 2 ? 0.85 : 0.0;
        if (prev) {
            const bool chordTransition = note.keys == 2 || prev->keys == 2;
            coordinationImpulse += 0.08 * changed;
            if (chordTransition && reused > 0)
                coordinationImpulse += (0.40 + 0.32 * reused) *
                    (0.12 / (dt + 0.025));
        }
        coordination_ = coordination_ * Decay(dt, kCoordinationTau) +
            coordinationImpulse / kCoordinationTau;

        double surprise = 0.0;
        if (prev) {
            if (dt > 2.0) gapCount_ = 0;
            else {
                surprise = RhythmSurprise(dt, gaps_, gapCount_);
                for (std::size_t j = gaps_.size()-1; j > 0; --j)
                    gaps_[j] = gaps_[j-1];
                gaps_[0] = dt;
                gapCount_ = std::min(gapCount_+1, gaps_.size());
            }
        }
        rhythm_ = rhythm_ * Decay(dt, kRhythmTau) +
            (0.70 * surprise * note.keys + 0.30 * fingerRhythm) / kRhythmTau;

        // Separate slow recovery from speed. Only substantial accumulated
        // physical input contributes to stamina; sparse songs stay easy.
        endurance_ = endurance_ * Decay(dt, kStaminaTau) + note.keys / kStaminaTau;
        return {speed_, fingerLoad, coordination_, rhythm_,
                std::max(0.0, endurance_ - 2.0)};
    }
private:
    struct FingerState {
        bool seen = false;
        double lastMs = 0.0;
        double lastGap = 0.0;
        double strain = 0.0;
    };
    std::array<FingerState, 4> fingers_{};
    std::array<double, 8> gaps_{};
    std::size_t gapCount_ = 0;
    double speed_ = 0.0, coordination_ = 0.0, rhythm_ = 0.0, endurance_ = 0.0;
};

} // namespace

ChartDifficulty CalculateChartDifficulty(const Chart& chart,
                                         double offsetSeconds,
                                         DifficultyTimeline* timeline,
                                         double durationSeconds,
                                         double clockRate) {
    if (timeline) *timeline={};
    ChartDifficulty result;
    if (!std::isfinite(chart.bpm) || chart.bpm <= 0.0 ||
        chart.ticksPerBeat <= 0 || !std::isfinite(offsetSeconds) ||
        std::abs(offsetSeconds) > 3600.0 || !std::isfinite(clockRate) ||
        clockRate < 0.5 || clockRate > 1.5)
        return result;

    // Difficulty needs relative distances between exact note timestamps,
    // not the song endpoint. Imported JSON deliberately has lengthBeats=1
    // until the audio is loaded; creating a gameplay route here is incorrect.
    std::vector<Note> notes;
    notes.reserve(chart.turns.size());
    int lastTick = chart.firstBeat;
    Point position{};
    Heading preceding = chart.initialHeading;
    std::int64_t lastTime = std::numeric_limits<std::int64_t>::min();
    std::size_t totalKeys = 0;
    for (const auto& event : chart.turns) {
        if (!ValidHeading(event.direction) || event.beat < lastTick)
            continue;
        const std::int64_t currentTime =
            EventTimeMs(chart, event, offsetSeconds);
        if (currentTime < -3600000 || currentTime > 86400000 ||
            currentTime <= lastTime)
            continue;
        // Equivalent to the time-indexed gameplay trajectory, but invariant
        // under translation and independent of chart length or audio data.
        if (lastTime != std::numeric_limits<std::int64_t>::min()) {
            // Constant visual-reference units per real second. Neither the
            // authored grid nor any BPM segment affects path readability.
            const double distance = static_cast<double>(currentTime - lastTime)
                / (500.0 * clockRate);
            const Point unit = Unit(preceding);
            position.x += unit.x * distance;
            position.y += unit.y * distance;
        }
        const unsigned mask = HeadingArrowMask(event.direction);
        const int keys = Bits(mask);
        if (keys == 0) continue;
        notes.push_back({static_cast<double>(currentTime) / clockRate, position,
                         event.direction, mask, keys});
        totalKeys += static_cast<std::size_t>(keys);
        lastTick = event.beat;
        lastTime = currentTime;
        preceding = event.direction;
    }
    const std::size_t n = notes.size();
    result.notes = n;
    result.keyPresses = totalKeys;
    if (n == 0) return result;

    InputStrain input;
    SectionPeaks peaks(timeline,durationSeconds / clockRate,notes.front().timeMs);
    // Geometry is a separate editor diagnostic. It is deliberately absent
    // from the input strain calculation and final rating.
    for (std::size_t i = 0; i < n; ++i) {
        Note& cur = notes[i];
        const Note* prev = i ? &notes[i-1] : nullptr;
        peaks.Add(cur.timeMs - notes.front().timeMs, input.Add(cur, prev));
        if (!prev) continue;
        const double dt = (cur.timeMs - prev->timeMs) / 1000.0;
        result.activeSeconds += std::min(1.5, dt);
        const int angle = std::abs(HeadingIndex(cur.heading) - HeadingIndex(prev->heading));
        const int eighths = std::min(angle, 8-angle);
        const double urgency = 1.0 + 0.8 * std::clamp((0.40 - dt) / 0.40, 0.0, 1.0);
        cur.reading = (static_cast<double>(eighths) / 4.0) * urgency;
        double clutter = 0.0;
        for (std::size_t back = 2; back <= 12 && back < i; ++back) {
            const auto j = i - back;
            if (j == 0 || cur.timeMs - notes[j].timeMs > 4000) break;
            const double gap = SegmentDistance(prev->pos, cur.pos, notes[j-1].pos, notes[j].pos);
            if (gap < 0.7) clutter += 0.35 + 0.42 * (1.0 - gap / 0.7);
        }
        cur.reading += std::min(1.9, clutter) + (eighths == 4 ? 0.40 : 0.0);
    }
    const auto skills = peaks.Finish();
    result.speed = skills[Speed];
    result.fingerStrain = skills[Finger];
    result.coordination = skills[Coordination];
    result.rhythm = skills[Rhythm];
    result.stamina = skills[Stamina];

    // Stable 1.8s density telemetry and the separate visual diagnostic.
    // The old active-wide average/percentile cannot dilute the new rating.
    std::vector<double> noteRates, readRates, keyRates;
    noteRates.reserve(n); readRates.reserve(n); keyRates.reserve(n);
    std::size_t first = 0;
    int keys = 0;
    double reading = 0.0;
    constexpr double window = 1.8;
    for (std::size_t i = 0; i < n; ++i) {
        keys += notes[i].keys;
        reading += notes[i].reading;
        while (first < i && notes[i].timeMs - notes[first].timeMs > 1800) {
            keys -= notes[first].keys;
            reading -= notes[first].reading;
            ++first;
        }
        const double nps = static_cast<double>(i - first + 1) / window;
        const double kps = keys / window;
        noteRates.push_back(nps);
        keyRates.push_back(kps);
        readRates.push_back(reading / window);
        result.notesPerSecondPeak = std::max(result.notesPerSecondPeak, nps);
        result.keysPerSecondPeak = std::max(result.keysPerSecondPeak, kps);
    }
    const double n85 = Percentile(noteRates, 0.85);
    result.reading = Nonnegative(
        0.80 * (0.75 * Percentile(readRates, 0.85) + 0.25 * Percentile(readRates, 0.95)) *
        std::max(0.45, std::sqrt(n85 / 3.0)));
    result.peak = Nonnegative(0.58 * (0.75 * Percentile(keyRates, 0.99) +
        0.25 * result.keysPerSecondPeak - 1.0));

    // All weighted skills are physical input skills. A geometric crossing
    // cannot raise the rating independently of the actual key sequence.
    result.overall = Nonnegative(PhysicalNorm(skills));
    return result;
}

} // namespace locus::core
