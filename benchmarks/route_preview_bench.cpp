#include "app/route_preview.h"
#include "app/path_mesh.h"

#include <algorithm>
#include <cmath>
#include <vector>

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
                preview.turnsFarToNear.size();
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

    // Same production builder, no GL/window/audio. Each timing includes
    // snapshot preparation, mesh rows and both outline/foreground quads.
    // Reuse one workspace sequentially, like RenderFrame.
    locus::app::PathRibbonWorkspace workspace;
    locus::app::Skin skin;
    skin.lookaheadMs=800.0f;
    skin.fadePower=1.7f;
    std::vector<double> samples;
    samples.reserve(kIterations);
    std::size_t maxRows=0,growths=0;
    std::uint64_t geometryChecksum=0;
    const auto geometryFrame=[&](int frame,bool measured) {
        const int noteIndex=1+(frame*193)%(notes-2);
        const double seconds=static_cast<double>(noteIndex)/notesPerSecond+0.007;
        const auto sampleCapacity=workspace.samples.capacity();
        const auto rowCapacity=workspace.rows.capacity();
        const auto frameStart=Clock::now();
        for(int pane=0;pane<panes;++pane) {
            auto& preview=frames[pane];
            locus::app::PrepareRoutePreview(route,seconds,skin.lookaheadMs,preview);
            const locus::app::RouteCanvas canvas{480.0f,270.0f,100.0f,preview.marker};
            locus::app::PrepareScoredPathRibbon(preview,canvas,skin,workspace);
            maxRows=std::max(maxRows,workspace.rows.size());
            for(std::size_t i=1;i<workspace.rows.size();++i) {
                const auto& a=workspace.rows[i-1];
                const auto& b=workspace.rows[i];
                if(b.startsRun || (a.opacity<=0 && b.opacity<=0)) continue;
                for(float scale:{skin.outlineScale,1.0f}) {
                    const auto quad=locus::app::MakePathRibbonQuad(a,b,scale);
                    // Consume coordinates rather than letting an optimizing
                    // compiler eliminate geometry that would go to the GPU.
                    for(const auto& v:quad.vertices) {
                        if(!std::isfinite(v.x)||!std::isfinite(v.y)) std::exit(1);
                        if(measured) geometryChecksum+=static_cast<std::uint64_t>(
                            std::llround(std::abs(v.x)*16+std::abs(v.y)*16));
                    }
                }
            }
        }
        const auto frameEnd=Clock::now();
        if(measured) {
            samples.push_back(std::chrono::duration<double,std::micro>(
                frameEnd-frameStart).count());
            growths+=(sampleCapacity!=workspace.samples.capacity() ||
                      rowCapacity!=workspace.rows.capacity())?1:0;
        }
    };
    // Warm the exact same seek sequence to establish a high-water capacity,
    // then report allocation-capacity changes over the measured sequence.
    for(int i=0;i<kIterations;++i) geometryFrame(i,false);
    for(int i=0;i<kIterations;++i) geometryFrame(i,true);
    if(growths || !geometryChecksum || maxRows<2) {
        std::cerr<<"RIBBON_PROFILE_ERROR capacity or geometry invariant\n";
        std::exit(1);
    }
    std::sort(samples.begin(),samples.end());
    const auto percentile=[&](std::size_t p) {
        return samples[(samples.size()*p+99)/100-1];
    };
    std::cout<<"RIBBON_PROFILE notes="<<notes<<" nps="<<notesPerSecond
             <<" panes="<<panes<<" frames="<<samples.size()
             <<" p50_us="<<percentile(50)<<" p95_us="<<percentile(95)
             <<" p99_us="<<percentile(99)<<" max_rows="<<maxRows
             <<" buffer_growth_frames="<<growths
             <<" checksum="<<geometryChecksum<<'\n';
}

} // namespace

int main() {
    RunScenario(1024, 4, 1);
    RunScenario(1024, 20, 4);
    RunScenario(60000, 4, 1);
    RunScenario(60000, 20, 4);
}
