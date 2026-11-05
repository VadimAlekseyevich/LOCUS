#include "core/arrow_chord.h"

#include <cmath>
#include <iostream>

using namespace locus::core;

namespace {
bool Check(bool condition, const char* message) {
    if (!condition) std::cerr << "ARROW_CHORD_FAIL " << message << '\n';
    return condition;
}
ArrowKeys Frame(int index, double timestamp,
                std::array<bool, 4> held) {
    ArrowKeys keys;
    keys.held = held;
    if (index >= 0) {
        keys.pressed[index] = true;
        keys.capturedAt[index] = timestamp;
    }
    return keys;
}
} // namespace

int main() {
    bool ok = true;
    ok &= Check(HeadingFromArrowMask(1u) == Heading::Up &&
                HeadingFromArrowMask(2u) == Heading::Right &&
                HeadingFromArrowMask(3u) == Heading::UpRight &&
                HeadingFromArrowMask(6u) == Heading::DownRight &&
                HeadingFromArrowMask(12u) == Heading::DownLeft &&
                HeadingFromArrowMask(9u) == Heading::UpLeft &&
                HeadingArrowMask(Heading::UpLeft) == 9u,
                "single arrows and four diagonals decode correctly");
    ok &= Check(!HeadingFromArrowMask(0u) &&
                !HeadingFromArrowMask(5u) &&  // Up + Down
                !HeadingFromArrowMask(10u) && // Left + Right
                !HeadingFromArrowMask(7u),
                "opposite arrows and three-key chords are invalid");

    ArrowChordTracker tracker;
    const auto first = tracker.Observe(
        Frame(0, 1.0, {true, false, false, false}),
        Heading::UpRight, Heading::Right);
    ok &= Check(!first, "first half of a diagonal does not judge");
    const auto second = tracker.Observe(
        Frame(1, 1.045, {true, true, false, false}),
        Heading::UpRight, Heading::Right);
    ok &= Check(second && second->outgoing == Heading::UpRight &&
                std::abs(second->capturedAt - 1.045) < 1e-9,
                "second key within 45ms completes one diagonal stroke");
    ok &= Check(!tracker.Observe(
        Frame(-1, 1.07, {true, true, false, false}),
        Heading::UpRight, Heading::Right),
        "holding two arrows does not generate another score");
    tracker.Reset();

    ArrowKeys together;
    together.pressed = {true, true, false, false};
    together.held = together.pressed;
    together.capturedAt = {2.0, 2.002, 0.0, 0.0};
    const auto both = tracker.Observe(together, Heading::UpRight, Heading::Down);
    ok &= Check(both && both->outgoing == Heading::UpRight,
                "same-frame UP+RIGHT produces exactly one diagonal note");

    tracker.Reset();
    tracker.Observe(Frame(0, 3.0, {true, false, false, false}),
                    Heading::UpRight, Heading::Right);
    const auto tooLate = tracker.Observe(
        Frame(1, 3.2, {true, true, false, false}),
        Heading::UpRight, Heading::Right);
    ok &= Check(!tooLate, "widely separated keydowns do not count as simultaneous");

    tracker.Reset();
    const auto cardinal = tracker.Observe(
        Frame(0, 4.0, {true, false, false, false}),
        Heading::Up, Heading::Right);
    ok &= Check(cardinal && cardinal->outgoing == Heading::Up,
                "classic cardinal input still judges immediately");

    tracker.Reset();
    ArrowKeys wrongCombo;
    wrongCombo.pressed = {true, false, true, false};
    wrongCombo.held = wrongCombo.pressed;
    wrongCombo.capturedAt = {5.0, 0.0, 5.0, 0.0};
    const auto wrong = tracker.Observe(wrongCombo, Heading::UpRight, Heading::Right);
    ok &= Check(wrong && wrong->outgoing != Heading::UpRight,
                "opposite keys cannot grant a correct diagonal");

    tracker.Reset();
    tracker.Observe(Frame(0, 6.0, {true, false, false, false}),
                    Heading::UpRight, Heading::Right);
    tracker.Reset();
    ok &= Check(!tracker.Observe(
        Frame(1, 6.04, {true, true, false, false}),
        Heading::UpRight, Heading::Right),
        "pause/restart reset prevents stale held key from completing chord");

    if (!ok) return 1;
    std::cout << "ARROW_CHORD_PASS 4 diagonals single score and timing tolerance\n";
    return 0;
}
