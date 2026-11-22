#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace locus::test {

inline void WriteLe16(std::ofstream& out, std::uint16_t value) {
    for (int i = 0; i != 2; ++i)
        out.put(static_cast<char>((value >> (8 * i)) & 255));
}
inline void WriteLe32(std::ofstream& out, std::uint32_t value) {
    for (int i = 0; i != 4; ++i)
        out.put(static_cast<char>((value >> (8 * i)) & 255));
}

// A valid four-second PCM WAV, not an invented path or a mocked song file.
inline bool WriteSilentWav(const std::filesystem::path& path) {
    constexpr std::uint32_t sampleRate = 8000;
    constexpr std::uint32_t dataBytes = sampleRate * 4 * 2;
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    WriteLe32(out, 36 + dataBytes);
    out.write("WAVEfmt ", 8);
    WriteLe32(out, 16);
    WriteLe16(out, 1);
    WriteLe16(out, 1);
    WriteLe32(out, sampleRate);
    WriteLe32(out, sampleRate * 2);
    WriteLe16(out, 2);
    WriteLe16(out, 16);
    out.write("data", 4);
    WriteLe32(out, dataBytes);
    const std::vector<char> silence(dataBytes, 0);
    out.write(silence.data(), static_cast<std::streamsize>(silence.size()));
    return static_cast<bool>(out);
}

// Both difficulties deliberately share a local BPM change at song time 2 s.
inline bool WriteDifficulty(const std::filesystem::path& path,
                            const char* title, const char* turns) {
    std::ofstream out(path, std::ios::binary);
    out << "{\"formatVersion\":4,\"title\":\"" << title <<
        "\",\"audioPath\":\"song.wav\",\"bpm\":120,\"offsetMs\":0,"
        "\"tempoMap\":[{\"timeMs\":0,\"bpm\":120},"
        "{\"timeMs\":2000,\"bpm\":180}],"
        "\"ticksPerBeat\":1,\"snapDivision\":1,"
        "\"grid\":{\"width\":8,\"height\":8},"
        "\"start\":{\"x\":3,\"y\":3,\"heading\":\"r\"},"
        "\"previewLeadBeats\":4,\"turns\":" << turns << "}";
    return static_cast<bool>(out);
}

} // namespace locus::test
