#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace locus::app {
inline constexpr std::size_t kMaxHitSoundBytes=400000;
inline constexpr std::size_t kMaxSkinJsonBytes=2*kMaxHitSoundBytes+65536;
// Bound and validate untrusted PCM before handing it to the audio decoder.
inline bool ValidHitSoundWav(std::string_view wav) noexcept {
    if (wav.size()<44 || wav.size()>kMaxHitSoundBytes ||
        wav.substr(0,4)!="RIFF" || wav.substr(8,4)!="WAVE") return false;
    const auto read=[&](std::size_t at,unsigned count) {
        std::uint32_t value=0;
        for (unsigned i=0;i<count;++i)
            value |= std::uint32_t(static_cast<unsigned char>(wav[at+i])) << (8*i);
        return value;
    };
    if (read(4,4)!=wav.size()-8) return false;
    std::uint32_t rate=0,channels=0,block=0,data=0;
    bool format=false,samples=false;
    for (std::size_t at=12;at<wav.size();) {
        if (wav.size()-at<8) return false;
        const auto type=wav.substr(at,4);
        const auto size=read(at+4,4);
        at+=8;
        if (size>wav.size()-at) return false;
        if (type=="fmt ") {
            if (format || size<16 || read(at,2)!=1) return false;
            channels=read(at+2,2); rate=read(at+4,4); block=read(at+12,2);
            if ((channels!=1 && channels!=2) || rate<8000 || rate>48000 ||
                read(at+14,2)!=16 || block!=channels*2 || read(at+8,4)!=rate*block)
                return false;
            format=true;
        } else if (type=="data") {
            if (samples || !format) return false;
            data=size; samples=true;
        }
        at+=size;
        if (size%2) { if (at==wav.size()) return false; ++at; }
    }
    return format && samples && data>0 && data%block==0 && data<=2*rate*block;
}
inline std::string EncodeHitSound(std::string_view bytes) {
    constexpr char hex[]="0123456789abcdef";
    std::string out; out.reserve(bytes.size()*2);
    for (unsigned char c : bytes) { out+=hex[c>>4]; out+=hex[c&15]; }
    return out;
}
inline bool DecodeHitSound(std::string_view encoded,std::string& bytes) {
    if (encoded.size()%2 || encoded.size()>2*kMaxHitSoundBytes) return false;
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
    if (!ValidHitSoundWav(result)) return false;
    bytes.swap(result); return true;
}
} // namespace locus::app
