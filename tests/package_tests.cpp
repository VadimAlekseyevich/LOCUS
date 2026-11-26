#include "app/package_validation.h"
#include "app/publication_revision.h"
#include "app/song_archive.h"
#include "app/song_timing.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs=std::filesystem;
using namespace locus;
namespace {
bool Check(bool ok,const char* message,const std::string& error) {
    if (!ok) std::cerr<<"PACKAGE_TEST_FAIL: "<<message<<": "<<error<<'\n';
    return ok;
}
bool Write(const fs::path& path,const std::string& contents) {
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    stream.write(contents.data(),static_cast<std::streamsize>(contents.size()));
    stream.close();
    return static_cast<bool>(stream);
}
std::string Read(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
struct Cleanup {
    fs::path root;
    ~Cleanup() { std::error_code ec; fs::remove_all(root,ec); }
};
}
int main() {
    const auto root=fs::temp_directory_path()/("locus-package-test-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!fs::create_directory(root)) return 1;
    Cleanup cleanup{root};
    const auto source=root/"source", stage=root/"stage";
    fs::create_directory(source);
    fs::create_directory(stage);
    app::SongSetManifest manifest;
    manifest.id="test-set"; manifest.title="Portable"; manifest.artist="Test";
    manifest.audioFile="music.wav"; manifest.bpm=120.0; manifest.offsetMs=0;
    manifest.tempoMap={{0,120.0},{1500,180.0}};
    core::LoadedChart chart;
    chart.title="Hard"; chart.difficultyId="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    chart.audioPath=manifest.audioFile;
    chart.route.turns={{2,core::Heading::UpRight,1000},{4,core::Heading::Left,2000}};
    chart.route.tempoMap={{0,120.0}};
    std::string manifestBytes, chartBytes, error;
    const auto chartFile=source/"hard.locus.json";
    const auto archive=root/"test.locuspack";
    // Audio is intentionally a stand-in: structural validation must not
    // claim to decode media or prove playable duration.
    bool ok=Check(app::EncodeSongSetManifest(manifest,manifestBytes,error) &&
        core::SerializeChartJson(chart,chartBytes,error) &&
        Write(source/"set.locus.json",manifestBytes) &&
        Write(source/"music.wav","test audio payload") &&
        Write(chartFile,chartBytes) &&
        app::ExportSongSetArchive(source,{chartFile},archive,error) &&
        app::ExtractSongSetArchive(archive,stage,error),"portable archive round trip",error);
    app::ValidatedPackage package;
    ok &= Check(app::ValidateStagedPackage(stage,package,error) &&
        package.manifest.tempoMap==manifest.tempoMap &&
        package.difficulties.size()==1 &&
        package.difficulties.count(chart.difficultyId)==1,
        "same manifest and chart validator on both hosts",error);
    if (!ok) return 1;
    auto effective=package.difficulties.at(chart.difficultyId).chart;
    ok &= Check(app::ApplySongTiming(effective,package.manifest.tempoMap,error) &&
        effective.route.turns[1].timeMs==2000 &&
        effective.route.tempoMap==manifest.tempoMap,"effective shared timing preserves exact notes",error);
    app::PackageRevisionContent original, changed;
    ok &= Check(Write(stage/"music.wav","abc") &&
        app::DescribePackageRevision(stage,original,error) &&
        original.audio.sha256=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" &&
        original.audio.bytes==3,"SHA-256 known vector and exact bytes",error);
    if (!ok) return 1;
    auto revised=chart;
    revised.title="Renamed";
    std::string revisedBytes;
    const auto& originalChart=original.difficulties.at(chart.difficultyId);
    ok &= Check(core::SerializeChartJson(revised,revisedBytes,error) &&
        Write(stage/"hard.locus.json",revisedBytes) &&
        app::DescribePackageRevision(stage,changed,error) &&
        changed.difficulties.at(chart.difficultyId).file.sha256!=originalChart.file.sha256 &&
        changed.difficulties.at(chart.difficultyId).gameplaySha256==originalChart.gameplaySha256,
        "rename changes exact bytes but not gameplay digest",error);
    ok &= Check(Write(stage/"music.wav","abd") &&
        app::DescribePackageRevision(stage,changed,error) &&
        changed.difficulties.at(chart.difficultyId).gameplaySha256!=originalChart.gameplaySha256,
        "audio change invalidates published gameplay content",error);
    revised.route.turns.back().timeMs=2100;
    ok &= Check(Write(stage/"music.wav","abc") &&
        core::SerializeChartJson(revised,revisedBytes,error) &&
        Write(stage/"hard.locus.json",revisedBytes) &&
        app::DescribePackageRevision(stage,changed,error) &&
        changed.difficulties.at(chart.difficultyId).gameplaySha256!=originalChart.gameplaySha256,
        "note edit invalidates published gameplay content",error);
    auto timingChanged=manifest;
    timingChanged.tempoMap.back().bpm=200.0;
    std::string timingBytes;
    ok &= Check(Write(stage/"hard.locus.json",chartBytes) &&
        app::EncodeSongSetManifest(timingChanged,timingBytes,error) &&
        Write(stage/"set.locus.json",timingBytes) &&
        app::DescribePackageRevision(stage,changed,error) &&
        changed.difficulties.at(chart.difficultyId).gameplaySha256!=originalChart.gameplaySha256 &&
        Write(stage/"set.locus.json",manifestBytes),
        "shared timing invalidates publication digest",error);
    ok &= Check(Write(stage/"duplicate.json",chartBytes) &&
        !app::ValidateStagedPackage(stage,package,error) &&
        package.difficulties.size()==1,"duplicate ID rejected without replacing output",error);
    fs::remove(stage/"duplicate.json");
    ok &= Check(Write(stage/"set.locus.json","{\"setId\":\"duplicate\","+manifestBytes.substr(1)) &&
        !app::ValidateStagedPackage(stage,package,error),
        "duplicate manifest keys rejected",error);
    ok &= Check(Write(stage/"set.locus.json",manifestBytes) &&
        Write(stage/"hard.locus.json",std::string(65,'[')+std::string(65,']')) &&
        !app::ValidateStagedPackage(stage,package,error),
        "excessive JSON nesting rejected before DOM parsing",error);
    auto corrupt=Read(archive);
    if (corrupt.size()<31) return 1;
    // The first stored payload follows the 30-byte header and filename.
    const auto payload=30u+std::string("set.locus.json").size();
    if (payload>=corrupt.size()) return 1;
    corrupt[payload]^=1;
    const auto invalid=root/"corrupt.locuspack", invalidStage=root/"bad-stage";
    fs::create_directory(invalidStage);
    ok &= Check(Write(invalid,corrupt) && !app::ExtractSongSetArchive(invalid,invalidStage,error),
        "CRC corruption rejected by production ZIP reader",error);
    return ok?0:1;
}
