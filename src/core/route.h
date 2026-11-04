#pragma once

#include <vector>

namespace locus::core {

enum class Heading { Up, Right, Down, Left };
enum class Turn { Left, Right };

struct Cell {
    int x = 0;
    int y = 0;
};

struct TurnEvent {
    int beat = 0;
    Turn direction = Turn::Left;
};

struct Chart {
    double bpm = 120.0;
    Cell start{0, 0};
    Heading initialHeading = Heading::Right;
    int lengthBeats = 32;
    std::vector<TurnEvent> turns;
};

struct RouteStep {
    Cell from;
    Cell to;
    Heading heading;
    bool wraps = false;
};

// A fixed, rights-clean gameplay chart for the first playable prototype.
// The original 120 BPM click track is generated in src/core/demo_audio.cpp.
Chart MakeDemoChart();

// Pure deterministic route: apply a charted turn at the beat boundary,
// then move one point in the new heading, wrapping at each grid edge.
// Input/judgements never influence the result.
// Throws std::out_of_range for beats outside [0, chart.lengthBeats).
RouteStep SampleRouteStep(const Chart& chart, int beat);

} // namespace locus::core
