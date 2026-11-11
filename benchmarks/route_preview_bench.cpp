#include "app/route_preview.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace {

using Clock = std::chrono::steady_clock;

// Synthesize a 1:1 musical-grid pattern at the stated notes/second. No GPU,
// font, window or audio device: this measures only the route snapshot builder.
void RunScenario(int notes, int notesPerSecond, int panes) {
    locus::core::Chart chart;
    chart.bpm = notesPerSecond * 60.0;
    chart.ticksPerBeat = 1;
    chart.firstBeat = 0;
    chart.lengthBeats = notes + 32;
    chart.start = {0, 0};
    chart.initialHeading = locus::core::Heading::Right;
    chart.turns.reserve(static_cast<std::size_t>(notes));
    for (int i = 1; i <= notes; ++i) {
        const auto heading = (i % 6) == 0
            ? locus::core::Heading::Left
            : (i % 4) == 0
                ? locus::core::Heading::Up
                : locus::core::Heading::Right;
        chart.turns.push_back({
            i, heading,
            static_cast<std::int64_t>(i) * 1000 / notesPerSecond});
    }

    locus::core::TimedRoute route;
    const auto compileStart = Clock::now();
    if (!route.Compile(chart, 0.0)) {
        std::cerr << "PREVIEW_BENCH_ERROR compile failed\n";
        std::exit(1);
    }
    const auto compileEnd = Clock::now();

    locus::app::RoutePreviewFrame frames[4];
    constexpr int kIterations = 1200;
    std::uint64_t checksum = 0;
    const auto start = Clock::now();
    for (int frame = 0; frame < kIterations; ++frame) {
        // Deterministic nonsequential seeks stress O(log chart size) lookup,
        // not just the adjacent-frame fast path.
        const int noteIndex = 1 + (frame * 193) % (notes - 2);
        const double songSeconds =
            static_cast<double>(noteIndex) / notesPerSecond + 0.007;
        for (int pane = 0; pane < panes; ++pane) {
            auto& preview = frames[pane];
            locus::app::PrepareRoutePreview(
                route, songSeconds, 800.0f, preview);
            checksum += preview.spans.size() +
                preview.turnsFarToNear.size() +
                preview.turnSpacingWorld.size();
        }
    }
    const auto end = Clock::now();
    const double compileMs =
        std::chrono::duration<double, std::milli>(
            compileEnd - compileStart).count();
    const double frameUs =
        std::chrono::duration<double, std::micro>(end - start).count() /
        kIterations;
    std::cout << std::fixed << std::setprecision(3)
              << "PREVIEW_BENCH notes=" << notes
              << " nps=" << notesPerSecond
              << " panes=" << panes
              << " compile_ms=" << compileMs
              << " preview_us_per_frame=" << frameUs
              << " checksum=" << checksum << '\n';
}

} // namespace

int main() {
    RunScenario(1024, 4, 1);
    RunScenario(1024, 20, 4);
    RunScenario(60000, 4, 1);
    RunScenario(60000, 20, 4);
}
