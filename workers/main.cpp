#include "verification_job.h"
#include "app/song_archive.h"
#include "core/chart_json.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using Json=nlohmann::json;
using namespace locus;
namespace {
constexpr int kProtocolVersion=1;
template<class T> bool Number(const std::string& s,T& value) {
    const auto parsed=std::from_chars(s.data(),s.data()+s.size(),value);
    return parsed.ec==std::errc{} && parsed.ptr==s.data()+s.size();
}
int Output(Json value,int code) {
    value["protocolVersion"]=kProtocolVersion;
    std::cout<<value.dump(-1,' ',false,Json::error_handler_t::replace)<<'\n';
    return code;
}
int Reject(const std::string& error) { return Output({{"ok",false},{"error",error}},2); }
Json Digest(const app::ContentDigest& d) { return {{"bytes",d.bytes},{"sha256",d.sha256}}; }
bool Regular(const fs::path& p) {
    std::error_code ec;
    return fs::symlink_status(p,ec).type()==fs::file_type::regular && !ec;
}
bool ReplayBytes(const fs::path& path,std::string& bytes) {
    if (!Regular(path)) return false;
    std::error_code ec;
    const auto n=fs::file_size(path,ec);
    if (ec || n<core::kReplayHeaderBytes || n>core::kMaxReplayBytes) return false;
    std::ifstream in(path,std::ios::binary);
    bytes.resize(static_cast<std::size_t>(n));
    return static_cast<bool>(in.read(bytes.data(),static_cast<std::streamsize>(n))) &&
        in.peek()==std::char_traits<char>::eof() && !in.bad();
}
Json InspectStage(const fs::path& stage, std::string& error,
                  std::int64_t frames=0, int sampleRate=0) {
        app::PackageRevisionContent revision;
        app::ValidatedPackage package;
        if (!app::DescribePackageRevision(stage,revision,error,&package)) return nullptr;
        Json charts=Json::array();
        for (const auto& entry : package.difficulties) {
            const auto& chart=entry.second.chart;
            if (sampleRate) {
                auto playable=chart;
                // Validate against both actual sample duration and the ms
                // representation used by the current score job.
                const auto ms=(frames*1000+sampleRate/2)/sampleRate;
                const auto duration=std::min(static_cast<double>(frames)/sampleRate, ms/1000.0);
                if (!core::PrepareChartForSong(playable,duration,error)) return nullptr;
            }
            const auto& digest=revision.difficulties.at(entry.first);
            const auto rating=core::CalculateChartDifficulty(chart.route,chart.offsetSeconds);
            charts.push_back({{"difficultyId",entry.first},{"title",chart.title},
                {"content",Digest(digest.file)},{"gameplaySha256",digest.gameplaySha256},
                {"stars",rating.overall},{"keyPresses",rating.keyPresses}});
        }
        Json art=nullptr;
        if (revision.background) art=Digest(*revision.background);
        return {{"protocolVersion",kProtocolVersion},{"ok",true},{"kind","packageInspection"},{"mediaValidated",false},
            {"setId",package.manifest.id},{"title",package.manifest.title},
            {"artist",package.manifest.artist},{"manifest",Digest(revision.manifest)},
            {"audio",Digest(revision.audio)},{"background",art},
            {"difficultyVersion",core::kChartDifficultyVersion},{"difficulties",charts}};
}
int Run(const std::vector<std::string>& args) {
    std::string error;
    if (args.size()==4 && args[1]=="inspect") {
        const auto archive=fs::u8path(args[2]), stage=fs::u8path(args[3]);
        std::error_code ec;
        if (!Regular(archive) || fs::symlink_status(stage,ec).type()!=fs::file_type::directory || ec)
            return Reject("Archive or private staging path is invalid.");
        if (!app::ExtractSongSetArchive(archive,stage,error)) return Reject(error);
        auto result=InspectStage(stage,error);
        if (result.is_null()) return Reject(error);
        return Output(std::move(result),0);
    }
    if (args.size()==5 && args[1]=="validate-timing") {
        std::int64_t frames=0;
        int rate=0;
        if (!Number(args[3],frames) || !Number(args[4],rate) ||
            rate<8000 || rate>192000 || frames<=0 || frames>86400LL*rate)
            return Reject("Invalid decoded sample duration.");
        auto inspection=InspectStage(fs::u8path(args[2]),error,frames,rate);
        if (inspection.is_null()) return Reject(error);
        return Output({{"ok",true},{"kind","timingValidation"},{"sampleFrames",frames},
            {"sampleRate",rate},{"inspection",std::move(inspection)}},0);
    }
    if (args.size()==10 && args[1]=="verify") {
        worker::ScoreJob job;
        job.difficultyId=args[3]; job.gameplaySha256=args[4];
        if (!Number(args[5],job.audioDurationMs) || !Number(args[6],job.ratePercent) ||
            !Number(args[7],job.modFlags) || !Number(args[8],job.modSeed))
            return Reject("Invalid numeric job parameter.");
        std::string bytes;
        if (!ReplayBytes(fs::u8path(args[9]),bytes)) return Reject("Cannot read bounded replay file.");
        worker::ScoreEvaluation result;
        if (!worker::EvaluateScoreJob(fs::u8path(args[2]),job,bytes,result,error)) return Reject(error);
        const auto& g=result.score.grades;
        const auto& s=result.score.statistics;
        Json histogram=nullptr;
        if (s.histogram) histogram=*s.histogram;
        return Output({{"ok",true},{"kind","scoreEvaluation"},{"rankApproved",false},
            {"difficultyId",job.difficultyId},{"gameplaySha256",job.gameplaySha256},
            {"ratePercent",job.ratePercent},{"modFlags",job.modFlags},{"modSeed",job.modSeed},
            {"judgementVersion",core::kJudgementRulesVersion},
            {"difficultyVersion",result.difficulty.difficultyVersion},
            {"performanceVersion",core::kPerformanceVersion},{"stars",result.difficulty.stars},
            {"keyPresses",result.difficulty.keyPresses},{"pp",result.pp},
            {"grades",{{"perfect",g.perfect},{"good",g.good},{"bad",g.bad},{"miss",g.miss}}},
            {"accuracyPercent",g.AccuracyPercent()},{"maxCombo",s.maxCombo},
            {"timedHits",s.timedHits},{"meanErrorMs",s.meanErrorMs},
            {"unstableRate",s.unstableRate},{"histogram",histogram}},0);
    }
    return Output({{"ok",false},{"error","Usage: inspect ARCHIVE EMPTY_STAGE | validate-timing STAGE SAMPLE_FRAMES SAMPLE_RATE | verify STAGE DIFFICULTY_ID GAMEPLAY_SHA256 AUDIO_DURATION_MS RATE_PERCENT MOD_FLAGS MOD_SEED REPLAY"}},64);
}
}
int main(int argc,char** argv) {
    try {
        return Run(std::vector<std::string>(argv,argv+argc));
    } catch (const std::exception&) {
        // Internal exceptions may contain host paths; don't expose them to
        // a future public HTTP response. The orchestrator owns diagnostics.
        return Output({{"ok",false},{"error","Worker execution failed."}},1);
    }
}
