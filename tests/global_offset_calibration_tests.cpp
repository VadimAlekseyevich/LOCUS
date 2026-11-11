#include "app/global_offset_calibrator.h"

#include <cmath>
#include <iostream>

using locus::app::CalibratedSongSeconds;
using locus::app::GlobalOffsetCalibrator;

namespace {
bool Check(bool yes, const char* description) {
    if (!yes) std::cerr << "OFFSET_CALIBRATION_FAIL " << description << '\n';
    return yes;
}
}

int main() {
    bool ok = true;
    ok &= Check(std::abs(CalibratedSongSeconds(10.0, 200) - 9.8) < 1e-9 &&
                std::abs(CalibratedSongSeconds(10.0, -120) - 10.12) < 1e-9,
                "positive output delay shifts judgement and graphics backward");
    ok &= Check(std::abs(CalibratedSongSeconds(10.0, 200, 0.5) - 9.9) < 1e-9 &&
                std::abs(CalibratedSongSeconds(10.0, 200, 0.1) - 9.98) < 1e-9,
                "same real-time offset works at half and tenth practice speed");
    ok &= Check(GlobalOffsetCalibrator::kBpm == 100.0 &&
                GlobalOffsetCalibrator::kRequiredTaps == 18 &&
                std::abs(GlobalOffsetCalibrator::kBeatSeconds - 0.6) < 1e-9,
                "calibration follows each 100 BPM quarter beat for 18 taps");

    GlobalOffsetCalibrator sample;
    sample.Start();
    // No mandatory introductory beats: the very first beat can be used.
    ok &= Check(sample.Tap(0.11) &&
                !sample.Tap(0.14) &&
                sample.Count() == 1,
                "start at the first audible beat and reject duplicate taps");
    sample.Start();

    // Start much later: there is no countdown or strong-beat-only rule.
    // One gross but still accepted tap is an outlier for the robust median.
    for (int i = 0; i < GlobalOffsetCalibrator::kRequiredTaps; ++i) {
        const double errorMs = i == 8 ? 270.0
            : 105.0 + 4.0 * (i % 5);
        const double at = (30 + i) *
            GlobalOffsetCalibrator::kBeatSeconds + errorMs / 1000.0;
        ok &= Check(sample.Tap(at), "one bound-key press per ordinary beat");
        ok &= Check(!sample.Tap(at + 0.05),
                    "fast key combinations and duplicates count once");
    }
    ok &= Check(sample.Complete() && sample.Reliable() &&
                sample.Count() == GlobalOffsetCalibrator::kRequiredTaps &&
                std::abs(sample.SuggestedMs() - 113) < 12 &&
                sample.JitterMs() < 15,
                "outlier-resistant estimate from 18 late taps");
    ok &= Check(!sample.Tap(36.1), "completed session ignores extra taps");

    sample.Start();
    ok &= Check(sample.Count() == 0 && !sample.Complete() &&
                !sample.Reliable(), "retry clears all samples");
    // Skipped beats and a delayed first press must NOT reset progress.
    for (int i = 0; i < GlobalOffsetCalibrator::kRequiredTaps; ++i) {
        const double at = (70 + 2 * i) *
            GlobalOffsetCalibrator::kBeatSeconds - 0.08;
        ok &= Check(sample.Tap(at),
                    "start at any moment, count every other click if skipped");
    }
    ok &= Check(sample.Reliable() && sample.SuggestedMs() == -80,
                "negative measured output delay is preserved");

    sample.Start();
    for (int i = 0; i < GlobalOffsetCalibrator::kRequiredTaps; ++i) {
        const double errorMs = i % 2 ? 230.0 : -230.0;
        ok &= Check(sample.Tap((100 + i) *
                    GlobalOffsetCalibrator::kBeatSeconds + errorMs / 1000.0),
                    "large alternating tap errors are still collected");
    }
    ok &= Check(sample.Complete() && !sample.Reliable(),
                "no suggestion if human input is too inconsistent");

    if (!ok) return 1;
    std::cout << "GLOBAL_OFFSET_CALIBRATION_PASS\n";
    return 0;
}
