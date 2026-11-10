#include "app/adaptive_camera.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {
bool Check(bool okay, const char* label) {
    if (!okay) std::cerr << "ADAPTIVE_CAMERA_FAIL " << label << '\n';
    return okay;
}
bool Near(double a, double b, double tolerance = 1e-4) {
    return std::abs(a - b) < tolerance;
}
double PixelOffset(const locus::core::GridPoint& marker,
                   const locus::core::GridPoint& anchor, float step) {
    return std::hypot((marker.x - anchor.x) * step,
                      (marker.y - anchor.y) * step);
}
}

int main() {
    using namespace locus;
    bool ok = true;
    core::Chart chart;
    chart.bpm = 120;
    chart.ticksPerBeat = 24;
    chart.lengthBeats = 1200;
    chart.initialHeading = core::Heading::Right;
    const double beat = 100.0;

    const auto marker = core::RoutePositionAt(chart, beat);
    const auto sample = [&](float speed, double rate, double atBeat) {
        const float step = app::StepForFixedSpeed(
            speed, chart.bpm, chart.ticksPerBeat, rate);
        return app::AdaptiveCameraAnchor(chart, atBeat, step, rate);
    };
    const auto slow = sample(100.0f, 1.0, beat);
    const auto faster = sample(200.0f, 1.0, beat);
    const float slowStep = app::StepForFixedSpeed(100.0f, 120, 24);
    const float fastStep = app::StepForFixedSpeed(200.0f, 120, 24);
    ok &= Check(slow.x > marker.x && Near(slow.y, marker.y) &&
                Near(PixelOffset(marker, slow, slowStep), 18.0) &&
                Near(PixelOffset(marker, faster, fastStep), 36.0),
                "camera leads in travel direction and offset grows linearly with speed");

    const auto halfRate = sample(200.0f, 0.5, beat);
    const auto twiceRate = sample(200.0f, 1.0, beat);
    const float halfStep = app::StepForFixedSpeed(200.0f, 120, 24, 0.5);
    ok &= Check(Near(PixelOffset(marker, halfRate, halfStep),
                     PixelOffset(marker, twiceRate, fastStep)),
                "practice playback does not change physical camera displacement");

    const auto maximum = sample(2000.0f, 1.0, beat);
    const float maxStep = app::StepForFixedSpeed(2000.0f, 120, 24);
    ok &= Check(Near(PixelOffset(marker, maximum, maxStep),
                     app::kAdaptiveCameraMaxOffsetPx),
                "extreme speed cannot push the marker out of safe playfield");

    core::Chart diagonal = chart;
    diagonal.initialHeading = core::Heading::UpRight;
    const auto diagonalMarker = core::RoutePositionAt(diagonal, beat);
    const auto diagonalAnchor = app::AdaptiveCameraAnchor(
        diagonal, beat, fastStep);
    ok &= Check(Near(PixelOffset(diagonalMarker, diagonalAnchor, fastStep), 36.0) &&
                diagonalAnchor.x > diagonalMarker.x &&
                diagonalAnchor.y < diagonalMarker.y,
                "diagonal headings retain equal movement speed and 2-D offset");

    // At a corner both route and camera anchor remain continuous. No frame
    // timer, acceleration state or tick rounding can introduce a teleport.
    core::Chart corner = chart;
    corner.turns.push_back({100, core::Heading::Up});
    const auto before = app::AdaptiveCameraAnchor(
        corner, 99.999, fastStep);
    const auto after = app::AdaptiveCameraAnchor(
        corner, 100.001, fastStep);
    ok &= Check(std::hypot(after.x - before.x,
                           after.y - before.y) * fastStep < 0.1,
                "camera travels continuously across authored turns");

    const auto end = app::AdaptiveCameraAnchor(
        chart, static_cast<double>(chart.lengthBeats), fastStep);
    ok &= Check(Near(PixelOffset(
                    core::RoutePositionAt(chart, chart.lengthBeats),
                    end, fastStep), 0.0),
                "camera recenters naturally at the end of the route");

    const auto frozenOne = sample(135.0f, 1.0, 400.25);
    const auto frozenTwo = sample(135.0f, 1.0, 400.25);
    ok &= Check(Near(frozenOne.x, frozenTwo.x) &&
                Near(frozenOne.y, frozenTwo.y),
                "same timestamp always projects to the same camera position");

    if (ok) std::cout << "ADAPTIVE_CAMERA_PASS capped=1 diagonal=1 deterministic=1\n";
    return ok ? 0 : 1;
}
