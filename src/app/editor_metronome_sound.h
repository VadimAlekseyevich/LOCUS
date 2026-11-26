#pragma once
#include "core/metronome.h"
#include "raylib.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace locus::app {
class EditorMetronomeSound {
public:
    EditorMetronomeSound()=default;
    EditorMetronomeSound(const EditorMetronomeSound&)=delete;
    EditorMetronomeSound& operator=(const EditorMetronomeSound&)=delete;
    ~EditorMetronomeSound() { Clear(); }
    void Initialize() {
        Clear();
        if (!IsAudioDeviceReady()) return;
        beat_=Make(1100.0);
        downbeat_=Make(1760.0);
    }
    void Play(core::MetronomeClick click,float volume) {
        if (click==core::MetronomeClick::None || volume<=0) return;
        auto sound=click==core::MetronomeClick::Downbeat?downbeat_:beat_;
        if (!IsSoundValid(sound)) return;
        SetSoundVolume(sound,std::clamp(volume,0.0f,1.0f));
        PlaySound(sound);
    }
    void Stop() {
        if (IsSoundValid(beat_)) StopSound(beat_);
        if (IsSoundValid(downbeat_)) StopSound(downbeat_);
    }
    void Clear() {
        Stop();
        if (IsSoundValid(beat_)) UnloadSound(beat_);
        if (IsSoundValid(downbeat_)) UnloadSound(downbeat_);
        beat_={}; downbeat_={};
    }
private:
    static Sound Make(double frequency) {
        constexpr unsigned rate=48000,frames=2400;
        std::array<short,frames> pcm{};
        for (unsigned i=0;i<frames;++i) {
            const double t=static_cast<double>(i)/rate;
            const double attack=std::min(1.0,t/0.001);
            const double phase=6.283185307179586*frequency*t;
            // A brighter transient cuts through music without clipping.
            pcm[i]=static_cast<short>(29000*attack*std::exp(-t*95)*
                (0.78*std::sin(phase)+0.22*std::sin(2*phase)));
        }
        // LoadSoundFromWave copies the PCM; the stack buffer is not owned
        // by raylib and must not be passed to UnloadWave.
        return LoadSoundFromWave(Wave{frames,rate,16,1,pcm.data()});
    }
    Sound beat_{},downbeat_{};
};
} // namespace locus::app
