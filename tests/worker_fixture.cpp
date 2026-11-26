#include "app/song_set_manifest.h"
#include "app/song_archive.h"
#include "core/chart_json.h"
#include "core/judgement.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs=std::filesystem;
using namespace locus;
namespace {
bool Write(const fs::path& path,const std::string& bytes) {
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    stream.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    stream.close(); return static_cast<bool>(stream);
}
}
int main(int argc,char** argv) {
    if (argc!=2) return 1;
    const auto root=fs::u8path(argv[1]), source=root/"source";
    fs::create_directory(source);
    app::SongSetManifest manifest;
    manifest.id="worker-fixture"; manifest.title="Test"; manifest.artist="Fixture";
    manifest.audioFile="music.wav"; manifest.bpm=120.0; manifest.offsetMs=0;
    manifest.tempoMap={{0,120.0}};
    core::LoadedChart chart;
    chart.title="Hard"; chart.difficultyId="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    chart.audioPath=manifest.audioFile; chart.route.tempoMap=manifest.tempoMap;
    chart.route.turns={{2,core::Heading::UpRight,1000},{4,core::Heading::Left,2000}};
    std::string m,c,error,bytes;
    const auto file=source/"hard.json";
    core::ReplayTrace trace;
    trace.judgementVersion=core::kJudgementRulesVersion;
    trace.events={{core::ReplayAction::Up,1.0},{core::ReplayAction::Right,1.0},
                  {core::ReplayAction::Left,2.0}};
    if (!app::EncodeSongSetManifest(manifest,m,error) ||
        !core::SerializeChartJson(chart,c,error) ||
        !Write(source/"set.locus.json",m) ||
        !Write(file,c) || !Write(source/"music.wav","opaque audio fixture") ||
        !app::ExportSongSetArchive(source,{file},root/"fixture.locuspack",error) ||
        !core::EncodeReplay(trace,bytes) || !Write(root/"valid.replay",bytes)) {
        std::cerr<<error<<'\n'; return 1;
    }
    trace.offsetSeconds=0.125;
    if (!core::EncodeReplay(trace,bytes) || !Write(root/"offset.replay",bytes)) return 1;
    trace.offsetSeconds=0;
    trace.events.back()={core::ReplayAction::Finish,0};
    if (!core::EncodeReplay(trace,bytes) || !Write(root/"finish.replay",bytes)) return 1;
    trace.events.back()={core::ReplayAction::Advance,10000};
    if (!core::EncodeReplay(trace,bytes) || !Write(root/"late.replay",bytes)) return 1;
    trace.events.pop_back();
    if (!core::EncodeReplay(trace,bytes) || !Write(root/"partial.replay",bytes)) return 1;
    return 0;
}
