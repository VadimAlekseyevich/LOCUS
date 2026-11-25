#include "app/song_library.h"
#include "core/chart_revision.h"
#include "app/song_set_storage.h"
#include "app/atomic_move.h"
#include "app/chart_file.h"
#include "app/song_archive.h"
#include "app/song_library_internal.h"
#include "app/song_difficulty_import.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <set>
#include <stdexcept>
#include <iomanip>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

using namespace locus::app;

namespace {
namespace fs = std::filesystem;

bool Check(bool condition, const char* label) {
    if (!condition) std::cerr << "SONG_SET_FAIL " << label << '\n';
    return condition;
}
std::string Read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}
void Write(const fs::path& file, const std::string& data) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << data;
}
bool WriteChart(const fs::path& file, const std::string& name) {
    const std::string json =
        "{"
        "\"formatVersion\":4,"
        "\"title\":\"" + name + "\","
        "\"audioPath\":\"song.wav\","
        "\"bpm\":120,\"offsetMs\":0,\"tempoMap\":[{\"timeMs\":0,\"bpm\":120}],"
        "\"ticksPerBeat\":1,\"snapDivision\":1,"
        "\"grid\":{\"width\":8,\"height\":8},"
        "\"start\":{\"x\":3,\"y\":3,\"heading\":\"r\"},"
        "\"previewLeadBeats\":4,"
        "\"turns\":[{\"timeMs\":1000,\"heading\":\"u\"}]"
        "}";
    Write(file, json);
    return fs::exists(file) && Read(file) == json;
}
locus::core::LoadedChart LoadPayload(const LibraryChart& chart) {
    locus::core::LoadedChart loaded;
    std::string error;
    if (!chart.CopyPayload(loaded,error))
        throw std::runtime_error("Test payload: "+error);
    return loaded;
}
bool HasConsistentCatalogMetadata(const SongLibrary& library) {
    for (const auto& song : library.Songs()) {
        for (const auto& chart : song.charts) {
            const auto loaded=LoadPayload(chart);
            if (chart.metadata.title != loaded.title ||
                chart.metadata.difficultyId != loaded.difficultyId ||
                chart.metadata.noteCount != loaded.route.turns.size())
                return false;
        }
    }
    return true;
}
// Separate process from correctness tests: peak working set is a lifetime
// high-water mark. Capture a post-fixture baseline, scan cold/warm, then
// materialize representative selected charts. No GPU or audio decoder.
#ifdef _WIN32
struct ProcessMemory {
    std::uint64_t workingSet=0,peakWorkingSet=0,privateBytes=0;
};
bool ReadMemory(ProcessMemory& result) {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb=sizeof(counters);
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters)))
        return false;
    result={counters.WorkingSetSize,counters.PeakWorkingSetSize,counters.PrivateUsage};
    return true;
}

int DenseCatalogBenchmark(const fs::path& csvPath) {
    using Clock=std::chrono::steady_clock;
    constexpr int count=96,notes=8192,samples=48;
    const auto folder=fs::temp_directory_path()/("locus-dense-bench-"+
        std::to_string(Clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ec; fs::remove_all(path,ec); }
    } cleanup{folder};
    const auto root=folder/"Songs",set=root/"dense";
    std::error_code ec;
    fs::create_directories(set,ec);
    if (ec) return 1;
    Write(set/"song.wav","RIFF-diagnostic-no-decoder");
    SongSetManifest manifest;
    manifest.id="dense-bench";
    manifest.title="Dense benchmark";
    manifest.audioFile="song.wav";
    manifest.bpm=120.0; manifest.offsetMs=0; manifest.tempoMap={{0,120.0}};
    std::string error;
    if (!WriteNewSongSetManifest(set/"set.locus.json",manifest,error)) {
        std::cerr<<"DENSE_BENCH_ERROR "<<error<<'\n'; return 1;
    }
    std::uint64_t sourceBytes=0;
    // Build one serialized pattern, then stream each file. Fixture generation
    // deliberately never accumulates every chart in RAM before the baseline.
    {
        std::string suffix=R"(","audioPath":"song.wav","bpm":120,"offsetMs":0,"tempoMap":[{"timeMs":0,"bpm":120}],"ticksPerBeat":4,"snapDivision":4,"grid":{"width":8,"height":8},"start":{"x":3,"y":3,"heading":"r"},"previewLeadBeats":4,"turns":[)";
        for (int n=0;n<notes;++n) {
            if (n) suffix+=',';
            suffix+="{\"timeMs\":"+std::to_string((n+1)*50)+
                ",\"heading\":\""+((n%4==0)?"u":(n%4==1)?"rd":(n%4==2)?"l":"ru")+"\"}";
        }
        suffix+="]}";
        for (int i=0;i<count;++i) {
            const auto n=std::to_string(i);
            const auto id=std::string(32-n.size(),'0')+n;
            const auto bytes=std::string(R"({"formatVersion":4,"title":"Dense )")+n+
                R"(","difficultyId":")"+id+suffix;
            const auto file=set/("chart-"+n+".locus.json");
            Write(file,bytes);
            const auto size=fs::file_size(file,ec);
            if (ec || size!=bytes.size()) return 1;
            sourceBytes+=size;
        }
    }
    const auto elapsed=[](Clock::time_point start) {
        return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    };
    SongLibrary library(root);
    ProcessMemory baseline{},coldMemory{},warmMemory{},loadedMemory{};
    if (!ReadMemory(baseline)) return 1;
    const auto coldStart=Clock::now();
    if (!library.Refresh(error) || library.ChartCount()!=count ||
        library.LastRefreshStats().parsedCharts!=count ||
        library.LastRefreshStats().peakParsedNotes!=notes) {
        std::cerr<<"DENSE_BENCH_ERROR cold "<<error<<'\n'; return 1;
    }
    const double coldMs=elapsed(coldStart);
    if (!ReadMemory(coldMemory)) return 1;
    const auto coldStats=library.LastRefreshStats();
    const auto warmStart=Clock::now();
    if (!library.Refresh(error) || library.ChartCount()!=count ||
        library.LastRefreshStats().parsedCharts!=0 ||
        library.LastRefreshStats().reusedCharts!=count ||
        library.LastRefreshStats().calculatedDifficulties!=0) {
        std::cerr<<"DENSE_BENCH_ERROR warm "<<error<<'\n'; return 1;
    }
    const double warmMs=elapsed(warmStart);
    if (!ReadMemory(warmMemory)) return 1;
    const auto warmStats=library.LastRefreshStats();
    std::vector<double> times;
    times.reserve(samples);
    std::uint64_t checksum=0;
    for (int i=0;i<samples;++i) {
        const auto& selected=library.Songs()[0].charts[(i*37)%count];
        locus::core::LoadedChart payload;
        const auto start=Clock::now();
        if (!selected.CopyPayload(payload,error) || payload.route.turns.size()!=notes) {
            std::cerr<<"DENSE_BENCH_ERROR materialize "<<error<<'\n';return 1;
        }
        times.push_back(elapsed(start));
        checksum+=payload.route.turns.size();
        if (i==samples-1 && !ReadMemory(loadedMemory)) return 1;
    }
    std::sort(times.begin(),times.end());
    const auto percentile=[&](std::size_t percent) {
        return times[(times.size()*percent+99)/100-1];
    };
    std::ofstream csv(csvPath,std::ios::binary);
    csv<<std::fixed<<std::setprecision(3)
       <<"phase,charts,notes_per_chart,source_bytes,elapsed_ms,parsed,reused,ratings,peak_parsed_notes,working_set_bytes,peak_working_set_bytes,private_bytes,load_samples,p50_ms,p95_ms,p99_ms,checksum\n";
    const auto row=[&](const char* phase,double ms,const LibraryRefreshStats& stats,
                       const ProcessMemory& mem,int n,double p50,double p95,double p99,
                       std::uint64_t sum) {
        csv<<phase<<','<<count<<','<<notes<<','<<sourceBytes<<','<<ms<<','
           <<stats.parsedCharts<<','<<stats.reusedCharts<<','<<stats.calculatedDifficulties<<','
           <<stats.peakParsedNotes<<','<<mem.workingSet<<','<<mem.peakWorkingSet<<','
           <<mem.privateBytes<<','<<n<<','<<p50<<','<<p95<<','<<p99<<','<<sum<<'\n';
    };
    row("baseline",0,{},baseline,0,0,0,0,0);
    row("cold",coldMs,coldStats,coldMemory,0,0,0,0,0);
    row("warm",warmMs,warmStats,warmMemory,0,0,0,0,0);
    row("materialize",0,{},loadedMemory,samples,percentile(50),percentile(95),
        percentile(99),checksum);
    csv.flush();
    if (!csv) return 1;
    std::cout<<"DENSE_BENCH_PASS charts="<<count<<" notes="<<notes
             <<" cold_ms="<<coldMs<<" warm_ms="<<warmMs<<" load_p95_ms="<<percentile(95)
             <<" peak_ws_bytes="<<loadedMemory.peakWorkingSet<<" checksum="<<checksum<<'\n';
    return 0;
}
#endif
} // namespace

