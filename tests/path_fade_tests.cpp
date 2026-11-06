#include "app/visual_style.h"

#include <cmath>
#include <iostream>
#include <limits>

using namespace locus::app;

namespace {
bool Expect(bool condition, const char* name) {
    if (!condition) std::cerr << "PATH_FADE_FAIL " << name << '\n';
    return condition;
}
bool Near(double x, double y) { return std::abs(x - y) < 1e-8; }
} // namespace

int main() {
    bool ok = true;
    ok &= Expect(Near(kScoredPathLengthBeats, 4.0),
                 "scored path must preview four beats, one full measure");
    ok &= Expect(Near(ScoredPathOpacity(0.0), 1.0) &&
                 Near(ScoredPathWidth(0.0), 6.0),
                 "line is opaque and thick immediately ahead of the player");
    ok &= Expect(ScoredPathOpacity(-0.000001) == 0.0f &&
                 ScoredPathOpacity(-1.0) == 0.0f &&
                 ScoredPathWidth(-0.1) == 0.0f,
                 "traversed line vanishes immediately behind the player");
    ok &= Expect(ScoredPathOpacity(1.0) > 0.80f &&
                 ScoredPathOpacity(2.0) > 0.65f &&
                 ScoredPathOpacity(3.0) > 0.30f &&
                 ScoredPathWidth(3.0) > 2.0f,
                 "route and turn targets stay readable multiple beats ahead");
    ok &= Expect(ScoredPathOpacity(0.0) > ScoredPathOpacity(1.0) &&
                 ScoredPathOpacity(1.0) > ScoredPathOpacity(2.0) &&
                 ScoredPathOpacity(2.0) > ScoredPathOpacity(3.0) &&
                 ScoredPathOpacity(3.0) > ScoredPathOpacity(3.9) &&
                 ScoredPathWidth(0.0) > ScoredPathWidth(1.0) &&
                 ScoredPathWidth(1.0) > ScoredPathWidth(2.0) &&
                 ScoredPathWidth(2.0) > ScoredPathWidth(3.0),
                 "trajectory gradually fades and narrows across the measure");
    ok &= Expect(ScoredPathOpacity(4.0) == 0.0f &&
                 ScoredPathOpacity(4.001) == 0.0f &&
                 ScoredPathWidth(4.0) == 0.0f &&
                 ScoredPathOpacity(std::numeric_limits<double>::quiet_NaN()) == 0.0f &&
                 ScoredPathOpacity(std::numeric_limits<double>::infinity()) == 0.0f,
                 "exact end of the four-beat preview is invisible");

    const auto behind = ScoredPathVisibleInterval(8.25, 7);
    const auto current = ScoredPathVisibleInterval(8.25, 8);
    const auto next = ScoredPathVisibleInterval(8.25, 9);
    const auto third = ScoredPathVisibleInterval(8.25, 10);
    const auto fourth = ScoredPathVisibleInterval(8.25, 11);
    const auto fifth = ScoredPathVisibleInterval(8.25, 12);
    const auto beyond = ScoredPathVisibleInterval(8.25, 13);
    ok &= Expect(!behind.Visible() && !beyond.Visible() &&
                 Near(current.begin, 8.25) && Near(current.end, 9.0) &&
                 Near(next.begin, 9.0) && Near(next.end, 10.0) &&
                 Near(third.begin, 10.0) && Near(third.end, 11.0) &&
                 Near(fourth.begin, 11.0) && Near(fourth.end, 12.0) &&
                 Near(fifth.begin, 12.0) && Near(fifth.end, 12.25),
                 "fractional four-beat window spans five contiguous beat segments");

    const auto endOfBeat = ScoredPathVisibleInterval(8.9999, 8);
    const auto beginningNext = ScoredPathVisibleInterval(9.0, 9);
    const auto alreadyPassed = ScoredPathVisibleInterval(9.0, 8);
    ok &= Expect(Near(endOfBeat.begin, 8.9999) &&
                 Near(endOfBeat.end, 9.0) &&
                 Near(beginningNext.begin, 9.0) &&
                 Near(beginningNext.end, 10.0) &&
                 !alreadyPassed.Visible(),
                 "current beat clips exactly to marker on integer beat change");

    // Exercise 4,001 successive sub-beat positions. The future path always
    // covers exactly four beats, including across turns, with no past tail.
    for (int i = 0; i <= 4000; ++i) {
        const double beat = 11.0 + i / 4000.0;
        const int firstBeat = static_cast<int>(std::floor(beat));
        double covered = 0.0;
        double previousEnd = beat;
        for (int target = firstBeat; target <= firstBeat + 4; ++target) {
            const auto interval = ScoredPathVisibleInterval(beat, target);
            if (!interval.Visible()) continue;
            if (interval.begin < beat - 1e-8 ||
                interval.end > beat + 4.0 + 1e-8 ||
                !Near(interval.begin, previousEnd))
                ok &= Expect(false, "window contains only connected upcoming path");
            covered += interval.end - interval.begin;
            previousEnd = interval.end;
        }
        if (!Near(covered, 4.0) || !Near(previousEnd, beat + 4.0)) {
            ok &= Expect(false, "every fractional frame shows a full four beats");
            break;
        }
    }

    if (!ok) return 1;
    std::cout << "PATH_FADE_PASS forward-only=1 measure=4 continuous=1\n";
    return 0;
}
