#include "core/route.h"

#include <stdexcept>

namespace locus::core {
namespace {

Heading ApplyTurn(Heading heading, Turn turn) {
    const int current = static_cast<int>(heading);
    const int rotation = (turn == Turn::Right) ? 1 : 3;
    return static_cast<Heading>((current + rotation) % 4);
}

Cell StepInHeading(Cell cell, Heading heading,
                   int width, int height, bool& wraps) {
    int x = cell.x;
    int y = cell.y;
    switch (heading) {
        case Heading::Up: --y; break;
        case Heading::Right: ++x; break;
        case Heading::Down: ++y; break;
        case Heading::Left: --x; break;
    }
    wraps = (x < 0 || x >= width || y < 0 || y >= height);
    return Cell{(x + width) % width, (y + height) % height};
}

} // namespace

std::optional<Turn> ArrowToTurn(Heading incoming, Heading pressed) noexcept {
    const int delta = (static_cast<int>(pressed) - static_cast<int>(incoming) + 4) % 4;
    if (delta == 1) return Turn::Right;
    if (delta == 3) return Turn::Left;
    return std::nullopt; // Straight ahead / reverse are forbidden.
}

Heading IncomingHeadingAt(const Chart& chart, int beat) {
    if (beat < chart.firstBeat || beat >= chart.lengthBeats)
        throw std::out_of_range("turn beat outside playable range");
    return beat == chart.firstBeat ? chart.initialHeading
                                  : SampleRouteStep(chart, beat - 1).heading;
}

std::vector<Cell> ReachedTurnCells(const Chart& chart, int inclusiveBeat) {
    std::vector<Cell> cells;
    if (chart.gridWidth < kMinGridSize || chart.gridWidth > kMaxGridSize ||
        chart.gridHeight < kMinGridSize || chart.gridHeight > kMaxGridSize ||
        inclusiveBeat < chart.firstBeat) return cells;
    const int finalBeat = std::min(inclusiveBeat, chart.lengthBeats - 1);
    std::vector<unsigned char> marked(chart.gridWidth * chart.gridHeight, 0);
    Cell cell = chart.start;
    Heading heading = chart.initialHeading;
    std::size_t nextTurn = 0;
    for (int beat = chart.firstBeat; beat <= finalBeat; ++beat) {
        if (nextTurn < chart.turns.size() && chart.turns[nextTurn].beat == beat) {
            const int index = cell.y * chart.gridWidth + cell.x;
            if (index >= 0 && index < static_cast<int>(marked.size()) &&
                !marked[index]) {
                marked[index] = 1;
                cells.push_back(cell);
            }
            heading = ApplyTurn(heading, chart.turns[nextTurn].direction);
            ++nextTurn;
        }
        bool wraps = false;
        cell = StepInHeading(cell, heading, chart.gridWidth, chart.gridHeight, wraps);
    }
    return cells;
}

Chart MakeDemoChart() {
    Chart chart;
    chart.bpm = 120.0;
    chart.start = Cell{1, 1};
    chart.initialHeading = Heading::Right;
    chart.lengthBeats = 32;
    chart.turns = {
        {2, Turn::Right}, {5, Turn::Left},  {8, Turn::Left},
        {12, Turn::Right}, {16, Turn::Right}, {19, Turn::Right},
        {23, Turn::Left}, {27, Turn::Left}, {30, Turn::Left}
    };
    return chart;
}

RouteStep SampleRouteStep(const Chart& chart, int beat) {
    if (beat < chart.firstBeat || beat >= chart.lengthBeats) {
        throw std::out_of_range("route beat is outside chart duration");
    }
    if (chart.gridWidth < kMinGridSize || chart.gridWidth > kMaxGridSize ||
        chart.gridHeight < kMinGridSize || chart.gridHeight > kMaxGridSize ||
        chart.start.x < 0 || chart.start.x >= chart.gridWidth ||
        chart.start.y < 0 || chart.start.y >= chart.gridHeight) {
        throw std::invalid_argument("route grid or starting cell is invalid");
    }

    Cell cell = chart.start;
    Heading heading = chart.initialHeading;
    std::size_t nextTurn = 0;
    for (int n = chart.firstBeat; n <= beat; ++n) {
        if (nextTurn < chart.turns.size() && chart.turns[nextTurn].beat == n) {
            heading = ApplyTurn(heading, chart.turns[nextTurn].direction);
            ++nextTurn;
        }
        bool wraps = false;
        const Cell destination = StepInHeading(
            cell, heading, chart.gridWidth, chart.gridHeight, wraps);
        if (n == beat) {
            return RouteStep{cell, destination, heading, wraps,
                             chart.gridWidth, chart.gridHeight};
        }
        cell = destination;
    }

    // Unreachable when the chart duration and requested beat are valid.
    throw std::logic_error("route iteration unexpectedly ended");
}

} // namespace locus::core
