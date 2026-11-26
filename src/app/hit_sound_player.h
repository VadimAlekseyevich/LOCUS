#pragma once
#include "app/skin.h"
#include <array>
#include <algorithm>

namespace locus::app {
// Bounded voices share one decoded PCM buffer; no per-hit decoding.
class HitSoundPlayer {
public:
    HitSoundPlayer()=default;
    HitSoundPlayer(const HitSoundPlayer&)=delete;
    HitSoundPlayer& operator=(const HitSoundPlayer&)=delete;
    ~HitSoundPlayer() { Clear(); }
    bool Configure(const Skin& skin) {
        if (source_==skin.hitSoundWav) return !source_ || IsSoundValid(sound_);
        Clear();
        source_=skin.hitSoundWav;
        if (!source_) return true;
        if (!IsAudioDeviceReady()) return false;
        auto wave=LoadWaveFromMemory(".wav",
            reinterpret_cast<const unsigned char*>(source_->data()),
            static_cast<int>(source_->size()));
        if (!IsWaveValid(wave)) return false;
        sound_=LoadSoundFromWave(wave);
        UnloadWave(wave);
        if (!IsSoundValid(sound_)) return false;
        for (auto& voice : voices_) voice=LoadSoundAlias(sound_);
        return true;
    }
    void Play(float volume) {
        if (volume<=0 || !IsSoundValid(sound_)) return;
        auto& voice=voices_[next_++%voices_.size()];
        auto* selected=IsSoundValid(voice) ? &voice : &sound_;
        if (IsSoundPlaying(*selected)) StopSound(*selected);
        SetSoundVolume(*selected,std::clamp(volume,0.0f,1.0f));
        PlaySound(*selected);
    }
    void Stop() noexcept {
        for (auto& voice : voices_) if (IsSoundValid(voice)) StopSound(voice);
        if (IsSoundValid(sound_)) StopSound(sound_);
    }
    void Clear() noexcept {
        Stop();
        for (auto& voice : voices_) {
            if (IsSoundValid(voice)) UnloadSoundAlias(voice);
            voice={};
        }
        if (IsSoundValid(sound_)) UnloadSound(sound_);
        sound_={}; source_.reset(); next_=0;
    }
private:
    std::shared_ptr<const std::string> source_;
    Sound sound_{};
    std::array<Sound,16> voices_{};
    std::size_t next_=0;
};
} // namespace locus::app
