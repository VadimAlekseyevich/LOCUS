#include "app/play_adjustments.h"

#include <cmath>
#include <iostream>
#include <utility>

using namespace locus::app;

namespace {
bool Check(bool value, const char* what) {
    if (!value) std::cerr << "PLAY_ADJUST_FAIL " << what << '\n';
    return value;
}
}

int main() {
    bool ok = true;
    ok &= Check(AdjustPreviewTimeMs(800.0f, 1) == 850.0f &&
                AdjustPreviewTimeMs(800.0f, -2) == 700.0f,
                "preview steps by 50 ms");
    ok &= Check(AdjustPreviewTimeMs(75.0f, -50) == 50.0f &&
                AdjustPreviewTimeMs(9950.0f, 10) == 10000.0f,
                "preview clamps to skin bounds");
    ok &= Check(AdjustPreviewTimeMs(800.25f, 0) == 800.25f &&
                AdjustPreviewTimeMs(NAN, 1) == 800.0f,
                "idle preserves typed precision, nonfinite uses default");
    ok &= Check(AdjustBaseSpeed(135.0f, 4) == 175.0f &&
                AdjustBaseSpeed(20.0f, -10) == 20.0f,
                "speed supports batched repeat with clamping");
    PlayAdjustmentRepeater repeat;
    const auto tick = [&](int speed, int preview, float dt) {
        return repeat.Advance(speed, preview, dt);
    };
    ok &= Check(tick(1, -1, 0.016f) == std::make_pair(0, 0),
                "fresh hold does not duplicate initial press");
    for (int i = 0; i < 2; ++i)
        ok &= Check(tick(1, -1, 0.1f) == std::make_pair(0, 0),
                    "no repeat before 230ms");
    const auto first = tick(1, -1, 0.1f);
    ok &= Check(first.first > 0 && first.second < 0,
                "both held axes repeat");
    int repeats = 0;
    for (int i = 0; i < 12; ++i) {
        const auto output = tick(1, -1, 0.1f);
        repeats += output.first;
        ok &= Check(output.first == -output.second,
                    "independent repeat axes have same timing");
    }
    ok &= Check(repeats >= 12, "long hold accelerates");
    ok &= Check(tick(0, 0, 0.016f) == std::make_pair(0, 0) &&
                tick(-1, 1, 0.016f) == std::make_pair(0, 0) &&
                tick(-1, 1, NAN) == std::make_pair(0, 0),
                "release, reversal and nonfinite dt do not leak state");
    if (!ok) return 1;
    std::cout << "PLAY_ADJUST_PASS repeat=1 preview=1 bounds=1\n";
    return 0;
}
