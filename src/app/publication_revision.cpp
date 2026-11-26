#include "app/publication_revision.h"
#include "app/chart_file.h"
#include "app/image_asset_file.h"
#include "app/song_timing.h"
#include "core/chart_revision.h"
#include <picosha2.h>
#include <array>
#include <fstream>
#include <string_view>
#include <utility>

namespace locus::app {
namespace {
namespace fs=std::filesystem;
using Hasher=picosha2::hash256_one_by_one;

bool HashFile(const fs::path& path,std::uint64_t limit,
              ContentDigest& digest,std::string& error) {
    std::error_code ec;
    const auto bytes=fs::file_size(path,ec);
    if (ec || bytes>limit || bytes==0) {
        error="Missing, empty or oversized publication asset: "+path.filename().u8string();
        return false;
    }
    std::ifstream in(path,std::ios::binary);
    if (!in) { error="Cannot open publication asset."; return false; }
    Hasher hash;
    std::array<unsigned char,65536> block{};
    std::uint64_t remaining=bytes;
    while (remaining) {
        const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(remaining,block.size()));
        if (!in.read(reinterpret_cast<char*>(block.data()),static_cast<std::streamsize>(count))) {
            error="Publication asset changed or could not be read."; return false;
        }
        hash.process(block.begin(),block.begin()+count);
        remaining-=count;
    }
    if (in.peek()!=std::char_traits<char>::eof() || in.bad()) {
        error="Publication asset changed or could not be read."; return false;
    }
    hash.finish();
    digest={bytes,picosha2::get_hash_hex_string(hash)};
    return true;
}

std::string GameplayDigest(const core::LoadedChart& chart,const ContentDigest& audio) {
    Hasher hash;
    // Domain, exact audio SHA-256 as 64 lowercase ASCII hex bytes, then
    // the canonical little-endian word stream. Fixed lengths; no ambiguity.
    constexpr std::string_view domain="LOCUS-GAMEPLAY-SHA256-V1";
    hash.process(domain.begin(),domain.end());
    hash.process(audio.sha256.begin(),audio.sha256.end());
    // Batch canonical words rather than allocating an entire encoded chart
    // or invoking the hash library once per byte.
    std::array<unsigned char,4096> block{};
    std::size_t used=0;
    core::VisitGameplayRevisionWords(chart,[&](std::uint64_t word) {
        for (unsigned i=0;i<8;++i) block[used++]=static_cast<unsigned char>(word>>(8*i));
        if (used==block.size()) { hash.process(block.begin(),block.end()); used=0; }
    });
    if (used) hash.process(block.begin(),block.begin()+used);
    hash.finish();
    return picosha2::get_hash_hex_string(hash);
}
} // namespace

bool DescribePackageRevision(const fs::path& folder,PackageRevisionContent& revision,
                             std::string& error) {
    ValidatedPackage package;
    if (!ValidateStagedPackage(folder,package,error)) return false;
    PackageRevisionContent result;
    if (!HashFile(folder/"set.locus.json",kMaxSongSetManifestBytes,result.manifest,error) ||
        !HashFile(package.audio,256ULL*1024ULL*1024ULL,result.audio,error))
        return false;
    if (!package.manifest.backgroundFile.empty()) {
        ContentDigest art;
        if (!HashFile(folder/fs::u8path(package.manifest.backgroundFile),kMaxImageAssetBytes,art,error))
            return false;
        result.background=std::move(art);
    }
    for (auto& entry : package.difficulties) {
        PublishedDifficultyContent content;
        if (!HashFile(entry.second.file,kMaxChartFileBytes,content.file,error) ||
            !ApplySongTiming(entry.second.chart,package.manifest.tempoMap,error))
            return false;
        content.gameplaySha256=GameplayDigest(entry.second.chart,result.audio);
        result.difficulties.emplace(entry.first,std::move(content));
    }
    revision=std::move(result);
    error.clear();
    return true;
}
} // namespace locus::app
