#include "core/music_file.h"

#include <iostream>
#include <string_view>

using locus::core::DetectMusicFileType;
using locus::core::MusicFileType;

int main() {
    struct Test {
        std::wstring_view path;
        MusicFileType expected;
    };
    const Test cases[] = {
        {L"C:\\Music\\song.wav", MusicFileType::Wav},
        {L"D:\\Songs\\demo.MP3", MusicFileType::Mp3},
        {L"C:\\Users\\test\\musique.WaV", MusicFileType::Wav},
        {L"C:\\Users\\test\\\u043F\u0435\u0441\u043D\u044F.mp3", MusicFileType::Mp3},
        {L"music.mp3.exe", MusicFileType::Unsupported},
        {L"song.ogg", MusicFileType::Unsupported},
        {L"", MusicFileType::Unsupported}
    };
    for (const auto& test : cases) {
        if (DetectMusicFileType(test.path) != test.expected) {
            std::cerr << "LOCAL_MUSIC_FILE_FAIL\n";
            return 1;
        }
    }
    std::cout << "LOCAL_MUSIC_FILE_PASS wav=1 mp3=1 unicode=1\n";
    return 0;
}
