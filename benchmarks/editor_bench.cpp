#include "core/chart_editor.h"
#include "core/tempo_map.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
using namespace locus::core;
using Clock=std::chrono::steady_clock;

struct Samples {
    std::vector<double> ms;
    std::uint64_t checksum=0;
    template<class Operation>
    bool Measure(Operation&& operation) {
        const auto start=Clock::now();
        const auto result=operation();
        const auto end=Clock::now();
        if (!result) return false;
        ms.push_back(std::chrono::duration<double,std::milli>(end-start).count());
        checksum+=result;
        return true;
    }
    void Print(const char* scenario) {
        std::sort(ms.begin(),ms.end());
        const auto percentile=[&](std::size_t p) {
            return ms[(ms.size()*p+99)/100-1];
        };
        std::cout<<std::fixed<<std::setprecision(6)
            <<"EDITOR_BENCH "<<scenario<<" samples="<<ms.size()
            <<" p50_ms="<<percentile(50)<<" p95_ms="<<percentile(95)
            <<" p99_ms="<<percentile(99)<<" checksum="<<checksum<<'\n';
    }
};

bool Run() {
    // 256 aligned markers with varying tempos. A 20+ minute chart
    // intentionally stresses the full editor model rather than the GPU.
    constexpr int halfNotes=32768;
    LoadedChart source;
    source.title="Editor benchmark";
    source.route.bpm=120;
    source.route.ticksPerBeat=24;
    source.route.snapDivision=4;
    source.route.lengthBeats=150000;
    for (int i=0;i<256;++i)
        source.route.tempoMap.push_back({i*6000LL,120.0+30.0*(i%3)});
    ChartEditor::Pattern pattern;
    pattern.span=halfNotes*2-1;
    pattern.ticksPerBeat=24;
    source.route.turns.reserve(halfNotes);
    pattern.notes.reserve(halfNotes);
    for (int i=0;i<halfNotes;++i) {
        const int tick=2+2*i;
        const auto ms=static_cast<std::int64_t>(std::llround(
            TempoSecondsAtTick(source.route,0,tick)*1000.0));
        if (TempoTickAtMs(source.route,0,ms)!=tick ||
            TempoTickAtMs(source.route,0,ms+4)!=tick) return false;
        source.route.turns.push_back({tick,Heading::Right,ms});
        pattern.notes.push_back({i*2,Heading::Up,4});
    }
    ChartEditor editor;
    editor.Load(source);
    if (!editor.SetSongDuration(2000)) return false;
    Samples paste,undo,redo,idle,seek,tempo;
    for (int i=0;i<24;++i) {
        if (!paste.Measure([&]() -> std::uint64_t {
            return editor.PasteTurns(2,pattern)
                ? editor.Data().route.turns.size():0;
        }) || editor.Data().route.turns.size()!=2*halfNotes ||
            !undo.Measure([&]() -> std::uint64_t {
                return editor.Undo()?editor.Data().route.turns.size():0;
            }) || editor.Data().route.turns.size()!=halfNotes ||
            !redo.Measure([&]() -> std::uint64_t {
                return editor.Redo()?editor.Data().route.turns.size():0;
            }) || editor.Data().route.turns.size()!=2*halfNotes) return false;
        if (i+1<24 && !editor.Undo()) return false;
    }
    // Cold rating is not part of the cached idle measurement.
    const auto count=editor.Difficulty().notes;
    if (count!=2*halfNotes) return false;
    const auto revision=editor.RouteRevision();
    for(int i=0;i<2048;++i) {
        if (!idle.Measure([&]() -> std::uint64_t {
            return editor.Difficulty().notes;
        }) || !seek.Measure([&]() -> std::uint64_t {
            const int index=(i*193)%halfNotes;
            const auto& note=editor.Data().route.turns[2*index+1];
            if (!editor.SelectTurnAtTimeMs(note.timeMs)) return 0;
            const auto* selected=editor.SelectedTurn();
            if (!selected || selected->timeMs!=note.timeMs) return 0;
            return static_cast<std::uint64_t>(std::llround(
                editor.AudioSecondsAtBeat(selected->beat)*1000.0))+1;
        })) return false;
    }
    if (editor.RouteRevision()!=revision) return false;
    // Shared-tempo projection on the full 65,536-note chart. Undo between
    // samples to keep the same authored timestamps and history scope.
    auto markers=source.route.tempoMap;
    markers[128].bpm+=1.0;
    for(int i=0;i<12;++i) {
        if (!tempo.Measure([&]() -> std::uint64_t {
            return editor.SetTempoMap(markers)?editor.Data().route.turns.size():0;
        })) return false;
        const auto& turns=editor.Data().route.turns;
        if (turns.size()!=2*halfNotes) return false;
        for(int n=0;n<halfNotes;++n)
            if (turns[2*n].timeMs!=source.route.turns[n].timeMs ||
                turns[2*n+1].timeMs!=source.route.turns[n].timeMs+4)
                return false;
        if (!editor.Undo() || editor.Data().route.tempoMap!=source.route.tempoMap)
            return false;
    }
    paste.Print("paste");
    undo.Print("undo");
    redo.Print("redo");
    idle.Print("cached_idle");
    seek.Print("seek_exact_note");
    tempo.Print("tempo_reprojection");
    return true;
}
} // namespace

int main() {
    if (!Run()) {
        std::cerr<<"EDITOR_BENCH_FAIL invariant failed\n";
        return 1;
    }
    std::cout<<"EDITOR_BENCH_PASS notes=65536 markers=256\n";
}
