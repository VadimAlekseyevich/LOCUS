#include "core/chart_difficulty.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

namespace locus::core {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1e-9;

struct Point { double x = 0.0, y = 0.0; };
struct Note {
    std::int64_t timeMs = 0;
    Point pos;
    Heading heading = Heading::Right;
    unsigned mask = 0;
    int keys = 1;
    double rhythm = 0.0;
    double coordination = 0.0;
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
Point Unit(Heading h) {
    const double angle = (static_cast<double>(HeadingIndex(h)) - 2.0) *
                         kPi / 4.0;
    return {std::cos(angle), std::sin(angle)};
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
} // namespace

ChartDifficulty CalculateChartDifficulty(const Chart& chart,
                                         double offsetSeconds) {
    ChartDifficulty result;
    if (!std::isfinite(chart.bpm) || chart.bpm <= 0.0 ||
        chart.ticksPerBeat <= 0 || !std::isfinite(offsetSeconds) ||
        std::abs(offsetSeconds) > 3600.0)
        return result;

    std::vector<Note> notes;
    notes.reserve(chart.turns.size());
    Point position;
    Heading preceding = chart.initialHeading;
    int lastTick = chart.firstBeat;
    std::int64_t lastTime = std::numeric_limits<std::int64_t>::min();
    const std::int64_t offsetMs =
        static_cast<std::int64_t>(std::llround(offsetSeconds * 1000.0));
    std::size_t totalKeys = 0;
    for (const auto& event : chart.turns) {
        if (!ValidHeading(event.direction) || event.beat < lastTick)
            continue;
        const std::int64_t currentTime =
            EventTimeMs(chart, event, offsetSeconds);
        if (currentTime < -3600000 || currentTime > 86400000 ||
            currentTime <= lastTime)
            continue;
        const Point direction = Unit(preceding);
        const double distance = static_cast<double>(event.beat - lastTick) /
                                chart.ticksPerBeat;
        position.x += direction.x * distance;
        position.y += direction.y * distance;
        const unsigned mask = HeadingArrowMask(event.direction);
        const int keys = Bits(mask);
        if (keys == 0) continue;
        notes.push_back({currentTime, position, event.direction, mask, keys});
        totalKeys += static_cast<std::size_t>(keys);
        preceding = event.direction;
        lastTick = event.beat;
        lastTime = currentTime;
    }
    const std::size_t n = notes.size();
    result.notes = n;
    result.keyPresses = totalKeys;
    if (n == 0) return result;

    std::vector<double> rhythmSamples, coordinationSamples, readingSamples;
    rhythmSamples.reserve(n);
    coordinationSamples.reserve(n);
    readingSamples.reserve(n);
    double previousInterval = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        Note& cur = notes[i];
        if (i == 0) continue;
        const Note& prev = notes[i-1];
        const double dt = (cur.timeMs - prev.timeMs) / 1000.0;
        const int angle = std::abs(HeadingIndex(cur.heading) -
                                   HeadingIndex(prev.heading));
        const int eighths = std::min(angle, 8-angle);
        const double turn = static_cast<double>(eighths) / 4.0;
        // Brief gaps make sharp geometry harder to decode.
        const double urgency = 1.0 + 0.8 * std::clamp(
            (0.40 - dt) / 0.40, 0.0, 1.0);

        // Rhythmic surprise: logarithmic interval change, plus slight
        // emphasis on off-quarter-beat placements. A steady triplet stream
        // should remain relatively readable; BPM is never a raw multiplier.
        double variation = 0.0;
        if (previousInterval > 0.0 && dt <= 2.0 && previousInterval <= 2.0)
            variation = std::min(2.0, std::abs(
                std::log2(dt / previousInterval)));
        const double beat = static_cast<double>(cur.timeMs - offsetMs) *
                            chart.bpm / 60000.0;
        const double quarterDistance = std::abs(beat * 4.0 - std::round(beat * 4.0));
        const double offbeat = std::min(quarterDistance * 2.0, 1.0);
        cur.rhythm = std::min(2.5,
            variation * 0.85 + offbeat * 0.13 +
            (dt < 0.22 ? 0.23 : 0.0));
        previousInterval = dt;

        const int changed = Bits(cur.mask ^ prev.mask);
        const bool repeated = cur.mask == prev.mask;
        cur.coordination = (cur.keys == 2 ? 1.0 : 0.0) +
            (changed >= 2 ? 0.55 : changed == 1 ? 0.18 : 0.0) +
            (changed == 4 ? 0.55 : 0.0) +
            (repeated && dt < 0.19 ? 0.52 : 0.0);
        cur.coordination *= std::clamp(0.55 / dt, 0.30, 1.8);

        cur.reading = turn * urgency;
        // Scan only nearby route pieces within the last four seconds:
        // actual path returns, intersections and overlapping strokes
        // obscure which of the visually close turns is next.
        double localClutter = 0.0;
        for (std::size_t back = 2; back <= 12 && back < i; ++back) {
            const std::size_t j = i - back;
            if (j == 0 || cur.timeMs - notes[j].timeMs > 4000) break;
            const double gap = SegmentDistance(
                prev.pos, cur.pos, notes[j-1].pos, notes[j].pos);
            if (gap < 0.7) {
                const double severity = 1.0 - gap / 0.7;
                localClutter += 0.35 + 0.42 * severity;
            }
        }
        cur.reading += std::min(1.9, localClutter) +
                       (eighths == 4 ? 0.40 : 0.0);
        rhythmSamples.push_back(cur.rhythm);
        coordinationSamples.push_back(cur.coordination);
        readingSamples.push_back(cur.reading);
        result.activeSeconds += std::min(1.5, dt);
    }

