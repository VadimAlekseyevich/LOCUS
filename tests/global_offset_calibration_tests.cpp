#include "app/global_offset_calibrator.h"

#include <cmath>
#include <iostream>

using locus::app::CalibratedSongSeconds;
using locus::app::GlobalOffsetCalibrator;

namespace {
bool Check(bool yes, const char* text) {
    if (!yes) std::cerr << "OFFSET_CALIBRATION_FAIL " << text << '\n';
    return yes;
}
}

int main() {
    bool ok = true;
    ok &= Check(std::abs(CalibratedSongSeconds(10.0, 200) - 9.8) < 1e-9 &&
                std::abs(CalibratedSongSeconds(10.0, -120) - 10.12) < 1e-9,
                "positive output latency moves both scoring and visuals later");

    GlobalOffsetCalibrator sample;
    sample.Start();
    ok &= Check(!sample.Tap(0.17) && !sample.Tap(2.12) &&
                sample.Count() == 0, "first two measures are warmup");
    const int errors[]{110, 115, 108, 120, 112, 500, 114, 116};
    // Outlier at 500ms intentionally ignored; substitute 345ms, which
    // counts as an outlying tap but must not skew robust estimate.
    const int outliers[]{110, 115, 108, 120, 112, 345, 114, 116};
    for (int i = 0; i < 8; ++i) {
        const double base = static_cast<double>(4 + 2 * i);
        const double at = base + outliers[i] / 1000.0;
        ok &= Check(sample.Tap(at), "valid accented click tap recorded");
        ok &= Check(!sample.Tap(at + 0.05), "duplicate measure tap rejected");
    }
    (void)errors;
    ok &= Check(sample.Complete() && sample.Reliable() &&
                sample.Count() == 8 &&
                std::abs(sample.SuggestedMs() - 114) < 10 &&
                sample.JitterMs() < 15,
                "outlier-resistant recommendation uses stable tap median");
    ok &= Check(!sample.Tap(22.1), "completed session ignores extra taps");

    sample.Start();
    ok &= Check(sample.Count() == 0 && !sample.Complete() &&
                !sample.Reliable(), "retry resets state");
    for (int i = 0; i < 8; ++i) {
        const double at = 4.0 + 2.0 * i - 0.08;
        ok &= Check(sample.Tap(at), "early audible beat is measurable");
    }
    ok &= Check(sample.Reliable() &&
                sample.SuggestedMs() == -80,
                "negative delay recommendation preserves sign");

    sample.Start();
    for (int i = 0; i < 8; ++i) {
        const double err = i % 2 ? 230.0 : -230.0;
        ok &= Check(sample.Tap(4.0 + 2.0 * i + err / 1000.0),
                    "variable but valid taps recorded");
    }
    ok &= Check(sample.Complete() && !sample.Reliable(),
                "unstable tapping cannot yield an automatic setting");
    if (!ok) return 1;
    std::cout << "GLOBAL_OFFSET_CALIBRATION_PASS\n";
    return 0;
}
