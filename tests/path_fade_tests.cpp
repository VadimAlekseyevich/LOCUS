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
}

int main() {
    bool ok = true;
    ok &= Expect(Near(ScoredPathOpacity(0.0), 1.0) &&
                 Near(ScoredPathWidth(0.0), 6.0),
                 "line is opaque and wide directly ahead of marker");
    ok &= Expect(ScoredPathOpacity(-0.000001) == 0.0f &&
                 ScoredPathOpacity(-0.5) == 0.0f &&
                 ScoredPathWidth(-0.1) == 0.0f,
                 "nothing at all visible after player passes a point");
    ok &= Expect(Near(ScoredPathOpacity(.5), .5) &&
                 ScoredPathOpacity(.25) > ScoredPathOpacity(.5) &&
                 ScoredPathOpacity(.5) > ScoredPathOpacity(.75) &&
                 ScoredPathWidth(.25) > ScoredPathWidth(.5) &&
                 ScoredPathWidth(.5) > ScoredPathWidth(.75),
                 "farther line gets steadily dimmer and thinner");
    ok &= Expect(ScoredPathOpacity(1.0) == 0.0f &&
                 ScoredPathOpacity(1.001) == 0.0f &&
                 ScoredPathWidth(1.0) == 0.0f &&
                 ScoredPathOpacity(std::numeric_limits<double>::quiet_NaN()) == 0.0f &&
                 ScoredPathOpacity(std::numeric_limits<double>::infinity()) == 0.0f,
                 "exact end of next beat is invisible and invalid values are safe");

    const auto before = ScoredPathVisibleInterval(8.25, 7);
    const auto current = ScoredPathVisibleInterval(8.25, 8);
    const auto next = ScoredPathVisibleInterval(8.25, 9);
    const auto far = ScoredPathVisibleInterval(8.25, 10);
    ok &= Expect(!before.Visible() && !far.Visible() &&
                 Near(current.begin, 8.25) && Near(current.end, 9.0) &&
                 Near(next.begin, 9.0) && Near(next.end, 9.25),
                 "visible interval is only one beat ahead, even across a turn");

    const auto atEnd = ScoredPathVisibleInterval(8.9999, 8);
    const auto atNext = ScoredPathVisibleInterval(9.0, 9);
    const auto behind = ScoredPathVisibleInterval(9.0, 8);
    ok &= Expect(Near(atEnd.begin, 8.9999) && Near(atEnd.end, 9.0) &&
                 Near(atNext.begin, 9.0) && Near(atNext.end, 10.0) &&
                 !behind.Visible(),
                 "past line clips exactly at marker when crossing integer beat");

    // A few thousand samples protect against regressions where the front
    // and back of the visible window jump at a frame boundary.
    for (int i = 0; i <= 4000; ++i) {
        const double beat = 11.0 + i / 4000.0;
        const int first = static_cast<int>(std::floor(beat));
        const auto sliceA = ScoredPathVisibleInterval(beat, first);
        const auto sliceB = ScoredPathVisibleInterval(beat, first + 1);
        if (sliceA.Visible() && (sliceA.begin < beat || sliceA.end > beat + 1.0))
            ok &= Expect(false, "current beat never trails marker");
        if (sliceB.Visible() && (sliceB.begin < beat || sliceB.end > beat + 1.0))
            ok &= Expect(false, "next beat never exceeds one-beat visibility");
        if (sliceA.Visible() && sliceB.Visible() && !Near(sliceA.end, sliceB.begin))
            ok &= Expect(false, "visible path remains joined across the turn");
    }

    if (!ok) return 1;
    std::cout << "PATH_FADE_PASS forward-only=1 one-beat=1 continuous=1\n";
    return 0;
}
