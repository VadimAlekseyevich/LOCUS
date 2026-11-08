#include "app/reverse_cap.h"

#include <array>
#include <cmath>
#include <iostream>

namespace {
bool Check(bool ok, const char* description) {
    if (!ok) std::cerr << "REVERSE_CAP_FAIL " << description << '\n';
    return ok;
}
bool Near(double a, double b) { return std::abs(a - b) < 1e-5; }
}

int main() {
    using namespace locus;
    bool ok = true;

    // Incoming RIGHT -> outgoing LEFT: the cap points RIGHT. Its flat
    // diameter is vertical, NOT horizontal/parallel to the route.
    const app::ReverseCapArc rightFacing =
        app::ReverseCapForOutgoing(core::Heading::Left);
    ok &= Check(Near(rightFacing.startDegrees, -90.0) &&
                Near(rightFacing.endDegrees, 90.0),
                "right-to-left reversal has a rightward bulge and vertical diameter");
    const app::ReverseCapArc leftFacing =
        app::ReverseCapForOutgoing(core::Heading::Right);
    ok &= Check(Near(leftFacing.startDegrees, 90.0) &&
                Near(leftFacing.endDegrees, 270.0),
                "left-to-right reversal has a leftward bulge and vertical diameter");
    const app::ReverseCapArc upFacing =
        app::ReverseCapForOutgoing(core::Heading::Down);
    const app::ReverseCapArc downFacing =
        app::ReverseCapForOutgoing(core::Heading::Up);
    ok &= Check(Near(upFacing.startDegrees, -180.0) &&
                Near(upFacing.endDegrees, 0.0) &&
                Near(downFacing.startDegrees, 0.0) &&
                Near(downFacing.endDegrees, 180.0),
                "vertical reversal has a horizontal diameter");

    // In ALL eight compass headings the half-disk faces exactly backward
    // along the incoming route; both ends of its straight diameter must
    // be perpendicular to the authored trajectory.
    constexpr std::array<core::Heading, 8> headings{
        core::Heading::Up, core::Heading::UpRight, core::Heading::Right,
        core::Heading::DownRight, core::Heading::Down,
        core::Heading::DownLeft, core::Heading::Left, core::Heading::UpLeft};
    constexpr double radians = 0.017453292519943295;
    for (const auto outgoing : headings) {
        const auto arc = app::ReverseCapForOutgoing(outgoing);
        const unsigned mask = core::HeadingArrowMask(outgoing);
        double dx = static_cast<double>((mask & 2u) != 0u) -
                    static_cast<double>((mask & 8u) != 0u);
        double dy = static_cast<double>((mask & 4u) != 0u) -
                    static_cast<double>((mask & 1u) != 0u);
        const double magnitude = std::hypot(dx, dy);
        dx /= magnitude;
        dy /= magnitude;

        const double center = (arc.startDegrees + arc.endDegrees) *
            0.5 * radians;
        const double start = arc.startDegrees * radians;
        const double finish = arc.endDegrees * radians;
        ok &= Check(Near(arc.endDegrees - arc.startDegrees, 180.0) &&
                    Near(dx * std::cos(center) + dy * std::sin(center), -1.0) &&
                    Near(dx * std::cos(start) + dy * std::sin(start), 0.0) &&
                    Near(dx * std::cos(finish) + dy * std::sin(finish), 0.0),
                    "every cap bulges opposite outgoing motion; diameter is perpendicular");
    }
    ok &= Check(app::NeedsReverseCap(core::Turn::Reverse) &&
                !app::NeedsReverseCap(core::Turn::Left) &&
                !app::NeedsReverseCap(core::Turn::Right) &&
                !app::NeedsReverseCap(core::Turn::Left45) &&
                !app::NeedsReverseCap(core::Turn::Right45) &&
                !app::NeedsReverseCap(core::Turn::Left135) &&
                !app::NeedsReverseCap(core::Turn::Right135) &&
                app::kReverseCapRadiusScale > 1.0f,
                "no cap for ordinary turns and reverse cap protrudes beyond the dot");

    if (!ok) return 1;
    std::cout << "REVERSE_CAP_PASS directions=8 normal-junctions=unchanged\n";
    return 0;
}
