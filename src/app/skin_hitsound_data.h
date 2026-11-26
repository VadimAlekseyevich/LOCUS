#pragma once

#include "raylib.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace locus::app {

// Resource budgets, not a duration restriction. Encoded assets are retained
// in their original format. The decoded audio is only needed by the player.
inline constexpr std::size_t kMaxHitSoundBytes=32U*1024U*1024U;
inline constexpr std::size_t kMaxSkinJsonBytes=2*kMaxHitSoundBytes+65536U;
inline constexpr std::uint64_t kMaxHitSoundPcmBytes=256ULL*1024*1024;

enum class HitSoundFormat { Wav, Mp3, Ogg };
inline const char* HitSoundExtension(HitSoundFormat format) noexcept {
    switch (format) {
        case HitSoundFormat::Wav: return ".wav";
        case HitSoundFormat::Mp3: return ".mp3";
        case HitSoundFormat::Ogg: return ".ogg";
    }
    return "";
}
inline bool ParseHitSoundFormat(std::string_view value,HitSoundFormat& format) noexcept {
    if (value==".wav") format=HitSoundFormat::Wav;
    else if (value==".mp3") format=HitSoundFormat::Mp3;
    else if (value==".ogg") format=HitSoundFormat::Ogg;
    else return false;
    return true;
}
inline bool HitSoundFormatFromPath(std::wstring path,HitSoundFormat& format) {
    const auto dot=path.find_last_of(L'.');
    if (dot==std::wstring::npos) return false;
    auto suffix=path.substr(dot);
    for (auto& c : suffix) if (c>=L'A' && c<=L'Z') c+=L'a'-L'A';
    if (suffix==L".wav") format=HitSoundFormat::Wav;
    else if (suffix==L".mp3") format=HitSoundFormat::Mp3;
    else if (suffix==L".ogg") format=HitSoundFormat::Ogg;
    else return false;
    return true;
}

inline bool ValidHitSoundWave(const Wave& wave) noexcept {
    if (!IsWaveValid(wave) || !wave.frameCount || !wave.sampleRate ||
        !wave.channels || wave.channels>2 ||
        (wave.sampleSize!=8 && wave.sampleSize!=16 && wave.sampleSize!=32))
        return false;
    const auto bytes=std::uint64_t(wave.frameCount)*wave.channels*(wave.sampleSize/8);
    // Also budget for raylib's conversion to float stereo at device rate.
    const double mixedBytes=static_cast<double>(wave.frameCount)*48000.0*8.0/wave.sampleRate;
    return bytes<=kMaxHitSoundPcmBytes && mixedBytes<=kMaxHitSoundPcmBytes;
}

class HitSoundAsset final {
public:
    HitSoundFormat Format() const noexcept { return format_; }
    const std::string& Bytes() const noexcept { return bytes_; }
    Wave Decode() const {
        return LoadWaveFromMemory(HitSoundExtension(format_),
            reinterpret_cast<const unsigned char*>(bytes_.data()),
            static_cast<int>(bytes_.size()));
    }
    // Decode once at the document boundary, never on a slider change.
    // This does not require a playback device and uses the same raylib
    // WAV/MP3/Vorbis decoders as the music subsystem.
    static std::shared_ptr<const HitSoundAsset> Create(
        HitSoundFormat format,std::string bytes,std::string& error) {
        if (bytes.empty() || bytes.size()>kMaxHitSoundBytes ||
            !*HitSoundExtension(format)) {
            error="Hitsound is empty, unsupported or exceeds 32 MiB.";
            return {};
        }
        auto result=std::shared_ptr<const HitSoundAsset>(
            new HitSoundAsset(format,std::move(bytes)));
        auto wave=result->Decode();
        const bool valid=ValidHitSoundWave(wave);
        if (wave.data) UnloadWave(wave);
        if (!valid) {
            error="Cannot decode hitsound, unsupported channels or PCM memory budget exceeded.";
            return {};
        }
        error.clear();
        return result;
    }
private:
    HitSoundAsset(HitSoundFormat format,std::string bytes)
        : format_(format),bytes_(std::move(bytes)) {}
    HitSoundFormat format_;
    std::string bytes_;
};

inline std::string EncodeHitSound(std::string_view bytes) {
    constexpr char hex[]="0123456789abcdef";
    std::string out; out.reserve(bytes.size()*2);
    for (unsigned char c : bytes) { out+=hex[c>>4]; out+=hex[c&15]; }
    return out;
}
inline bool DecodeHitSound(std::string_view encoded,std::string& bytes) {
    if (encoded.empty() || encoded.size()%2 || encoded.size()>2*kMaxHitSoundBytes) return false;
    const auto digit=[](char c) {
        if (c>='0'&&c<='9') return c-'0';
        if (c>='a'&&c<='f') return c-'a'+10;
        if (c>='A'&&c<='F') return c-'A'+10;
        return -1;
    };
    std::string result; result.reserve(encoded.size()/2);
    for (std::size_t i=0;i<encoded.size();i+=2) {
        const int a=digit(encoded[i]), b=digit(encoded[i+1]);
        if (a<0||b<0) return false;
        result+=static_cast<char>(a*16+b);
    }
    bytes.swap(result); return true;
}
} // namespace locus::app