    // A fixed time window handles both full streams and micro-bursts. The
    // peak is informative, but percentile load resists one-note outliers.
    std::vector<double> keyRates, noteRates, coordRates, readRates, rhythmRates;
    keyRates.reserve(n); noteRates.reserve(n);
    coordRates.reserve(n); readRates.reserve(n); rhythmRates.reserve(n);
    std::size_t first = 0;
    int rollingKeys = 0;
    double rollingCoord = 0.0, rollingRead = 0.0, rollingRhythm = 0.0;
    constexpr double window = 1.8;
    constexpr std::int64_t windowMs = 1800;
    for (std::size_t i = 0; i < n; ++i) {
        rollingKeys += notes[i].keys;
        rollingCoord += notes[i].coordination;
        rollingRead += notes[i].reading;
        rollingRhythm += notes[i].rhythm;
        while (first < i &&
               notes[i].timeMs - notes[first].timeMs > windowMs) {
            rollingKeys -= notes[first].keys;
            rollingCoord -= notes[first].coordination;
            rollingRead -= notes[first].reading;
            rollingRhythm -= notes[first].rhythm;
            ++first;
        }
        const double nps = static_cast<double>(i - first + 1) / window;
        const double kps = rollingKeys / window;
        noteRates.push_back(nps);
        keyRates.push_back(kps);
        coordRates.push_back(rollingCoord / window);
        readRates.push_back(rollingRead / window);
        rhythmRates.push_back(rollingRhythm / window);
        result.notesPerSecondPeak = std::max(result.notesPerSecondPeak, nps);
        result.keysPerSecondPeak = std::max(result.keysPerSecondPeak, kps);
    }
    const double q85Keys = Percentile(keyRates, 0.85);
    const double q85Notes = Percentile(noteRates, 0.85);
    // Local density alone overvalues a short burst. Include the number of
    // physical presses over the chart's active spans, with rests above 1.5 s
    // contributing only 1.5 s to the denominator. Repeated dense passages
    // therefore matter even when the fastest small window is unchanged.
    const double activeKeysPerSecond = result.activeSeconds > 0.0
        ? static_cast<double>(totalKeys) / result.activeSeconds : 0.0;
    result.speed = Nonnegative(1.35 * (
        0.55*q85Notes + 0.20*q85Keys + 0.25*activeKeysPerSecond));

    const double avgRhythm = rhythmSamples.empty() ? 0.0 :
        std::accumulate(rhythmSamples.begin(), rhythmSamples.end(), 0.0) /
        rhythmSamples.size();
    result.rhythm = Nonnegative(
        (0.38 * Percentile(rhythmRates, 0.85) + 0.30*avgRhythm) *
        std::max(0.30, q85Notes / 3.0));
    result.coordination = Nonnegative(
        0.50 * (0.75*Percentile(coordRates, 0.85) +
                0.25*Percentile(coordRates, 0.95)));
    result.reading = Nonnegative(
        0.80 * (0.75*Percentile(readRates, 0.85) +
                0.25*Percentile(readRates, 0.95)) *
        std::max(0.45, std::sqrt(q85Notes / 3.0)));
    // Both how long a dense pattern persists and how many presses it
    // requires matter. Quiet songs and isolated notes earn no stamina.
    // Square-root duration grows indefinitely, but at a diminishing rate.
    result.stamina = Nonnegative(
        0.85 * std::sqrt(result.activeSeconds / 30.0) *
        std::max(0.0, activeKeysPerSecond - 1.0));
    result.peak = Nonnegative(
        0.48 * (0.75*Percentile(keyRates, 0.99) +
                0.25*result.keysPerSecondPeak - 1.0));
    const double energy =
        0.52*result.speed*result.speed +
        0.14*result.rhythm*result.rhythm +
        0.14*result.coordination*result.coordination +
        0.15*result.reading*result.reading +
        0.05*result.stamina*result.stamina;
    // Confidence adjusts tiny samples; it is NOT a difficulty ceiling.
    const double sampleConfidence = 0.70 + 0.30 *
        std::min(1.0, static_cast<double>(n)/24.0);
    result.overall = Nonnegative((
        std::sqrt(energy) + result.peak * 0.08) *
        sampleConfidence);
    return result;
}

} // namespace locus::core