int main(int argc,char** argv) {
    if (argc!=1) {
#ifdef _WIN32
        if (argc==3 && std::string(argv[1])=="--dense-benchmark")
            return DenseCatalogBenchmark(fs::u8path(argv[2]));
#endif
        std::cerr<<"Usage: locus_song_library_tests [--dense-benchmark output.csv]\n";
        return 1;
    }
    bool ok = true;
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path folder = fs::temp_directory_path() /
                            ("locus-song-sets-test-" + unique);
    const fs::path source = folder / "sources" / "My Song";
    const fs::path root = folder / "user" / "Songs";
    std::error_code ec;
    fs::create_directories(source, ec);
    if (ec) return 1;
    const fs::path audio = source / "song.wav";
    const std::string firstBytes = "RIFF-song-set-test-audio-v1";
    Write(audio, firstBytes);
    const fs::path easy = source / "easy.locus.json";
    const fs::path hard = source / "hard.locus.json";
    ok &= Check(WriteChart(easy, "Easy") && WriteChart(hard, "Hard"),
                "create two difficulties referencing one audio");

    SongLibrary library(root);
    std::string note, error;
    ok &= Check(library.Refresh(note) && library.Songs().empty() &&
                library.Revision() == 1,
                "empty library can be indexed and increments its revision");
    // A current-format chart stored outside a managed Song Set is not
    // library content. Do not parse or hash orphaned/pre-release folders.
    const fs::path orphan = root / "unmanaged";
    fs::create_directories(orphan, ec);
    Write(orphan / "song.wav", firstBytes);
    WriteChart(orphan / "orphan.locus.json", "Orphan");
    ok &= Check(library.Refresh(note) && library.Songs().empty() &&
                library.Revision() == 2 &&
                library.LastRefreshStats().parsedCharts == 0 &&
                library.LastRefreshStats().reusedCharts == 0,
                "manifestless charts are ignored before expensive parsing");
    fs::remove_all(orphan, ec);

    // Never move an external file to library Trash, including through a
    // junction or a valid-looking but lexically foreign path.
    ok &= Check(!TrashLibraryItem(root, easy, error) &&
                fs::exists(easy) && Read(easy).find("\"title\":\"Easy\"") !=
                    std::string::npos,
                "Trash rejects files outside canonical Songs boundary");

    // A final name may appear after a preflight existence check, including
    // across processes. The publish operation must preserve BOTH paths.
    {
        const fs::path stage = folder / "move-source.tmp";
        const fs::path destination = folder / "move-destination.tmp";
        Write(stage, "new bytes");
        Write(destination, "other writer's bytes");
        std::error_code moveError;
        ok &= Check(!MovePathNoReplace(stage, destination, moveError) &&
                    Read(stage) == "new bytes" &&
                    Read(destination) == "other writer's bytes",
                    "atomic no-replace refuses existing file without data loss");
        fs::remove(destination, ec);
        ok &= Check(MovePathNoReplace(stage, destination, moveError) &&
                    !fs::exists(stage) &&
                    Read(destination) == "new bytes",
                    "atomic no-replace publishes staged file after conflict clears");
        fs::remove(destination, ec);
    }

    // Removing old-format loading must not allow a new import to silently
    // adopt an existing legacy folder or overwrite its contents.
    const fs::path occupied = root /
        (detail::FilenamePart(audio.stem().u8string()) + "-" +
         detail::HashText(fs::canonical(audio)));
    fs::create_directories(occupied / ".locus-versions", ec);
    Write(occupied / "old-data.txt", "must survive");
    fs::path deniedResult;
    locus::core::LoadedChart fresh;
    std::wstring freshAudio;
    const bool preparedFresh = OpenChartWithAudioPath(
        easy.wstring(), fresh, freshAudio, error);
    const bool rejectsAdoption = preparedFresh &&
        !library.Import(easy, deniedResult, error) &&
        !library.CreateDifficulty(audio, fresh, deniedResult, error) &&
        Read(occupied / "old-data.txt") == "must survive" &&
        !fs::exists(occupied / "set.locus.json");
    ok &= Check(rejectsAdoption,
                "current-only import never annexes an occupied manifestless folder");
    fs::remove_all(occupied, ec);

    // An invalid authored difficulty may fail after a fresh Song Set's
    // shared audio has already been staged. The next import must not be
    // blocked by a manifestless folder with orphaned audio.
    {
        SongLibrary abortLibrary(folder / "abort-user" / "Songs");
        const fs::path abortAudio = source / "abort-audio.wav";
        Write(abortAudio, firstBytes);
        locus::core::LoadedChart malformed = fresh;
        malformed.title.clear(); // invalid only at chart serialization
        const fs::path pendingFolder = abortLibrary.Root() /
            (detail::FilenamePart(abortAudio.stem().u8string()) + "-" +
             detail::HashText(fs::canonical(abortAudio)));
        fs::path attempted;
        const bool failed = !abortLibrary.CreateDifficulty(
            abortAudio, malformed, attempted, error);
        ok &= Check(failed && attempted.empty() &&
                    fs::exists(pendingFolder) &&
                    !fs::exists(pendingFolder / "abort-audio.wav") &&
                    !fs::exists(pendingFolder / "set.locus.json"),
                    "failed new Song Set save rolls back owned audio without publishing");
        malformed.title = "Recovered";
        ok &= Check(abortLibrary.CreateDifficulty(
                        abortAudio, malformed, attempted, error) &&
                    fs::exists(attempted) &&
                    fs::exists(pendingFolder / "set.locus.json") &&
                    abortLibrary.ChartCount() == 1,
                    "retry installs a valid chart into the cleaned new Song Set");
    }

    fs::path easyInstalled, hardInstalled;
    const bool importedEasy = library.Import(easy, easyInstalled, error);
    if (!importedEasy) std::cerr << "IMPORT_ERROR " << error << '\n';
    ok &= Check(importedEasy && fs::exists(easyInstalled) &&
                library.ChartCount() == 1 && library.Songs().size() == 1 &&
                library.Songs()[0].managed &&
                fs::exists(easyInstalled.parent_path() / "set.locus.json"),
                "import creates a managed Song Set with metadata");
    const bool importedHard = library.Import(hard, hardInstalled, error);
    ok &= Check(importedHard && library.Songs().size() == 1 &&
                library.ChartCount() == 2 &&
                library.Songs()[0].charts[0].metadata.title == "Easy" &&
                library.Songs()[0].charts[1].metadata.title == "Hard",
                "two difficulties are grouped in one Song Set");
    ok &= Check(HasConsistentCatalogMetadata(library),
                "import publishes standalone chart metadata for UI and search");
    const auto easyId = library.Songs()[0].charts[0].metadata.difficultyId;
    ok &= Check(library.Songs()[0].charts[0].gameplayRevision ==
                    locus::core::ChartGameplayRevision(
                        LoadPayload(library.Songs()[0].charts[0])),
                "gameplay revision is cached alongside validated difficulty");
    // Exercise actual disk materialization from the compact catalog. The exact bytes parsed must match the indexed fingerprint.
    {
        LibraryChart compact = library.Songs()[0].charts[0];
        locus::core::LoadedChart loaded;
        ok &= Check(compact.HasPayload() && compact.CopyPayload(loaded,error) &&
                    loaded.title==compact.metadata.title &&
                    loaded.difficultyId==compact.metadata.difficultyId &&
                    loaded.route.turns.size()==compact.metadata.noteCount &&
                    locus::core::ChartGameplayRevision(loaded)==compact.gameplayRevision,
                    "metadata-only record materializes exact indexed chart");
        const auto original = Read(compact.file);
        auto edited = original;
        const auto title = edited.find("Easy");
        if (title != std::string::npos) edited.replace(title,4,"EDIT");
        const auto modified = fs::last_write_time(compact.file,ec);
        Write(compact.file,edited);
        fs::last_write_time(compact.file,modified,ec);
        loaded.title="UNCHANGED";
        ok &= Check(title != std::string::npos &&
                    !compact.CopyPayload(loaded,error) &&
                    loaded.title=="UNCHANGED" && !error.empty(),
                    "same-size same-mtime external edit cannot materialize stale revision");
        Write(compact.file,original);
        fs::last_write_time(compact.file,modified,ec);
        ok &= Check(!ec && compact.CopyPayload(loaded,error),
                    "restored indexed bytes materialize again");
        auto manifestPath=compact.file.parent_path()/L"set.locus.json";
        const auto manifestBytes=Read(manifestPath);
        SongSetManifest changedTiming;
        const bool manifestLoaded=LoadSongSetManifest(manifestPath,changedTiming,error);
        if (manifestLoaded) {
            changedTiming.bpm=150.0;
            changedTiming.tempoMap.front().bpm=150.0;
        }
        const bool changed=manifestLoaded && ReplaceSongSetManifest(manifestPath,changedTiming,error);
        loaded.title="UNCHANGED";
        ok &= Check(changed && !compact.CopyPayload(loaded,error) &&
                    loaded.title=="UNCHANGED",
                    "changed authoritative tempo cannot load against old gameplay revision");
        Write(manifestPath,manifestBytes);
        ok &= Check(compact.CopyPayload(loaded,error),
                    "restored authoritative timing materializes exact revision");
    }
    const auto hardId = library.Songs()[0].charts[1].metadata.difficultyId;
    ok &= Check(locus::core::ValidDifficultyId(easyId) &&
                locus::core::ValidDifficultyId(hardId) && easyId != hardId,
                "installed difficulties receive distinct persistent identities");
    {
        // A second file with the same ID must not silently merge two
        // independent charts (and therefore their saved results).
        const auto duplicateIdFile = easyInstalled.parent_path() /
            "same-id-different-file.locus.json";
        fs::copy_file(easyInstalled, duplicateIdFile, ec);
        SongLibrary indexProbe(root);
        const bool rejected = !ec && !indexProbe.Refresh(note) &&
            note.find("same difficultyId") != std::string::npos;
        ok &= Check(rejected,
                    "scan rejects duplicate chart IDs without publishing an index");
        fs::remove(duplicateIdFile, ec);
    }
    ok &= Check(library.Revision() == 4,
                "successful imports each advance the library revision");
    const fs::path sharedAudio = library.Songs()[0].audioFile;
    SongSetManifest metadata;
    ok &= Check(LoadSongSetManifest(
                    easyInstalled.parent_path() / "set.locus.json", metadata, error) &&
                !metadata.id.empty() && metadata.title == "song" &&
                metadata.audioFile == "song.wav" &&
                metadata.bpm && *metadata.bpm == 120.0 &&
                metadata.offsetMs && *metadata.offsetMs == 0 &&
                sharedAudio == library.Songs()[0].charts[0].audioFile &&
                sharedAudio == library.Songs()[0].charts[1].audioFile &&
                Read(sharedAudio) == firstBytes,
                "manifest and all difficulties refer to one immutable audio");
    SongSetManifest unicodeMetadata = metadata;
    unicodeMetadata.title = u8"Песня — Ночной город";
    unicodeMetadata.artist = u8"Исполнитель Ёж";
    unicodeMetadata.mapper = u8"Маппер Ёж";
    unicodeMetadata.tags = {u8"электроника", u8"ночь"};
    const fs::path unicodeManifest = folder / "unicode-set.json";
    SongSetManifest unicodeLoaded;
    ok &= Check(WriteNewSongSetManifest(unicodeManifest, unicodeMetadata, error) &&
                LoadSongSetManifest(unicodeManifest, unicodeLoaded, error) &&
                unicodeLoaded.title == unicodeMetadata.title &&
                unicodeLoaded.artist == unicodeMetadata.artist &&
                unicodeLoaded.mapper == unicodeMetadata.mapper &&
                unicodeLoaded.tags == unicodeMetadata.tags,
                "Song Set metadata including pack-wide tags round-trips UTF-8");
    const std::string publishedManifest = Read(unicodeManifest);
    // Create-only must not overwrite a preexisting manifest. A stale legacy
    // .tmp must neither interfere nor be truncated by the new writer.
    Write(fs::path(unicodeManifest.wstring() + L".tmp"), "abandoned");
    unicodeMetadata.title = "Accidental replacement";
    ok &= Check(!WriteNewSongSetManifest(unicodeManifest, unicodeMetadata, error) &&
                error.find("already exists") != std::string::npos &&
                Read(unicodeManifest) == publishedManifest &&
                Read(fs::path(unicodeManifest.wstring() + L".tmp")) == "abandoned",
                "manifest create rejects collision without touching previous file");
    // Replacing invalid data must never change the previous published document.
    unicodeMetadata.id = std::string(129, 'x');
    ok &= Check(!ReplaceSongSetManifest(unicodeManifest, unicodeMetadata, error) &&
                Read(unicodeManifest) == publishedManifest,
                "manifest writer rejects an ID the loader cannot accept");
    unicodeMetadata.id = metadata.id;
    unicodeMetadata.title = u8"Обновлённый набор";
    ok &= Check(ReplaceSongSetManifest(unicodeManifest, unicodeMetadata, error) &&
                LoadSongSetManifest(unicodeManifest, unicodeLoaded, error) &&
                unicodeLoaded.title == unicodeMetadata.title &&
                unicodeLoaded.tags == unicodeMetadata.tags,
                "manifest replacement persists all fields through shared encoder");
#ifdef _WIN32
    {
        HANDLE blocked = CreateFileW(unicodeManifest.c_str(), GENERIC_READ,
                                    FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        ok &= Check(blocked != INVALID_HANDLE_VALUE,
                    "manifest can be pinned without FILE_SHARE_DELETE");
        if (blocked != INVALID_HANDLE_VALUE) {
            const std::string beforeFailure = Read(unicodeManifest);
            unicodeMetadata.title = "Blocked replacement";
            ok &= Check(!ReplaceSongSetManifest(unicodeManifest, unicodeMetadata,
                                                error) &&
                        Read(unicodeManifest) == beforeFailure,
                        "sharing violation preserves original complete manifest");
            CloseHandle(blocked);
            ok &= Check(ReplaceSongSetManifest(unicodeManifest, unicodeMetadata,
                                               error) &&
                        Read(unicodeManifest).find("Blocked replacement") !=
                            std::string::npos,
                        "manifest replace succeeds after releasing blocker");
        }
    }
#endif
    bool leakedManifestStage = false;
    for (const auto& entry : fs::directory_iterator(folder))
        if (entry.path().filename().string().find("unicode-set.json.tmp-") == 0)
            leakedManifestStage = true;
    ok &= Check(!leakedManifestStage,
                "manifest save failures clean their own staging documents");
    fs::remove(unicodeManifest, ec);
    fs::remove(fs::path(unicodeManifest.wstring() + L".tmp"), ec);
    // Menu listening start is SONG SET metadata, not a difficulty's
    // BPM/offset or note timestamps. It persists and survives ZIP export.
    const auto originalChartBytes = Read(easyInstalled);
    const auto originalHardBytes = Read(hardInstalled);
    const std::vector<std::string> packTags{u8"electronic", u8"ночь", "rhythm"};
    ok &= Check(library.SetSongMetadata(
                    0, u8"Песня — Ночной город", u8"Исполнитель Ёж",
                    u8"Маппер Ёж", packTags, error) &&
                library.Songs()[0].title == u8"Песня — Ночной город" &&
                library.Songs()[0].artist == u8"Исполнитель Ёж" &&
                library.Songs()[0].mapper == u8"Маппер Ёж" &&
                library.Songs()[0].tags == packTags &&
                LoadSongSetManifest(easyInstalled.parent_path() /
                                    "set.locus.json", metadata, error) &&
                metadata.tags == packTags &&
                metadata.mapper == u8"Маппер Ёж" &&
                Read(easyInstalled) == originalChartBytes &&
                Read(hardInstalled) == originalHardBytes &&
                library.ChartCount() == 2 &&
                library.Revision() == 5,
                "metadata edits apply to entire pack without changing charts");
    ok &= Check(!library.SetSongMetadata(
                    0, "Bad", "", "Mapper", {"dance", "DANCE"}, error) &&
                !library.SetSongMetadata(
                    0, "Bad", "", "Mapper", {""}, error) &&
                library.Songs()[0].tags == packTags &&
                Read(easyInstalled) == originalChartBytes &&
                library.Revision() == 5,
                "invalid tags rejected before any in-memory or disk mutation");
    SongSetManifest badTagManifest = metadata;
    badTagManifest.tags = {"dance", "DANCE"};
    const fs::path badTagFile = folder / "bad-pack-tags.json";
    ok &= Check(!WriteNewSongSetManifest(badTagFile, badTagManifest, error) &&
                !fs::exists(badTagFile),
                "invalid duplicate pack tags cannot be serialized");
    ok &= Check(!locus::app::ValidSongSetTags(
                    {"rhythm,broken"}) &&
                !locus::app::ValidSongSetTags(
                    std::vector<std::string>(17, "x")) &&
                locus::app::ValidSongSetTags(packTags),
                "pack tag validation enforces flat array, count and encoding");
    ok &= Check(metadata.previewStartMs == 0 &&
                library.Songs()[0].previewStartMs == 0,
                "new Song Sets default preview start to time zero");
    ok &= Check(library.SetPreviewStartMs(0, 12500, error) &&
                library.Songs()[0].previewStartMs == 12500 &&
                LoadSongSetManifest(
                    easyInstalled.parent_path() / "set.locus.json",
                    metadata, error) &&
                metadata.previewStartMs == 12500 &&
                Read(easyInstalled) == originalChartBytes &&
                library.Revision() == 6,
                "preview timestamp is atomic and does not rewrite chart timing");
    // A song's BPM/offset are single manifest properties. All difficulties
    // are reprojected in memory by exact note timestamps, and raw installed chart
    // files remain byte-for-byte unchanged (recoverable if migration fails).
    const bool timingUpdated=library.SetSongTiming(0,150.0,175,nullptr,error);
    const auto timedA=LoadPayload(library.Songs()[0].charts[0]);
    const auto timedB=LoadPayload(library.Songs()[0].charts[1]);
    ok &= Check(timingUpdated && library.Songs()[0].bpm==150.0 &&
                library.Songs()[0].offsetMs==175 &&
                timedA.route.bpm==150.0 && timedB.route.bpm==150.0 &&
                timedA.offsetSeconds==0.175 && timedB.offsetSeconds==0.175 &&
                locus::core::EventTimeMs(timedA.route,timedA.route.turns[0],
                    timedA.offsetSeconds)==1000 &&
                Read(easyInstalled)==originalChartBytes,
                "shared BPM and offset preserve note times and authored files");
    ok &= Check(HasConsistentCatalogMetadata(library),
                "tempo projection keeps UI metadata and payload in sync");
    ok &= Check(LoadSongSetManifest(
                    easyInstalled.parent_path() / "set.locus.json",
                    metadata, error) && metadata.bpm &&
                *metadata.bpm == 150.0 && metadata.offsetMs &&
                *metadata.offsetMs == 175,
                "Song Set manifest persists canonical timing once");
    SongLibrary reopened(root);
    ok &= Check(reopened.Refresh(note) &&
                reopened.Songs()[0].bpm == 150.0 &&
                reopened.Songs()[0].offsetMs == 175 &&
                reopened.Songs()[0].previewStartMs == 12500 &&
                reopened.Songs()[0].title == u8"Песня — Ночной город" &&
                reopened.Songs()[0].mapper == u8"Маппер Ёж" &&
                reopened.Songs()[0].tags == packTags,
                "preview position survives app restart / library rescan");
    ok &= Check(!library.SetPreviewStartMs(0, -100, error) &&
                library.Songs()[0].previewStartMs == 12500 &&
                !library.SetPreviewStartMs(0, kMaxSongPreviewStartMs + 1, error),
                "out of range preview time does not mutate metadata");
    // Invalid preview metadata is rejected without modifying output object.
    const fs::path badPreviewFile = folder / "bad-preview.json";
    Write(badPreviewFile,
          R"({"formatVersion":2,"setId":"one","title":"Test",)"
          R"("artist":"","audioFile":"song.wav","previewStartMs":-1})");
    SongSetManifest sentinel = metadata;
    ok &= Check(!LoadSongSetManifest(badPreviewFile, sentinel, error) &&
                sentinel.previewStartMs == 12500,
                "invalid preview position rejected atomically");
    fs::remove(badPreviewFile, ec);

    // A hostile or malformed package manifest cannot use a chart JSON as
    // its alleged shared audio file.
    const fs::path invalidMetadata = folder / "unsafe-set.json";
    Write(invalidMetadata, R"({"formatVersion":2,"setId":"test",)"
        R"("title":"Hostile","artist":"","audioFile":"set.locus.json"})");
    SongSetManifest invalidSet;
    ok &= Check(!LoadSongSetManifest(invalidMetadata, invalidSet, error),
                "manifest cannot point its audioFile at JSON");
    fs::remove(invalidMetadata, ec);

    fs::path repeated;
    ok &= Check(library.Import(easy, repeated, error) &&
                repeated == easyInstalled && library.ChartCount() == 2 &&
                library.Songs()[0].charts[0].metadata.difficultyId == easyId,
                "repeated import updates same difficulty instead of duplicating");

    // A change to source audio must NOT silently replace bytes referenced
    // by any existing difficulty.
    Write(audio, "RIFF-different-audio-never-overwrite");
    ok &= Check(!library.Import(hard, repeated, error) &&
                error.find("different audio") != std::string::npos &&
                Read(sharedAudio) == firstBytes &&
                library.ChartCount() == 2,
                "changed audio is rejected without touching existing set");
    Write(audio, firstBytes);

    const locus::core::LoadedChart copiedChart = LoadPayload(library.Songs()[0].charts[0]);
    locus::core::LoadedChart newChart = copiedChart;
    newChart.title = "Insane";
    fs::path thirdInstalled;
    ok &= Check(library.CreateDifficulty(
                    audio, newChart, thirdInstalled, error,
                    easyInstalled.parent_path()) &&
                library.ChartCount() == 3 &&
                fs::exists(thirdInstalled),
                "new difficulty is written directly to existing Song Set");

    fs::path duplicate;
    ok &= Check(library.DuplicateDifficulty(easyInstalled, duplicate, error) &&
                library.ChartCount() == 4 &&
                duplicate != easyInstalled &&
                Read(duplicate).find("Easy Copy") != std::string::npos &&
                std::none_of(library.Songs()[0].charts.begin(),
                             library.Songs()[0].charts.end(),
                             [&](const LibraryChart& candidate) {
                                 return candidate.file == duplicate &&
                                     candidate.metadata.difficultyId == easyId;
                             }),
                "duplicate difficulty uses shared audio and a separate JSON");

    const auto beforeArtworkBytes = Read(easyInstalled);
    // Image assets are independent of chart JSONs, shared by all difficulties,
    // and never permitted to overwrite their source audio.
    const fs::path sourceCover = source / "cover.png";
    const unsigned char imageHeader[] = {
        137,80,78,71,13,10,26,10,0,0,0,13,
        73,72,68,82,0,0,0,2,0,0,0,2,8,2,0,0,0,253,212,154,115,
        0,0,0,22,73,68,65,84,120,156,99,108,144,75,97,96,96,96,
        98,96,96,96,96,96,0,0,12,94,1,6,140,139,168,168,
        0,0,0,0,73,69,78,68,174,66,96,130
    };
    Write(sourceCover, std::string(
        reinterpret_cast<const char*>(imageHeader), sizeof imageHeader));
    ok &= Check(library.SetBackgroundImage(0, sourceCover, error) &&
                !library.Songs()[0].backgroundImage.empty() &&
                fs::exists(library.Songs()[0].backgroundImage) &&
                Read(library.Songs()[0].backgroundImage) == Read(sourceCover) &&
                LoadSongSetManifest(easyInstalled.parent_path() /
                                    "set.locus.json", metadata, error) &&
                !metadata.backgroundFile.empty() &&
                Read(easyInstalled) == beforeArtworkBytes,
                "background image installs without touching chart timing");
    SongLibrary artReloaded(root);
    ok &= Check(artReloaded.Refresh(note) &&
                artReloaded.Songs()[0].backgroundImage ==
                  library.Songs()[0].backgroundImage,
                "image reference persists across library scan");
    const auto activeArtwork = library.Songs()[0].backgroundImage;
    ok &= Check(!library.SetBackgroundImage(0, audio, error) &&
                library.Songs()[0].backgroundImage == activeArtwork,
                "invalid image leaves existing artwork unchanged");
    const fs::path forgedManifest = folder / "unsafe-art.json";
    Write(forgedManifest,
          R"({"formatVersion":2,"setId":"one","title":"Test",)"
          R"("artist":"","audioFile":"song.wav","previewStartMs":0,)"
          R"("bpm":120,"offsetMs":0,"tempoMap":[{"timeMs":0,"bpm":120}],)"
          R"("backgroundFile":"../secret.png"})");
    SongSetManifest safe = metadata;
    ok &= Check(!LoadSongSetManifest(forgedManifest, safe, error) &&
                safe.backgroundFile == metadata.backgroundFile,
                "manifest rejects path traversal without modifying output");
    fs::remove(forgedManifest, ec);

    // An additional BPM segment is shared by all difficulties and must
    // survive portable package export without touching authored note times.
    const std::vector<locus::core::TempoMarker> multiTempo{
        {175, 150.0}, {30000, 180.0}};
    ok &= Check(library.SetSongTempoMap(0, multiTempo, nullptr, error) &&
                library.Songs()[0].tempoMap == multiTempo &&
                LoadPayload(library.Songs()[0].charts[0]).route.tempoMap == multiTempo &&
                Read(easyInstalled) == beforeArtworkBytes,
                "multi-BPM map persists in Song Set without rewriting notes");

    // Portable .locuspack is a real ZIP file containing manifest, shared
    // audio and two independent chart JSONs. Import into a fresh library
    // must not depend on the original source path or existing installation.
    const fs::path packageFile = folder / "song.locuspack";
    const fs::path copiedRoot = folder / "second-user" / "Songs";
    const fs::path corruptFile = folder / "corrupt.locuspack";
    const fs::path traversalFile = folder / "traversal.locuspack";
    fs::path importedFolder;
    SongLibrary copiedLibrary(copiedRoot);
    // Export must never silently omit a new difficulty added after the
    // indexed catalog was built. It is a data-loss-like partial success even
    // though the archive is technically a valid ZIP.
    const fs::path unindexedChart = easyInstalled.parent_path() /
        "externally-added.locus.json";
    fs::copy_file(easyInstalled, unindexedChart, ec);
    ok &= Check(!ec &&
                !library.ExportPackage(easyInstalled.parent_path(),
                                       packageFile, error) &&
                error.find("changed on disk") != std::string::npos &&
                !fs::exists(packageFile),
                "stale Song Set index cannot export a partial ZIP package");
    fs::remove(unindexedChart, ec);
    ec.clear();
    ok &= Check(library.ExportPackage(easyInstalled.parent_path(),
                                     packageFile, error) &&
                Read(packageFile).substr(0, 4) == std::string("PK\x03\x04", 4),
                "Song Set exported as portable ZIP");
    ok &= Check(copiedLibrary.Refresh(note) &&
                copiedLibrary.ImportPackage(packageFile, importedFolder, error) &&
                copiedLibrary.Songs().size() == 1 &&
                copiedLibrary.ChartCount() == 4 &&
                fs::exists(importedFolder / "set.locus.json") &&
                copiedLibrary.Songs()[0].previewStartMs == 12500 &&
                copiedLibrary.Songs()[0].tempoMap == multiTempo &&
                copiedLibrary.Songs()[0].mapper == u8"Маппер Ёж" &&
                copiedLibrary.Songs()[0].tags == packTags &&
                fs::exists(copiedLibrary.Songs()[0].backgroundImage) &&
                Read(copiedLibrary.Songs()[0].backgroundImage) == Read(sourceCover) &&
                Read(importedFolder / "song.wav") == firstBytes,
                "ZIP import restores manifest, audio and four difficulties");
    {
        // Construct a syntactically valid package with two separate JSON
        // filenames but one repeated persistent ID; reject before publishing.
        const fs::path duplicateFile = easyInstalled.parent_path() /
            "copied-id.locus.json";
        const fs::path duplicateArchive = folder / "duplicate-identity.locuspack";
        fs::copy_file(easyInstalled, duplicateFile, ec);
        const bool built = !ec && ExportSongSetArchive(
            easyInstalled.parent_path(), {easyInstalled, duplicateFile},
            duplicateArchive, error);
        SongLibrary target(folder / "reject-duplicate-id" / "Songs");
        fs::path rejectedTarget;
        ok &= Check(built &&
                    !target.ImportPackage(duplicateArchive, rejectedTarget, error) &&
                    error.find("duplicate difficultyId") != std::string::npos &&
                    rejectedTarget.empty() && target.ChartCount() == 0,
                    "package import rejects colliding difficulty IDs before publish");
        fs::remove(duplicateFile, ec);
        fs::remove(duplicateArchive, ec);
    }
    // Portable packs carry the persistent IDs of each chart, not fresh IDs
    // derived from installed filenames or export/import destination paths.
    {
        std::set<std::string> originals, transferred;
        for (const auto& difficulty : library.Songs()[0].charts)
            originals.insert(difficulty.metadata.difficultyId);
        for (const auto& difficulty : copiedLibrary.Songs()[0].charts)
            transferred.insert(difficulty.metadata.difficultyId);
        ok &= Check(originals == transferred && originals.size() == 4 &&
                    copiedLibrary.Songs()[0].id == library.Songs()[0].id,
                    "package transfer preserves every chart ID and parent Song Set ID");
    }
    // A Song Set installed in a nested directory is equally authoritative.
    // A package update must find the installed set even when its manifest
    // is deeper than one directory below Songs/.
    {
        const fs::path nestedRoot = folder / "nested-identity-check" / "Songs";
        const fs::path nestedSet = nestedRoot / "Albums" / "Existing";
        fs::create_directories(nestedSet.parent_path(), ec);
        if (!ec) fs::copy(importedFolder, nestedSet,
                         fs::copy_options::recursive, ec);
        SongLibrary nestedLibrary(nestedRoot);
        std::string nestedNotice;
        fs::path duplicateDestination;
        const bool scanReady = !ec && nestedLibrary.Refresh(nestedNotice) &&
            nestedLibrary.ChartCount() == 4;
        const bool merged = scanReady &&
            nestedLibrary.ImportPackage(packageFile, duplicateDestination, error);
        const bool sameFolder = merged && fs::equivalent(duplicateDestination, nestedSet, ec) && !ec;
        if (!scanReady || !merged || !sameFolder)
            std::cerr << "NESTED_IMPORT_ERROR " << error << " scan=" << nestedNotice
                      << " fs=" << ec.message() << '\n';
        ok &= Check(merged && sameFolder && nestedLibrary.ChartCount() == 4,
                    "package reimport locates the existing nested Song Set");
        fs::remove_all(folder / "nested-identity-check", ec);
        ec.clear();
    }
    // Package import runs library rating before audio duration is decoded.
    // Every imported difficulty (including sparse ones) must be rated.
    ok &= Check(copiedLibrary.Songs().size() == 1 &&
                std::all_of(copiedLibrary.Songs()[0].charts.begin(),
                            copiedLibrary.Songs()[0].charts.end(),
                            [](const LibraryChart& chart) {
                                return chart.metadata.noteCount!=0 &&
                                    chart.difficulty.notes ==
                                        chart.metadata.noteCount &&
                                    chart.difficulty.overall > 0.0;
                            }),
                "imported package charts retain nonzero ratings before play");

    ok &= Check(copiedLibrary.SetBackgroundImage(0, {}, error) &&
                copiedLibrary.Songs()[0].backgroundImage.empty() &&
                !fs::exists(importedFolder / fs::u8path(metadata.backgroundFile)) &&
                copiedLibrary.ChartCount() == 4 &&
                Read(importedFolder / "song.wav") == firstBytes,
                "removing background cleans its owned copy without affecting the song");
    fs::path duplicatePackage;
    ok &= Check(copiedLibrary.ImportPackage(packageFile, duplicatePackage, error) &&
                duplicatePackage == importedFolder && copiedLibrary.ChartCount() == 4 &&
                copiedLibrary.Songs()[0].backgroundImage.empty(),
                "same package merges without duplicates or replacing local metadata");
    {
        // A renamed external chart still updates the existing persistent ID.
        const auto editedSource = folder / "edited-source";
        fs::create_directories(editedSource, ec);
        Write(editedSource / "song.wav", firstBytes);
        auto edited = LoadPayload(copiedLibrary.Songs()[0].charts[0]);
        const auto originalFile = copiedLibrary.Songs()[0].charts[0].file;
        const auto oldBytes = Read(originalFile);
        edited.title = "Updated imported difficulty";
        edited.audioPath = "song.wav";
        std::string encoded;
        const auto external = editedSource / "renamed.locus.json";
        const bool encodedOk = locus::core::SerializeChartJson(edited, encoded, error);
        if (encodedOk) Write(external, encoded);
        fs::path updated;
        ok &= Check(encodedOk && copiedLibrary.Import(external, updated, error) &&
                    updated == originalFile && copiedLibrary.ChartCount() == 4 &&
                    Read(originalFile) != oldBytes,
                    "standalone reimport resolves persistent ID across source paths");
        const auto changedBytes = Read(originalFile);
        const auto history = importedFolder / ".locus-history";
        const auto backupCount = [&]() {
            std::size_t count = 0;
            for (const auto& entry : fs::directory_iterator(history))
                if (entry.is_regular_file()) ++count;
            return count;
        };
        const auto backups = backupCount();
        ok &= Check(copiedLibrary.ImportIntoSet(external, importedFolder, updated, error) &&
                    updated == originalFile && Read(originalFile) == changedBytes &&
                    backupCount() == backups && copiedLibrary.ChartCount() == 4,
                    "identical selected-set import does not write or create backups");

        // Export only the changed chart and one new ID. Missing difficulties
        // in an update are not deletions; the installed manifest remains local.
        const auto updateArchive = folder / "partial-update.locuspack";
        const auto extraSource = easyInstalled.parent_path() / "update-new.locus.json";
        auto extra = edited;
        extra.difficultyId = "1234567890abcdef1234567890abcdef";
        extra.title = "Additional difficulty";
        std::string extraBytes;
        const bool extraOk = locus::core::SerializeChartJson(extra, extraBytes, error);
        if (extraOk) Write(extraSource, extraBytes);
        const bool partialBuilt = extraOk && ExportSongSetArchive(
            easyInstalled.parent_path(), {easyInstalled, extraSource}, updateArchive, error);
        fs::remove(extraSource, ec);
        ec.clear();
        ok &= Check(partialBuilt && copiedLibrary.ImportPackage(updateArchive, updated, error) &&
                    updated == importedFolder && copiedLibrary.ChartCount() == 5,
                    "partial package adds a new ID and retains unmentioned difficulties");
        // Restore the four-chart test fixture for subsequent corruption tests.
        for (const auto& chart : copiedLibrary.Songs()[0].charts) {
            if (chart.metadata.difficultyId == extra.difficultyId) {
                fs::remove(chart.file, ec);
                break;
            }
        }
        ok &= Check(copiedLibrary.Refresh(note) && copiedLibrary.ChartCount() == 4,
                    "refresh after removing test-only difficulty");
    }
    {
        // Simulate the pre-ID library directly from the package fixture.
        // Do not copy lock files, backups, or previously edited charts.
        const auto legacyRoot = folder / "legacy-repair" / "Songs";
        const auto legacyFolder = legacyRoot / "old-set";
        fs::create_directories(legacyFolder, ec);
        bool prepared = !ec && ExtractSongSetArchive(packageFile, legacyFolder, error);
        SongLibrary legacyLibrary(legacyRoot);
        std::vector<fs::path> oldFiles;
        if (prepared) {
            for (const auto& entry : fs::directory_iterator(legacyFolder)) {
                const auto path = entry.path();
                if (path.extension() != L".json" || path.filename() == L"set.locus.json")
                    continue;
                locus::core::LoadedChart parsed;
                std::wstring resolved;
                if (!OpenChartWithAudioPath(path.wstring(), parsed, resolved, error)) {
                    prepared = false; break;
                }
                parsed.difficultyId.clear();
                std::string encoded;
                if (!locus::core::SerializeChartJson(parsed, encoded, error)) {
                    prepared = false; break;
                }
                Write(path, encoded);
                oldFiles.push_back(path);
            }
        }
        fs::path repaired;
        const bool emptyIndex = prepared && oldFiles.size() == 4 &&
            legacyLibrary.Refresh(note) && legacyLibrary.ChartCount() == 0;
        const bool imported = emptyIndex &&
            legacyLibrary.ImportPackage(packageFile, repaired, error);
        const bool sameFolder = imported && fs::equivalent(repaired, legacyFolder, ec) && !ec;
        if (!sameFolder)
            std::cerr << "LEGACY_IMPORT_ERROR " << error << " scan=" << note
                      << " fs=" << ec.message() << '\n';
        bool sameFiles = sameFolder;
        for (const auto& path : oldFiles) {
            locus::core::LoadedChart chart;
            std::wstring resolved;
            sameFiles &= OpenChartWithAudioPath(path.wstring(), chart, resolved, error) &&
                locus::core::ValidDifficultyId(chart.difficultyId);
        }
        ok &= Check(sameFolder && sameFiles && legacyLibrary.ChartCount() == 4,
                    "reimport repairs old ID-less charts in place");
        fs::remove_all(folder / "legacy-repair", ec);
        ec.clear();
    }
    std::string zipBytes = Read(packageFile);
    const auto payload = zipBytes.find(firstBytes);
    if (payload != std::string::npos) zipBytes[payload] ^= 1;
    Write(corruptFile, zipBytes);
    ok &= Check(payload != std::string::npos &&
                !copiedLibrary.ImportPackage(corruptFile, duplicatePackage, error) &&
                copiedLibrary.ChartCount() == 4,
                "bad ZIP payload CRC is rejected without changing library");
    zipBytes = Read(packageFile);
    const auto secondHeader = zipBytes.find(std::string("PK\x03\x04", 4), 4);
    const auto filename = secondHeader == std::string::npos
        ? std::string::npos : secondHeader + 30;
    const bool validFilename = filename != std::string::npos &&
        filename + 8 <= zipBytes.size() &&
        zipBytes.substr(filename, 8) == "song.wav";
    if (validFilename) zipBytes.replace(filename, 8, "../g.wav");
    Write(traversalFile, zipBytes);
    ok &= Check(validFilename &&
                !copiedLibrary.ImportPackage(traversalFile, duplicatePackage, error) &&
                copiedLibrary.ChartCount() == 4,
                "path traversal in ZIP entry names is rejected");

    // T07: fail publication of the second prepared item after the first
    // replacement. The original chart and an independently created file
    // must survive, and retrying with a free name must succeed.
    {
        const auto batchFolder = folder / "import-rollback";
        fs::create_directories(batchFolder, ec);
        const auto first = batchFolder / "first.locus.json";
        const auto second = batchFolder / "second.locus.json";
        Write(first, "old chart");
        Write(second, "independent file");
        std::vector<detail::DifficultyImportWrite> writes{
            {first, "old chart", "updated chart", true},
            {second, {}, "new chart", false}};
        const bool rejected = !detail::PublishDifficultyImports(writes, error);
        ok &= Check(rejected && Read(first) == "old chart" &&
                    Read(second) == "independent file",
                    "failed second import publication rolls back first replacement");
        writes[1].destination = batchFolder / "available.locus.json";
        ok &= Check(detail::PublishDifficultyImports(writes, error) &&
                    Read(first) == "updated chart" &&
                    Read(writes[1].destination) == "new chart" &&
                    Read(second) == "independent file",
                    "retry after rollback publishes both intended changes");
        fs::remove_all(batchFolder, ec);
        ec.clear();
    }

    // Normal Ctrl+S equivalent: overwrite chart and verify its previous bytes
    // were backed up before atomic publication.
    const auto before = Read(easyInstalled);
    newChart.title = "Easy Updated";
    ok &= Check(SaveChartWithAudioPath(easyInstalled.wstring(),
                                       sharedAudio.wstring(), newChart, error) &&
                Read(easyInstalled) != before &&
                Read(easyInstalled).find("Easy Updated") != std::string::npos,
                "overwriting a difficulty changes only its own file");
    bool backupFound = false;
    for (fs::directory_iterator it(easyInstalled.parent_path() /
                                    ".locus-history", ec), end;
         !ec && it != end; it.increment(ec)) {
        if (Read(it->path()) == before) backupFound = true;
    }
    ok &= Check(!ec && backupFound, "previous chart can be recovered from backup");
    ok &= Check(library.Refresh(note) &&
                note.find("SKIPPED") == std::string::npos &&
                library.ChartCount() == 4,
                "library scan ignores recoverable history without phantom skipped charts");

    // Creating a NEW difficulty never turns into an overwrite if a file
    // appears at the target name between filename allocation and publication.
    // The legacy replace path still keeps its recoverable backup separately.
    {
        const std::string original = Read(easyInstalled);
        newChart.title = "Should Not Replace";
        ok &= Check(!SaveChartWithAudioPath(
                        easyInstalled.wstring(), sharedAudio.wstring(),
                        newChart, error, true, ChartWriteMode::CreateNew) &&
                    Read(easyInstalled) == original &&
                    error.find("already exists") != std::string::npos,
                    "create-new chart publication refuses an occupied filename");
        const fs::path freshFile = easyInstalled.parent_path() /
            "new-create-only.locus.json";
        ok &= Check(SaveChartWithAudioPath(
                        freshFile.wstring(), sharedAudio.wstring(),
                        newChart, error, true, ChartWriteMode::CreateNew) &&
                    fs::exists(freshFile) &&
                    Read(freshFile).find("Should Not Replace") != std::string::npos,
                    "create-new chart publishes a complete JSON exactly once");
        fs::remove(freshFile, ec);
    }

    // Writer lock must be exclusive across simultaneous editors/importers.
    {
        FolderWriteLock firstLock, secondLock;
        ok &= Check(firstLock.Acquire(easyInstalled.parent_path(), error) &&
                    !secondLock.Acquire(easyInstalled.parent_path(), error),
                    "a concurrent writer cannot acquire the same Song Set lock");
    }

    ok &= Check(library.TrashDifficulty(duplicate, error) &&
                !fs::exists(duplicate) && library.ChartCount() == 3,
                "deleting a difficulty preserves other difficulties and audio");
    Write(duplicate, "external writer must survive");
    ok &= Check(!library.RestoreLastDeleted(error) &&
                Read(duplicate) == "external writer must survive" &&
                library.ChartCount() == 3,
                "Trash recovery refuses to overwrite externally created chart");
    fs::remove(duplicate, ec);
    ok &= Check(library.RestoreLastDeleted(error) &&
                fs::exists(duplicate) && library.ChartCount() == 4,
                "recover deleted difficulty from durable local Trash");

    const fs::path managedFolder = easyInstalled.parent_path();
    ok &= Check(library.TrashSongSet(managedFolder, error) &&
                !fs::exists(managedFolder) && library.Songs().empty(),
                "deleting Song Set moves entire folder to Trash");
    fs::create_directory(managedFolder, ec);
    Write(managedFolder / "external.txt", "foreign folder must survive");
    ok &= Check(!library.RestoreLastDeleted(error) &&
                Read(managedFolder / "external.txt") == "foreign folder must survive" &&
                library.Songs().empty(),
                "Trash recovery refuses to overwrite externally created Song Set folder");
    fs::remove_all(managedFolder, ec);
    ok &= Check(library.RestoreLastDeleted(error) &&
                fs::exists(managedFolder) && library.ChartCount() == 4,
                "restore complete Song Set with all shared content");

    // Restart preserves a managed Song Set; deletion/restore did not rewrite
    // the audio or require any audio revision directories.
    SongLibrary restarted(root);
    ok &= Check(restarted.Refresh(note) && restarted.ChartCount() == 4 &&
                restarted.Songs().size() == 1 && restarted.Songs()[0].managed &&
                Read(sharedAudio) == firstBytes,
                "restart reconstructs Song Set and retains audio bytes");
    // Alphabetical order would put AAA first. Difficulty order instead
    // puts the much denser pattern last in both PLAY and EDIT -> SET.
    const fs::path denseSource = source / "dense.locus.json";
    std::string denseJson = Read(easy);
    const auto replaceDense = [&](const std::string& before,
                                  const std::string& after) {
        const auto at = denseJson.find(before);
        if (at != std::string::npos) denseJson.replace(at, before.size(), after);
    };
    replaceDense("\"title\":\"Easy\"", "\"title\":\"AAA Dense\"");
    replaceDense("\"ticksPerBeat\":1", "\"ticksPerBeat\":24");
    replaceDense("\"snapDivision\":1", "\"snapDivision\":24");
    replaceDense("\"turns\":[{\"timeMs\":1000,\"heading\":\"u\"}]",
                 "\"turns\":[{\"timeMs\":1000,\"heading\":\"u\"},"
                 "{\"timeMs\":1100,\"heading\":\"r\"},"
                 "{\"timeMs\":1200,\"heading\":\"d\"},"
                 "{\"timeMs\":1300,\"heading\":\"l\"}]");
    Write(denseSource, denseJson);
    fs::path denseInstalled;
    const bool denseAdded = library.Import(denseSource, denseInstalled, error);
    if (!denseAdded) std::cerr << "DENSE_IMPORT_ERROR " << error << '\n';
    const auto& sorted = library.Songs()[0].charts;
    ok &= Check(denseAdded && sorted.size() == 5 &&
                sorted.front().file != denseInstalled &&
                sorted.back().file == denseInstalled &&
                sorted.back().difficulty.overall >
                    sorted.front().difficulty.overall,
                "Song Set difficulties default to ascending calculated difficulty");

    // Warm scans reuse compact summaries, not resident notes.
    const auto sharedGameplayBefore = LoadPayload(library.Songs()[0].charts[0]);
    const auto sharedRevisionBefore = library.Songs()[0].charts[0].gameplayRevision;
    const std::size_t chartTotal = library.ChartCount();
    ok &= Check(library.Refresh(note) &&
                library.LastRefreshStats().parsedCharts == 0 &&
                library.LastRefreshStats().reusedCharts == chartTotal &&
                library.LastRefreshStats().calculatedDifficulties == 0 &&
                library.LastRefreshStats().peakParsedNotes == 0 &&
                library.Songs()[0].charts[0].gameplayRevision == sharedRevisionBefore,
                "warm refresh reuses summaries without materializing notes");
    ok &= Check(HasConsistentCatalogMetadata(library),
                "warm scan preserves title, difficulty ID and note count");

    ok &= Check(library.SetPreviewStartMs(0, 21000, error) &&
                library.Refresh(note) &&
                library.Songs()[0].previewStartMs == 21000 &&
                library.LastRefreshStats().calculatedDifficulties == 0,
                "manifest-only preview change does not re-rate unchanged charts");

    // External modification of one difficulty invalidates exactly that
    // cached file; the other songs/difficulties remain cached.
    const auto densePosition = std::find_if(
        library.Songs()[0].charts.begin(), library.Songs()[0].charts.end(),
        [&](const LibraryChart& item) { return item.file == denseInstalled; });
    if (densePosition == library.Songs()[0].charts.end()) {
        ok &= Check(false, "dense chart remains indexed before cache test");
    } else {
        locus::core::LoadedChart editedDense = LoadPayload(*densePosition);
        editedDense.title += " Renamed Externally";
        const bool saved = SaveChartWithAudioPath(
            denseInstalled.wstring(), densePosition->audioFile.wstring(),
            editedDense, error, true);
        const double beforeBpm = library.Songs()[0].bpm;
        const int beforeOffset = library.Songs()[0].offsetMs;
        ok &= Check(saved &&
                    !library.SetSongTiming(0, beforeBpm + 5.0,
                                           beforeOffset, nullptr, error) &&
                    library.Songs()[0].bpm == beforeBpm,
                    "stale difficulty files block shared timing publication");
        ok &= Check(library.Refresh(note) &&
                    library.LastRefreshStats().parsedCharts == 1 &&
                    library.LastRefreshStats().reusedCharts == chartTotal - 1 &&
                    library.LastRefreshStats().calculatedDifficulties == 1,
                    "editing a single chart invalidates only that chart cache");
    }

    // A writer may preserve both file size and last-write timestamp (e.g.
    // an external editor restoring metadata). Compare actual bytes before
    // reusing a cached chart or publishing Song Set-wide BPM changes.
    const auto fingerprintItem = std::find_if(
        library.Songs()[0].charts.begin(), library.Songs()[0].charts.end(),
        [&](const LibraryChart& item) { return item.file == denseInstalled; });
    if (fingerprintItem == library.Songs()[0].charts.end()) {
        ok &= Check(false, "find cached chart for same-size/same-mtime edit");
    } else {
        const auto oldTimestamp = fingerprintItem->indexedWriteTime;
        const std::string original = Read(denseInstalled);
        std::string altered = original;
        const auto name = altered.find("Renamed Externally");
        if (name == std::string::npos) {
            ok &= Check(false, "find title bytes for fingerprint regression");
        } else {
            altered[name] = 'r'; // Same length; valid but different title.
            Write(denseInstalled, altered);
            ec.clear();
            fs::last_write_time(denseInstalled, oldTimestamp, ec);
            const double bpm = library.Songs()[0].bpm;
            const int offset = library.Songs()[0].offsetMs;
            const bool rejected = !ec &&
                !library.SetSongTiming(0, bpm + 0.001, offset,
                                       nullptr, error) &&
                error.find("changed on disk") != std::string::npos;
            const bool refreshed = library.Refresh(note);
            const auto changed = std::find_if(
                library.Songs()[0].charts.begin(),
                library.Songs()[0].charts.end(),
                [&](const LibraryChart& item) {
                    return item.file == denseInstalled;
                });
            ok &= Check(rejected && refreshed &&
                        changed != library.Songs()[0].charts.end() &&
                        changed->metadata.title.find("renamed Externally") !=
                            std::string::npos &&
                        library.LastRefreshStats().parsedCharts == 1 &&
                        library.LastRefreshStats().reusedCharts == chartTotal - 1 &&
                        library.LastRefreshStats().calculatedDifficulties == 1,
                        "same-size/same-time edited chart cannot poison cache or tempo writes");
            ok &= Check(HasConsistentCatalogMetadata(library),
                        "same-size/same-mtime edit refreshes catalog metadata");
        }
    }

    // External timing changes reparse only affected files: the compact
    // catalog no longer retains authored notes.
    const fs::path finalManifest =
        easyInstalled.parent_path() / "set.locus.json";
    SongSetManifest externalTiming;
    if (LoadSongSetManifest(finalManifest, externalTiming, error) &&
        externalTiming.bpm) {
        const double savedBpm = *externalTiming.bpm;
        externalTiming.bpm = savedBpm + 1.0;
        externalTiming.tempoMap.front().bpm = *externalTiming.bpm;
        ok &= Check(ReplaceSongSetManifest(finalManifest, externalTiming, error) &&
                    library.Refresh(note) &&
                    library.Songs()[0].bpm == savedBpm + 1.0 &&
                    library.LastRefreshStats().parsedCharts == chartTotal &&
                    library.LastRefreshStats().reusedCharts == 0 &&
                    library.LastRefreshStats().calculatedDifficulties == chartTotal,
                    "external BPM reprojects affected files into compact summaries");
        ok &= Check(LoadPayload(library.Songs()[0].charts[0]).route.bpm==savedBpm+1.0 &&
                    sharedGameplayBefore.route.bpm==savedBpm,
                    "new tempo materializes without mutating owned chart payload");
        externalTiming.bpm = savedBpm;
        externalTiming.tempoMap.front().bpm = savedBpm;
        ok &= Check(ReplaceSongSetManifest(finalManifest, externalTiming, error) &&
                    library.Refresh(note) &&
                    library.Songs()[0].bpm == savedBpm,
                    "restoring shared BPM restores the cached library view");
    } else {
        ok &= Check(false, "external timing cache fixture has a valid Song Set");
    }

    // A large valid Song Set used to be silently truncated after the first
    // 8192 JSON candidates while Refresh still returned success. Exercise
    // one chart beyond that exact boundary and verify warm-cache reuse.
    {
        constexpr int kLargeChartCount = 8193;
        const fs::path scaleRoot = folder / "large-library" / "Songs";
        const fs::path scaleSet = scaleRoot / "large-set";
        fs::create_directories(scaleSet, ec);
        if (!ec) Write(scaleSet / "song.wav", firstBytes);
        SongSetManifest largeManifest;
        largeManifest.id = "stress-8193-set";
        largeManifest.title = "Large Collection";
        largeManifest.audioFile = "song.wav";
        largeManifest.bpm = 120.0;
        largeManifest.offsetMs = 0;
        largeManifest.tempoMap = {{0, 120.0}};
        const bool metadataReady = !ec && WriteNewSongSetManifest(
            scaleSet / "set.locus.json", largeManifest, error);
        if (metadataReady) {
            const std::string prefix =
                R"({"formatVersion":4,"title":"Stress","difficultyId":")";
            const std::string suffix =
                R"(","audioPath":"song.wav","bpm":120,"offsetMs":0,)"
                R"("tempoMap":[{"timeMs":0,"bpm":120}],"grid":{"width":8,)"
                R"("height":8},"start":{"x":3,"y":3,"heading":"r"},)"
                R"("previewLeadBeats":4,"turns":[{"timeMs":1000,"heading":"u"}]})";
            for (int index = 0; index < kLargeChartCount; ++index) {
                const std::string n = std::to_string(index);
                const std::string id(32 - n.size(), '0');
                Write(scaleSet / ("stress-" + n + ".locus.json"),
                      prefix + id + n + suffix);
            }
            SongLibrary largeLibrary(scaleRoot);
            std::string summary;
            const auto coldStart = std::chrono::steady_clock::now();
            const bool complete = largeLibrary.Refresh(summary) &&
                largeLibrary.ChartCount() == kLargeChartCount &&
                largeLibrary.Songs().size() == 1 &&
                largeLibrary.LastRefreshStats().parsedCharts ==
                    kLargeChartCount &&
                largeLibrary.LastRefreshStats().resolvedAudioPaths == 1 &&
                largeLibrary.LastRefreshStats().calculatedDifficulties == kLargeChartCount &&
                largeLibrary.LastRefreshStats().peakParsedNotes == 1 &&
                std::all_of(largeLibrary.Songs()[0].charts.begin(),
                    largeLibrary.Songs()[0].charts.end(),[](const LibraryChart& chart) {
                        return chart.HasPayload() && chart.metadata.noteCount==1 &&
                               chart.difficulty.notes==1;
                    }) &&
                summary.find("SKIPPED") == std::string::npos;
            const auto coldStats = largeLibrary.LastRefreshStats();
            const auto warmStart = std::chrono::steady_clock::now();
            const bool reused = complete && largeLibrary.Refresh(summary) &&
                largeLibrary.ChartCount() == kLargeChartCount &&
                largeLibrary.LastRefreshStats().reusedCharts ==
                    kLargeChartCount &&
                largeLibrary.LastRefreshStats().parsedCharts == 0 &&
                largeLibrary.LastRefreshStats().peakParsedNotes == 0 &&
                largeLibrary.LastRefreshStats().calculatedDifficulties == 0 &&
                largeLibrary.LastRefreshStats().resolvedAudioPaths == 1;
            const auto finish = std::chrono::steady_clock::now();
            const auto ms = [](auto duration) {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                    duration).count();
            };
            const auto warmStats = largeLibrary.LastRefreshStats();
            std::cout << "SONG_SET_SCAN_SCALE charts=" << kLargeChartCount
                      << " cold_ms=" << ms(warmStart - coldStart)
                      << " warm_ms=" << ms(finish - warmStart) << '\n';
            // Emit machine-readable CI metrics from the EXISTING integration
            // test, without launching an expensive second 8193-file scan.
            // Parsing and path probes are recorded alongside wall time so
            // future scanner changes can be compared without conflating work.
            if (const char* output = std::getenv("LOCUS_SCAN_BENCHMARK_OUTPUT")) {
                std::ofstream csv(fs::u8path(output), std::ios::binary);
                csv << "scenario,charts,elapsed_ms,parsed,reused,audio_resolves,ratings\n";
                csv << "cold," << kLargeChartCount << ','
                    << ms(warmStart - coldStart) << ','
                    << coldStats.parsedCharts << ',' << coldStats.reusedCharts
                    << ',' << coldStats.resolvedAudioPaths << ','
                    << coldStats.calculatedDifficulties << '\n';
                csv << "warm," << kLargeChartCount << ','
                    << ms(finish - warmStart) << ','
                    << warmStats.parsedCharts << ',' << warmStats.reusedCharts
                    << ',' << warmStats.resolvedAudioPaths << ','
                    << warmStats.calculatedDifficulties << '\n';
                ok &= Check(static_cast<bool>(csv),
                            "scan benchmark CSV saved for CI comparison");
            }
            ok &= Check(complete && reused,
                        "8193 nonempty charts index compactly with zero warm parses");
            if (reused) {
                const auto& chosen=largeLibrary.Songs()[0].charts.back();
                locus::core::LoadedChart loaded;
                ok &= Check(chosen.CopyPayload(loaded,error) &&
                            loaded.route.turns.size()==1 &&
                            loaded.difficultyId==chosen.metadata.difficultyId &&
                            locus::core::ChartGameplayRevision(loaded)==chosen.gameplayRevision,
                            "last chart of large compact catalog materializes on demand");
            }
        } else {
            ok &= Check(false, "large Song Set can create valid manifest");
        }
        fs::remove_all(folder / "large-library", ec);
        ec.clear();
    }

    fs::remove_all(folder, ec);
    if (!ok) return 1;
    std::cout << "SONG_SET_PASS shared-audio=1 copies=1 backups=1 trash=1\n";
    return 0;
}
