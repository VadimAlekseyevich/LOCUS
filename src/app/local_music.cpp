#include "app/local_music.h"

#include "core/music_file.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace locus::app {
namespace {

constexpr std::size_t kMaxLocalSongBytes = 256U * 1024U * 1024U;

std::string SafeDisplayName(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    const std::wstring name = path.substr(slash == std::wstring::npos ? 0 : slash + 1);
    std::string result;
    result.reserve(std::min(name.size(), std::size_t{38}));
    for (wchar_t c : name) {
        if (result.size() >= 38) break;
        result.push_back((c >= 32 && c <= 126) ? static_cast<char>(c) : '?');
    }
    return result.empty() ? "Local song" : result;
}

bool ReadLocalFile(const std::wstring& path, std::vector<std::uint8_t>& bytes,
                   std::string& error) {
#ifdef _WIN32
    // Read via Unicode Win32 APIs so paths with Cyrillic/other scripts work
    // independently of the machine's legacy ANSI codepage.
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "Cannot open that audio file. Check its location and permissions.";
        return false;
    }
    LARGE_INTEGER length{};
    const bool validLength = GetFileSizeEx(handle, &length) != 0 &&
                             length.QuadPart > 0 &&
                             static_cast<unsigned long long>(length.QuadPart) <=
                                 static_cast<unsigned long long>(kMaxLocalSongBytes);
    if (!validLength) {
        CloseHandle(handle);
        error = "File is empty or over 256 MiB. Try a smaller WAV/MP3.";
        return false;
    }
    try {
        bytes.resize(static_cast<std::size_t>(length.QuadPart));
    } catch (const std::bad_alloc&) {
        CloseHandle(handle);
        error = "Not enough memory to load that song.";
        return false;
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size() - offset, 1024U * 1024U));
        DWORD read = 0;
        if (!ReadFile(handle, bytes.data() + offset, request, &read, nullptr) || read == 0) {
            CloseHandle(handle);
            error = "Could not finish reading the selected music file.";
            return false;
        }
        offset += read;
    }
    CloseHandle(handle);
    return true;
#else
    (void)path;
    (void)bytes;
    error = "Local music playback requires Windows.";
    return false;
#endif
}

} // namespace

LocalMusic::~LocalMusic() {
    Shutdown();
}

bool LocalMusic::LoadFromFile(const std::wstring& path) {
    const core::MusicFileType type = core::DetectMusicFileType(path);
    if (type == core::MusicFileType::Unsupported) {
        error_ = "Unsupported audio format. Choose a WAV or MP3.";
        return false;
    }
    if (!IsAudioDeviceReady()) {
        error_ = "No audio output device is ready. Check Windows sound settings.";
        return false;
    }
    std::vector<std::uint8_t> newBytes;
    if (!ReadLocalFile(path, newBytes, error_)) return false;

    const char* format = (type == core::MusicFileType::Wav) ? ".wav" : ".mp3";
    Music candidate = LoadMusicStreamFromMemory(
        format, newBytes.data(), static_cast<int>(newBytes.size()));
    if (!IsMusicValid(candidate)) {
        error_ = "Audio decoder could not open this file. Try a different WAV or MP3.";
        return false;
    }
    const double length = static_cast<double>(GetMusicTimeLength(candidate));
    if (!std::isfinite(length) || length <= 0.0) {
        UnloadMusicStream(candidate);
        error_ = "This audio file has no valid playback duration.";
        return false;
    }
    candidate.looping = false;
    SetMusicVolume(candidate, 0.70f);

    // Only replace the previous working song after successful decoding.
    Shutdown();
    fileBytes_.swap(newBytes);
    music_ = candidate;
    durationSeconds_ = length;
    displayName_ = SafeDisplayName(path);
    ready_ = true;
    error_.clear();
    std::cout << "LOCAL_MUSIC_READY duration=" << durationSeconds_ << "s\n";
    return true;
}

void LocalMusic::Start(double monotonicSeconds) {
    if (!ready_) return;
    Stop();
    finished_ = false;
    PlayMusicStream(music_);
    clock_.Start(monotonicSeconds);
    playing_ = true;
}

void LocalMusic::Pause(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    PauseMusicStream(music_);
    clock_.Pause(monotonicSeconds);
    paused_ = true;
}

void LocalMusic::Resume(double monotonicSeconds) {
    if (!playing_ || !paused_) return;
    ResumeMusicStream(music_);
    clock_.Resume(monotonicSeconds);
    paused_ = false;
}

void LocalMusic::Stop() {
    if (ready_ && playing_) StopMusicStream(music_);
    playing_ = false;
    paused_ = false;
    clock_.Stop();
}

void LocalMusic::Update(double monotonicSeconds) {
    if (!playing_ || paused_) return;
    UpdateMusicStream(music_);
    if (clock_.SecondsAt(monotonicSeconds) >= durationSeconds_) {
        Stop();
        finished_ = true;
    }
}

void LocalMusic::Shutdown() {
    Stop();
    if (ready_) {
        UnloadMusicStream(music_);
        ready_ = false;
    }
    music_ = Music{};
    fileBytes_.clear();
    displayName_.clear();
    durationSeconds_ = 0.0;
    finished_ = false;
}

double LocalMusic::SongSeconds(double monotonicSeconds) const noexcept {
    return finished_ ? durationSeconds_ : clock_.SecondsAt(monotonicSeconds);
}

} // namespace locus::app
